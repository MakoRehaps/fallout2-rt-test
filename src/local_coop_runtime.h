#ifndef LOCAL_COOP_RUNTIME_H
#define LOCAL_COOP_RUNTIME_H

#include <SDL.h>

#include <algorithm>
#include <array>

#include "combat.h"
#include "critter.h"
#include "debug.h"
#include "game.h"
#include "input.h"
#include "item.h"
#include "kb.h"
#include "local_coop.h"
#include "local_coop_ai_realtime.h"
#include "local_coop_danger.h"
#include "local_coop_focus.h"
#include "object.h"
#include "perk.h"
#include "proto_types.h"
#include "trait.h"
#include "tile.h"
#include "unified_campaign_carryover.h"
#include "unified_campaign_transition.h"

namespace fallout {

inline constexpr Uint32 kLocalCoopInitialSchedulerHeartbeatMs = 200;
inline constexpr Uint32 kLocalCoopBookkeepingHeartbeatMs = 1000;

struct LocalCoopRuntimeSlot {
    Uint32 nextPrimaryAttackTick = 0;
    Uint32 nextSecondaryAttackTick = 0;
    Uint32 nextReloadTick = 0;
    Uint32 nextApRegenTick = 0;
    int actionPointsHundredths = -1;
    int actionPointsActorId = -1;
    int apRegenDelayTicks = 0;
    bool primaryWasDown = false;
    bool reloadWasDown = false;
    bool secondaryWasDown = false;
    bool swapWasDown = false;
    bool postgameSwitchWasDown = false;
    bool queuedAttackPending = false;
    bool queuedAttackSecondary = false;
    int queuedAttackTargetId = -1;
    Object* aimTarget = nullptr;
};

inline std::array<LocalCoopRuntimeSlot, kLocalCoopMaxPlayers> gLocalCoopRuntimeSlots;
inline bool gLocalCoopRealtimeCombatActive = false;
inline bool gLocalCoopRuntimeTickerInstalled = false;
inline bool gLocalCoopRuntimeInsideTick = false;
inline bool gLocalCoopLegacyYieldQueued = false;
inline Uint32 gLocalCoopNextLegacyYieldTick = 0;

inline bool localCoopTickReached(Uint32 now, Uint32 target)
{
    return static_cast<Sint32>(now - target) >= 0;
}

inline int localCoopGetHitMode(LocalCoopPlayer& player, bool secondary)
{
    return secondary ? localCoopGetSecondaryHitMode(player) : localCoopGetPrimaryHitMode(player);
}

inline Uint32 localCoopGetAttackCooldown(Object* actor, int hitMode)
{
    int actionPointCost = weaponGetActionPointCost(actor, hitMode, false);
    if (actionPointCost <= 0) {
        actionPointCost = 1;
    }

    int cooldown = actionPointCost * 120;
    cooldown = std::max(240, std::min(cooldown, 1200));
    return static_cast<Uint32>(cooldown);
}

inline int localCoopActionPointMaximumHundredths(Object* actor)
{
    if (actor == nullptr) {
        return 0;
    }

    return std::max(100, critterGetStat(actor, STAT_MAXIMUM_ACTION_POINTS) * 100);
}

inline int localCoopPlayerApRegenAmountHundredths(Object* actor)
{
    if (actor == nullptr) {
        return 0;
    }

    // Port of ap_regen.fos: (360 + 69 * (AG + 2 * Action Boy
    // - 3 * Bruiser)) / 2 every 500 ms. Fallout companions do not own the
    // player's selected traits, so Bruiser is applied only to gDude.
    int agility = critterGetStat(actor, STAT_AGILITY);
    int actionBoy = perkGetRank(actor, PERK_ACTION_BOY);
    int bruiser = actor == gDude && traitIsSelected(TRAIT_BRUISER) ? 1 : 0;
    return std::max(1, (360 + 69 * (agility + 2 * actionBoy - 3 * bruiser)) / 2);
}

inline void localCoopUpdatePlayerActionPoints(LocalCoopPlayer& player, Uint32 now)
{
    Object* actor = player.actor;
    LocalCoopRuntimeSlot& runtime = gLocalCoopRuntimeSlots[player.slot];
    if (actor == nullptr) {
        runtime.actionPointsHundredths = -1;
        runtime.actionPointsActorId = -1;
        runtime.nextApRegenTick = 0;
        runtime.apRegenDelayTicks = 0;
        return;
    }

    int maximum = localCoopActionPointMaximumHundredths(actor);
    if (runtime.actionPointsActorId != actor->id || runtime.actionPointsHundredths < 0) {
        runtime.actionPointsActorId = actor->id;
        runtime.actionPointsHundredths = maximum;
        runtime.nextApRegenTick = now + 500;
        runtime.apRegenDelayTicks = 0;
        return;
    }

    runtime.actionPointsHundredths = std::min(runtime.actionPointsHundredths, maximum);
    int processedTicks = 0;
    while (localCoopTickReached(now, runtime.nextApRegenTick) && processedTicks < 20) {
        if (runtime.apRegenDelayTicks > 0) {
            runtime.apRegenDelayTicks--;
        } else if (runtime.actionPointsHundredths < maximum) {
            runtime.actionPointsHundredths = std::min(maximum,
                runtime.actionPointsHundredths + localCoopPlayerApRegenAmountHundredths(actor));
        }
        runtime.nextApRegenTick += 500;
        processedTicks++;
    }

    // Do not award an unbounded offline/modal catch-up burst.
    if (processedTicks == 20 && localCoopTickReached(now, runtime.nextApRegenTick)) {
        runtime.nextApRegenTick = now + 500;
    }
}

inline int localCoopActionPointCostHundredths(Object* actor, int hitMode)
{
    int cost = weaponGetActionPointCost(actor, hitMode, false);
    return std::max(1, cost) * 100;
}

inline bool localCoopHasActionPoints(LocalCoopPlayer& player, int costHundredths, Uint32 now)
{
    localCoopUpdatePlayerActionPoints(player, now);
    return gLocalCoopRuntimeSlots[player.slot].actionPointsHundredths >= costHundredths;
}

inline void localCoopSpendActionPoints(LocalCoopPlayer& player, int costHundredths)
{
    LocalCoopRuntimeSlot& runtime = gLocalCoopRuntimeSlots[player.slot];
    runtime.actionPointsHundredths = std::max(0, runtime.actionPointsHundredths - costHundredths);
    runtime.apRegenDelayTicks = std::max(runtime.apRegenDelayTicks, 1);
}

inline bool localCoopReloadFromSharedPool(LocalCoopPlayer& player)
{
    Object* actor = player.actor;
    if (actor == nullptr) {
        return false;
    }

    Object* weapon = localCoopGetActiveItem(player);
    if (weapon == nullptr || itemGetType(weapon) != ITEM_TYPE_WEAPON) {
        return false;
    }

    int hand = localCoopGetActiveHand(player);
    int reloadHitMode = hand == HAND_LEFT ? HIT_MODE_LEFT_WEAPON_RELOAD : HIT_MODE_RIGHT_WEAPON_RELOAD;
    int reloadCost = localCoopActionPointCostHundredths(actor, reloadHitMode);
    Uint32 now = SDL_GetTicks();
    if (!localCoopHasActionPoints(player, reloadCost, now)) {
        debugPrint("[COOP RELOAD] slot=%d blocked-ap current=%d cost=%d\n",
            player.slot,
            gLocalCoopRuntimeSlots[player.slot].actionPointsHundredths,
            reloadCost);
        return false;
    }

    Object* sharedOwner = localCoopGetSharedInventoryOwner();
    if (sharedOwner == nullptr) {
        return false;
    }

    if (actor == sharedOwner) {
        int rc = weaponAttemptReload(actor, weapon);
        if (rc != -1) {
            localCoopSpendActionPoints(player, reloadCost);
        }
        debugPrint("[COOP RELOAD] slot=%d pid=%d rc=%d ap=%d\n",
            player.slot,
            weapon->pid,
            rc,
            gLocalCoopRuntimeSlots[player.slot].actionPointsHundredths);
        return rc != -1;
    }

    Inventory& inventory = sharedOwner->data.inventory;
    Object* compatibleAmmo = nullptr;
    for (int index = 0; index < inventory.length; index++) {
        Object* item = inventory.items[index].item;
        if (item != nullptr
            && itemGetType(item) == ITEM_TYPE_AMMO
            && weaponCanBeReloadedWith(weapon, item)) {
            compatibleAmmo = item;
            break;
        }
    }

    if (compatibleAmmo == nullptr) {
        debugPrint("[COOP RELOAD] slot=%d pid=%d no-compatible-ammo\n", player.slot, weapon->pid);
        return false;
    }

    if (itemMoveForce(sharedOwner, actor, compatibleAmmo, 1) != 0) {
        debugPrint("[COOP RELOAD] slot=%d pid=%d ammo-move-failed\n", player.slot, weapon->pid);
        return false;
    }

    int reloadRc = weaponAttemptReload(actor, weapon);
    bool reloaded = reloadRc != -1;
    if (reloaded) {
        localCoopSpendActionPoints(player, reloadCost);
    }
    localCoopSweepSharedInventory();
    debugPrint("[COOP RELOAD] slot=%d pid=%d rc=%d ap=%d\n",
        player.slot,
        weapon->pid,
        reloadRc,
        gLocalCoopRuntimeSlots[player.slot].actionPointsHundredths);
    return reloaded;
}

inline void localCoopQueueAttack(LocalCoopPlayer& player, Object* target, bool secondary)
{
    if (target == nullptr) {
        return;
    }

    LocalCoopRuntimeSlot& runtime = gLocalCoopRuntimeSlots[player.slot];
    runtime.queuedAttackPending = true;
    runtime.queuedAttackSecondary = secondary;
    runtime.queuedAttackTargetId = target->id;
    runtime.aimTarget = target;
}

inline void localCoopClearQueuedAttack(LocalCoopRuntimeSlot& runtime)
{
    runtime.queuedAttackPending = false;
    runtime.queuedAttackSecondary = false;
    runtime.queuedAttackTargetId = -1;
}

// One attack backend for every human input path: controller RT/RB, controller A
// context attacks, and P1 mouse left-click all land here. There is no separate
// Fallout combat-mode attack path in the co-op world.
inline bool localCoopPerformAttackAgainst(LocalCoopPlayer& player, Object* target, bool secondary)
{
    Object* actor = player.actor;
    if (actor == nullptr || target == nullptr || !localCoopFocusPointerIsLive(target)) {
        debugPrint("[COOP ATTACK] slot=%d invalid-target\n", player.slot);
        return false;
    }

    if (!player.humanOwned
        || player.uiMode != LocalCoopUiMode::World
        || actor == target
        || localCoopActorIsHumanOwned(target)
        || PID_TYPE(target->pid) != OBJ_TYPE_CRITTER
        || (target->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0
        || target->data.critter.combat.team == actor->data.critter.combat.team) {
        debugPrint("[COOP ATTACK] slot=%d target-rejected\n", player.slot);
        return false;
    }

    LocalCoopRuntimeSlot& runtime = gLocalCoopRuntimeSlots[player.slot];
    runtime.aimTarget = target;

    if (animationIsBusy(actor)) {
        // Movement and other insignificant animation may be interrupted so the
        // player stays responsive. Fallout marks attack/hit/death sequences as
        // prioritized, and reg_anim_clear correctly refuses those with -2. In
        // realtime mode that is a temporary lock, not a failed attack: remember
        // the request and fire it as soon as the prioritized sequence completes.
        int clearRc = reg_anim_clear(actor);
        if (clearRc == -2) {
            localCoopQueueAttack(player, target, secondary);
            debugPrint("[COOP ATTACK] slot=%d queued-behind-prioritized-actor-animation targetId=%d secondary=%d\n",
                player.slot,
                target->id,
                secondary ? 1 : 0);
            return false;
        }

        if (clearRc < 0 && animationIsBusy(actor)) {
            localCoopQueueAttack(player, target, secondary);
            debugPrint("[COOP ATTACK] slot=%d queued-behind-actor-animation targetId=%d clearRc=%d\n",
                player.slot,
                target->id,
                clearRc);
            return false;
        }
    }

    // Keep the actor's weapon code/FID synchronized with the same active hand
    // that supplies hit mode/range/ammo to combat.
    localCoopSyncActiveHandVisual(player.slot);

    int hand = localCoopGetActiveHand(player);
    Object* activeItem = localCoopGetActiveItem(player);
    int hitMode = localCoopGetHitMode(player, secondary);
    int attackCost = localCoopActionPointCostHundredths(actor, hitMode);
    Uint32 now = SDL_GetTicks();
    if (!localCoopHasActionPoints(player, attackCost, now)) {
        debugPrint("[COOP ATTACK] slot=%d blocked-ap current=%d cost=%d hitMode=%d\n",
            player.slot,
            runtime.actionPointsHundredths,
            attackCost,
            hitMode);
        return false;
    }

    int attackAnimation = critterGetAnimationForHitMode(actor, hitMode);
    int weaponAnimationCode = activeItem != nullptr && itemGetType(activeItem) == ITEM_TYPE_WEAPON
        ? weaponGetAnimationCode(activeItem)
        : 0;

    debugPrint("[COOP ATTACK] begin slot=%d secondary=%d actorId=%d targetId=%d hand=%d itemPid=%d hitMode=%d attackAnim=%d weaponAnim=%d actorFid=%08X distance=%d\n",
        player.slot,
        secondary ? 1 : 0,
        actor->id,
        target->id,
        hand,
        activeItem != nullptr ? activeItem->pid : -1,
        hitMode,
        attackAnimation,
        weaponAnimationCode,
        actor->fid,
        objectGetDistanceBetween(actor, target));

    int savedActionPoints = actor->data.critter.combat.ap;
    actor->data.critter.combat.ap = 9999;

    int badShot = _combat_check_bad_shot(actor, target, hitMode, false);
    if (badShot == COMBAT_BAD_SHOT_NO_AMMO) {
        debugPrint("[COOP ATTACK] no-ammo slot=%d itemPid=%d hitMode=%d; trying realtime reload\n",
            player.slot,
            activeItem != nullptr ? activeItem->pid : -1,
            hitMode);

        if (localCoopReloadFromSharedPool(player)) {
            // Reload can change item state; resync before retrying the same shot.
            localCoopSyncActiveHandVisual(player.slot);
            badShot = _combat_check_bad_shot(actor, target, hitMode, false);
        }
    }

    if (badShot != COMBAT_BAD_SHOT_OK) {
        debugPrint("[COOP ATTACK] bad-shot slot=%d code=%d itemPid=%d hitMode=%d\n",
            player.slot,
            badShot,
            activeItem != nullptr ? activeItem->pid : -1,
            hitMode);
        actor->data.critter.combat.ap = savedActionPoints;
        return false;
    }

    // _combat_attack is retained strictly as Fallout's hit/ammo/crit/damage and
    // attack-animation service. We never call _combat() or enter its turn loop.
    int rc = _combat_attack(actor, target, hitMode, HIT_LOCATION_UNCALLED);
    actor->data.critter.combat.ap = savedActionPoints;
    if (rc != 0) {
        debugPrint("[COOP ATTACK] sequence-failed slot=%d rc=%d hand=%d itemPid=%d hitMode=%d attackAnim=%d actorFid=%08X\n",
            player.slot,
            rc,
            hand,
            activeItem != nullptr ? activeItem->pid : -1,
            hitMode,
            attackAnimation,
            actor->fid);
        return false;
    }

    localCoopSpendActionPoints(player, attackCost);
    debugPrint("[COOP ATTACK] sequence-ok slot=%d hand=%d itemPid=%d hitMode=%d ap=%d cost=%d\n",
        player.slot,
        hand,
        activeItem != nullptr ? activeItem->pid : -1,
        hitMode,
        runtime.actionPointsHundredths,
        attackCost);

    localCoopClearQueuedAttack(runtime);
    localCoopRealtimeAiEngageHostile(target, actor);
    localCoopDangerTouch();
    gLocalCoopRealtimeCombatActive = true;

    if (actor == gDude && gInterfaceBarWindow != -1) {
        interfaceRenderActionPoints(-1, -1);
    }
    return true;
}

inline bool localCoopPerformAttack(LocalCoopPlayer& player, bool secondary)
{
    LocalCoopRuntimeSlot& runtime = gLocalCoopRuntimeSlots[player.slot];
    Object* target = localCoopFocusFindEnemy(player);
    runtime.aimTarget = target;
    if (target == nullptr) {
        debugPrint("[COOP ATTACK] slot=%d no-target aim=(%d,%d)\n", player.slot, player.aimX, player.aimY);
        return false;
    }

    return localCoopPerformAttackAgainst(player, target, secondary);
}

inline void localCoopProcessQueuedAttacks(Uint32 now)
{
    for (LocalCoopPlayer& player : gLocalCoopPlayers) {
        LocalCoopRuntimeSlot& runtime = gLocalCoopRuntimeSlots[player.slot];
        if (!runtime.queuedAttackPending) {
            continue;
        }

        Object* actor = player.actor;
        if (!player.humanOwned
            || actor == nullptr
            || player.uiMode != LocalCoopUiMode::World
            || (actor->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0) {
            localCoopClearQueuedAttack(runtime);
            continue;
        }

        Uint32 nextAttackTick = runtime.queuedAttackSecondary
            ? runtime.nextSecondaryAttackTick
            : runtime.nextPrimaryAttackTick;
        if (!localCoopTickReached(now, nextAttackTick)) {
            continue;
        }

        Object* target = runtime.queuedAttackTargetId != -1
            ? objectFindById(runtime.queuedAttackTargetId)
            : nullptr;
        if (target == nullptr || !localCoopFocusPointerIsLive(target)) {
            localCoopClearQueuedAttack(runtime);
            continue;
        }

        bool secondary = runtime.queuedAttackSecondary;
        int hitMode = localCoopGetHitMode(player, secondary);

        // Clear before dispatch. If a prioritized animation is still active,
        // localCoopPerformAttackAgainst re-queues the request. Any other failure
        // is intentionally dropped so stale bad-shot requests cannot loop forever.
        localCoopClearQueuedAttack(runtime);
        if (localCoopPerformAttackAgainst(player, target, secondary)) {
            Uint32 cooldown = localCoopGetAttackCooldown(actor, hitMode);
            if (secondary) {
                runtime.nextSecondaryAttackTick = now + cooldown;
            } else {
                runtime.nextPrimaryAttackTick = now + cooldown;
            }
        } else if (!runtime.queuedAttackPending) {
            if (secondary) {
                runtime.nextSecondaryAttackTick = now + 100;
            } else {
                runtime.nextPrimaryAttackTick = now + 100;
            }
        }
    }
}

inline void localCoopProcessPostgameWorldSwitch()
{
    if (localCoopDangerBlocksMapExit() || !unifiedCampaignBothGamesCompleted()) {
        return;
    }

    LocalCoopPlayer& player = gLocalCoopPlayers[0];
    if (!player.connected || player.controller == nullptr || player.uiMode != LocalCoopUiMode::World) {
        return;
    }

    LocalCoopRuntimeSlot& runtime = gLocalCoopRuntimeSlots[0];
    bool backDown = SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_BACK) != 0;
    bool startDown = SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_START) != 0;
    bool switchDown = backDown && startDown;

    if (switchDown && !runtime.postgameSwitchWasDown) {
        UnifiedGameId destination = unifiedCampaignGetActiveGame() == UnifiedGameId::Fallout1
            ? UnifiedGameId::Fallout2
            : UnifiedGameId::Fallout1;

        if (unifiedCampaignCapturePlayerCarryover(destination)
            && unifiedCampaignRequestPostgameWorldSwitchAndResume()) {
            _game_user_wants_to_quit = 2;
        } else {
            unifiedCampaignClearCarryover();
            unifiedCampaignCancelPostgameResume();
            unifiedCampaignCancelAutoStartNewGame();
        }
    }

    runtime.postgameSwitchWasDown = switchDown;
}

inline void localCoopProcessCombatInput()
{
    Uint32 now = SDL_GetTicks();

    // A mouse click or context attack that lands during a prioritized Fallout
    // attack/hit sequence must not be lost. Service those queued one-shot inputs
    // first; held RT input below then naturally observes the updated cooldown.
    localCoopProcessQueuedAttacks(now);

    for (LocalCoopPlayer& player : gLocalCoopPlayers) {
        bool hasController = player.connected && player.controller != nullptr;
        if (!player.humanOwned
            || player.actor == nullptr
            || player.uiMode != LocalCoopUiMode::World
            || (player.actor->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0
            || (!hasController && player.slot != 0)) {
            continue;
        }

        LocalCoopRuntimeSlot& runtime = gLocalCoopRuntimeSlots[player.slot];
        localCoopUpdatePlayerActionPoints(player, now);
        runtime.aimTarget = localCoopFocusFindEnemy(player);

        bool primaryDown = false;
        bool secondaryDown = false;
        bool reloadDown = false;
        bool swapDown = false;

        if (hasController) {
            int rightTrigger = SDL_GameControllerGetAxis(player.controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
            primaryDown = rightTrigger > 12000;
            secondaryDown = SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) != 0;
            reloadDown = SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_X) != 0;
            swapDown = SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_Y) != 0;
        }

        if (player.slot == 0) {
            swapDown = swapDown || gPressedPhysicalKeys[SDL_SCANCODE_TAB];
        }

        if (swapDown && !runtime.swapWasDown) {
            localCoopSwapActiveHand(player.slot, false);
            runtime.aimTarget = localCoopFocusFindEnemy(player);
        }

        if (reloadDown && !runtime.reloadWasDown && localCoopTickReached(now, runtime.nextReloadTick)) {
            localCoopReloadFromSharedPool(player);
            runtime.nextReloadTick = now + 400;
        }

        if (primaryDown && localCoopTickReached(now, runtime.nextPrimaryAttackTick)) {
            int hitMode = localCoopGetHitMode(player, false);
            if (localCoopPerformAttack(player, false)) {
                runtime.nextPrimaryAttackTick = now + localCoopGetAttackCooldown(player.actor, hitMode);
            } else {
                runtime.nextPrimaryAttackTick = now + 100;
            }
        }

        if (secondaryDown
            && !runtime.secondaryWasDown
            && localCoopTickReached(now, runtime.nextSecondaryAttackTick)) {
            int hitMode = localCoopGetHitMode(player, true);
            if (localCoopPerformAttack(player, true)) {
                runtime.nextSecondaryAttackTick = now + localCoopGetAttackCooldown(player.actor, hitMode);
            } else {
                runtime.nextSecondaryAttackTick = now + 150;
            }
        }

        runtime.primaryWasDown = primaryDown;
        runtime.reloadWasDown = reloadDown;
        runtime.secondaryWasDown = secondaryDown;
        runtime.swapWasDown = swapDown;
    }
}

inline void localCoopUpdateSharedCamera()
{
    if (gDude == nullptr || !tileIsValid(gDude->tile)) {
        return;
    }

    int elevation = gDude->elevation;
    long long totalX = 0;
    long long totalY = 0;
    int count = 0;

    for (const LocalCoopPlayer& player : gLocalCoopPlayers) {
        Object* actor = player.actor;
        if (!player.humanOwned
            || actor == nullptr
            || actor->elevation != elevation
            || !tileIsValid(actor->tile)
            || (actor->data.critter.combat.results & DAM_DEAD) != 0) {
            continue;
        }

        int x;
        int y;
        if (tileToScreenXY(actor->tile, &x, &y, elevation) == 0) {
            totalX += x;
            totalY += y;
            count++;
        }
    }

    if (count == 0) {
        return;
    }

    int centerX = static_cast<int>(totalX / count);
    int centerY = static_cast<int>(totalY / count);
    int centerTile = tileFromScreenXY(centerX, centerY, elevation, true);
    if (tileIsValid(centerTile) && centerTile != gCenterTile) {
        tileSetCenter(centerTile,
            TILE_SET_CENTER_REFRESH_WINDOW | TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS);
    }
}

inline void localCoopSetRealtimeCombatActive(bool active)
{
    gLocalCoopRealtimeCombatActive = active;

    if (!active) {
        gLocalCoopLegacyYieldQueued = false;
        gLocalCoopNextLegacyYieldTick = 0;
        localCoopRealtimeAiReset();
        for (int index = 0; index < kLocalCoopMaxPlayers; index++) {
            LocalCoopRuntimeSlot& runtime = gLocalCoopRuntimeSlots[index];
            runtime.aimTarget = nullptr;
            runtime.nextPrimaryAttackTick = 0;
            runtime.nextSecondaryAttackTick = 0;
            runtime.nextReloadTick = 0;
            runtime.nextApRegenTick = 0;
            runtime.actionPointsHundredths = -1;
            runtime.actionPointsActorId = -1;
            runtime.apRegenDelayTicks = 0;
            localCoopClearQueuedAttack(runtime);
            gLocalCoopFocusSlots[index].combatTarget = nullptr;
            gLocalCoopFocusSlots[index].interactionTarget = nullptr;
        }
    }
}

inline void localCoopRuntimeTick();

inline void localCoopRuntimeTicker()
{
    localCoopRuntimeTick();
}

inline void localCoopRuntimeEnsureTicker()
{
    if (!gLocalCoopRuntimeTickerInstalled) {
        tickersAdd(localCoopRuntimeTicker);
        gLocalCoopRuntimeTickerInstalled = true;
    }
}

inline void localCoopRuntimeTick()
{
    if (gLocalCoopRuntimeInsideTick) {
        return;
    }

    gLocalCoopRuntimeInsideTick = true;

    if (!gLocalCoopInitialized) {
        localCoopInit();
    }

    localCoopRealtimeAiInstall();
    localCoopRuntimeEnsureTicker();
    localCoopPollControllers();

    // This should never become the player's normal state anymore. Keep the old
    // escape hatch only as a defensive breaker for an obscure legacy caller;
    // all known HUD/keyboard/mouse/controller/script attack paths are realtime.
    if (isInCombat()) {
        debugPrint("[COOP HYBRID] legacy turn state detected; forcing return to realtime world\n");
        localCoopDangerBegin();
        gLocalCoopRealtimeCombatActive = true;
        _game_user_wants_to_quit = 1;
    }

    localCoopProcessPostgameWorldSwitch();
    localCoopProcessCombatInput();
    localCoopRealtimeAiTick();

    if (!gLocalCoopDangerActive && gLocalCoopRealtimeAiActors.empty()) {
        gLocalCoopRealtimeCombatActive = false;
    }

    if (gDude != nullptr && gInterfaceBarWindow != -1) {
        interfaceRenderActionPoints(-1, -1);
    }

    localCoopUpdateSharedCamera();
    localCoopSweepSharedInventory();

    gLocalCoopRuntimeInsideTick = false;
}

} // namespace fallout

#endif /* LOCAL_COOP_RUNTIME_H */
