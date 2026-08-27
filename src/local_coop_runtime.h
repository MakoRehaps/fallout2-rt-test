#ifndef LOCAL_COOP_RUNTIME_H
#define LOCAL_COOP_RUNTIME_H

#include <SDL.h>

#include <algorithm>
#include <array>

#include "actions.h"
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
#include "local_coop_fps.h"
#include "local_coop_personal_ui.h"
#include "local_coop_system_menu.h"
#include "mainmenu.h"
#include "mouse.h"
#include "object.h"
#include "perk.h"
#include "proto_types.h"
#include "trait.h"
#include "tile.h"
#include "unified_campaign_carryover.h"
#include "unified_campaign_transition.h"
#include "unified_living_world.h"

namespace fallout {

inline constexpr Uint32 kLocalCoopInitialSchedulerHeartbeatMs = 200;
inline constexpr Uint32 kLocalCoopBookkeepingHeartbeatMs = 1000;

struct LocalCoopRuntimeSlot {
    Uint32 nextPrimaryAttackTick = 0;
    Uint32 nextSecondaryAttackTick = 0;
    Uint32 nextReloadTick = 0;
    Uint32 nextHealingSkillTick = 0;
    Uint32 nextApRegenTick = 0;
    int actionPointsHundredths = -1;
    int actionPointsActorId = -1;
    int apRegenDelayTicks = 0;
    bool primaryWasDown = false;
    bool reloadWasDown = false;
    bool secondaryWasDown = false;
    bool swapWasDown = false;
    bool firstAidWasDown = false;
    bool doctorWasDown = false;
    bool pipboyWasDown = false;
    bool inventoryWasDown = false;
    bool pipboyToggleArmed = true;
    bool inventoryToggleArmed = true;
    Uint32 pipboyReleaseStartedTick = 0;
    Uint32 inventoryReleaseStartedTick = 0;
    bool startWasDown = false;
    bool startToggleArmed = true;
    Uint32 startReleaseStartedTick = 0;
    bool skilldexWasDown = false;
    bool postgameSwitchWasDown = false;
    bool queuedAttackPending = false;
    bool queuedAttackSecondary = false;
    int queuedAttackTargetId = -1;
    Object* aimTarget = nullptr;
};

// COOP_FINAL_HOTBINDS_V1
inline std::array<LocalCoopRuntimeSlot, kLocalCoopMaxPlayers> gLocalCoopRuntimeSlots;
inline bool gLocalCoopRealtimeCombatActive = false;
inline bool gLocalCoopRuntimeTickerInstalled = false;
inline bool gLocalCoopRuntimeInsideTick = false;
inline bool gLocalCoopLegacyYieldQueued = false;
inline Uint32 gLocalCoopNextLegacyYieldTick = 0;
inline Uint32 gLocalCoopNextCameraStepTick = 0;
inline int gLocalCoopCameraTargetTile = -1;

// COOP_P1_HYBRID_INPUT_START_TOGGLE_V1
inline bool gLocalCoopP1ControllerActive = false;
inline int gLocalCoopP1LastMouseX = -1;
inline int gLocalCoopP1LastMouseY = -1;
inline int gLocalCoopP1LastMouseButtons = 0;

// COOP_FOUR_PLAYER_HUD_V1
inline int gLocalCoopHudWindow = -1;
inline Uint32 gLocalCoopNextHudRefreshTick = 0;

inline void localCoopDestroyHud()
{
    if (gLocalCoopHudWindow != -1) {
        windowDestroy(gLocalCoopHudWindow);
        gLocalCoopHudWindow = -1;
    }
}

inline void localCoopEnsureHud()
{
    int width = screenGetWidth();
    int y = screenGetHeight() - INTERFACE_BAR_HEIGHT;
    if (width <= 0 || y < 0) {
        return;
    }

    if (gLocalCoopHudWindow == -1) {
        gLocalCoopHudWindow = windowCreate(0, y, width, INTERFACE_BAR_HEIGHT, _colorTable[0], WINDOW_MOVE_ON_TOP);
        if (gLocalCoopHudWindow == -1) {
            return;
        }
        if (gInterfaceBarWindow != -1) {
            interfaceBarHide();
        }
    }
}

inline void localCoopDrawHud(Uint32 now)
{
    if (static_cast<Sint32>(now - gLocalCoopNextHudRefreshTick) < 0) {
        return;
    }
    gLocalCoopNextHudRefreshTick = now + 100;

    localCoopEnsureHud();
    if (gLocalCoopHudWindow == -1) {
        return;
    }

    int width = screenGetWidth();
    int panelWidth = std::max(1, width / kLocalCoopMaxPlayers);
    windowFill(gLocalCoopHudWindow, 0, 0, width, INTERFACE_BAR_HEIGHT, _colorTable[0]);

    for (int slot = 0; slot < kLocalCoopMaxPlayers; slot++) {
        LocalCoopPlayer& player = gLocalCoopPlayers[slot];
        int x = slot * panelWidth;
        int textX = x + 10;
        int textWidth = std::max(40, panelWidth - 20);

        if (slot > 0) {
            windowDrawLine(gLocalCoopHudWindow, x, 5, x, INTERFACE_BAR_HEIGHT - 6, _colorTable[992]);
        }

        char header[64];
        snprintf(header, sizeof(header), "P%d  %s", slot + 1,
            player.connected ? "CONNECTED" : (player.slotLocked ? "RESERVED" : "EMPTY"));
        windowDrawText(gLocalCoopHudWindow, header, textWidth, textX, 8, _colorTable[992]);

        Object* actor = player.actor;
        if (actor == nullptr || !player.slotLocked) {
            windowDrawText(gLocalCoopHudWindow, "NO CHARACTER", textWidth, textX, 32, _colorTable[992]);
            continue;
        }

        int hp = actor->data.critter.hp;
        int maxHp = std::max(1, critterGetStat(actor, STAT_MAXIMUM_HIT_POINTS));
        int apHundredths = gLocalCoopRuntimeSlots[slot].actionPointsHundredths;
        if (apHundredths < 0) {
            apHundredths = critterGetStat(actor, STAT_MAXIMUM_ACTION_POINTS) * 100;
        }
        char stats[96];
        snprintf(stats, sizeof(stats), "HP %d/%d   AP %.1f", hp, maxHp, apHundredths / 100.0f);
        windowDrawText(gLocalCoopHudWindow, stats, textWidth, textX, 31, _colorTable[992]);

        Object* item = localCoopGetActiveItem(player);
        const char* itemName = item != nullptr ? protoGetName(item->pid) : nullptr;
        if (itemName == nullptr || *itemName == '\0') {
            itemName = "UNARMED";
        }
        const char* hand = localCoopGetActiveHand(player) == HAND_LEFT ? "L" : "R";
        char equip[160];
        snprintf(equip, sizeof(equip), "[%s HAND] %s", hand, itemName);
        windowDrawText(gLocalCoopHudWindow, equip, textWidth, textX, 54, _colorTable[992]);

        if (player.archetype >= 0 && player.archetype < kLocalCoopArchetypeCount) {
            windowDrawText(gLocalCoopHudWindow, kLocalCoopArchetypeNames[player.archetype], textWidth, textX, 76, _colorTable[992]);
        }
    }

    windowRefresh(gLocalCoopHudWindow);
}

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
            // Reload can change item state and spends its own AP. Require the
            // attack cost again so reload+shot can never overdraw the AP pool.
            localCoopSyncActiveHandVisual(player.slot);
            if (!localCoopHasActionPoints(player, attackCost, SDL_GetTicks())) {
                debugPrint("[COOP ATTACK] slot=%d reloaded-but-blocked-ap current=%d cost=%d\n",
                    player.slot,
                    runtime.actionPointsHundredths,
                    attackCost);
                actor->data.critter.combat.ap = savedActionPoints;
                return false;
            }
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

inline void localCoopProcessModalMenuInput()
{
    // COOP_FOUR_PERSONAL_HUD_SHARED_BAG_RUNTIME_V1
    localCoopPersonalUiTick();
    // The co-op ticker continues to run inside stock modal loops. Treat every
    // non-world UI as exclusive so PhoBoi/Skilldex input can never stack a
    // second screen over Inventory, Loot, Barter, Dialogue, Character, etc.
    constexpr int kBlockingMenuModes =
        GameMode::kWorldmap
        | GameMode::kDialog
        | GameMode::kOptions
        | GameMode::kSaveGame
        | GameMode::kLoadGame
        | GameMode::kPreferences
        | GameMode::kHelp
        | GameMode::kEditor
        | GameMode::kPipboy
        | GameMode::kInventory
        | GameMode::kAutomap
        | GameMode::kSkilldex
        | GameMode::kUseOn
        | GameMode::kLoot
        | GameMode::kBarter
        | GameMode::kHero
        | GameMode::kDialogReview
        | GameMode::kCounter;
    int currentGameMode = GameMode::getCurrentGameMode();
    bool modalActive = (currentGameMode & kBlockingMenuModes) != 0;
    bool inventoryModalActive = (currentGameMode & GameMode::kInventory) != 0;
    bool pipboyModalActive = (currentGameMode & (GameMode::kPipboy | GameMode::kAutomap)) != 0;
    bool startMenuModalActive = (currentGameMode & (GameMode::kOptions
        | GameMode::kPreferences
        | GameMode::kSaveGame
        | GameMode::kLoadGame
        | GameMode::kHelp)) != 0;
    Uint32 now = SDL_GetTicks();

    for (int slot = 0; slot < kLocalCoopMaxPlayers; slot++) {
        LocalCoopPlayer& player = gLocalCoopPlayers[slot];
        LocalCoopRuntimeSlot& runtime = gLocalCoopRuntimeSlots[slot];

        bool backDown = player.controller != nullptr
            && SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_BACK) != 0;
        bool startDown = player.controller != nullptr
            && SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_START) != 0;
        bool skilldexDown = player.controller != nullptr
            && SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_RIGHTSTICK) != 0;
        bool pipboyDown = player.controller != nullptr
            && SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT) != 0;
        // Personal co-op bag overlays consume Back per player; stock Inventory is global.
        bool inventoryDown = false;
        bool canOpen = !modalActive
            && player.connected
            && player.humanOwned
            && player.controller != nullptr
            && player.uiMode == LocalCoopUiMode::World
            && !localCoopDangerBlocksMapExit();
        // COOP_P1_GLOBAL_UI_OWNER_V1
        // COOP_P1_GLOBAL_UI_TOGGLE_V1
        // Inventory and Pip-Boy/map are global modal screens, so only P1 owns
        // their open/close toggle. A phone packet can momentarily cross neutral,
        // so a simple rising-edge test is not enough: after every toggle require
        // a real release held for 140 ms before the button is armed again. This
        // prevents one physical tap from closing and immediately reopening.
        if (slot == 0) {
            if (!pipboyDown) {
                if (runtime.pipboyReleaseStartedTick == 0) runtime.pipboyReleaseStartedTick = now;
                if (!runtime.pipboyToggleArmed
                    && static_cast<Sint32>(now - runtime.pipboyReleaseStartedTick) >= 140) {
                    runtime.pipboyToggleArmed = true;
                }
            } else {
                runtime.pipboyReleaseStartedTick = 0;
            }

            if (!inventoryDown) {
                if (runtime.inventoryReleaseStartedTick == 0) runtime.inventoryReleaseStartedTick = now;
                if (!runtime.inventoryToggleArmed
                    && static_cast<Sint32>(now - runtime.inventoryReleaseStartedTick) >= 140) {
                    runtime.inventoryToggleArmed = true;
                }
            } else {
                runtime.inventoryReleaseStartedTick = 0;
            }

            if (!startDown) {
                if (runtime.startReleaseStartedTick == 0) runtime.startReleaseStartedTick = now;
                if (!runtime.startToggleArmed
                    && static_cast<Sint32>(now - runtime.startReleaseStartedTick) >= 140) {
                    runtime.startToggleArmed = true;
                }
            } else {
                runtime.startReleaseStartedTick = 0;
            }
        }

        bool canOwnGlobalUi = canOpen && slot == 0;
        bool p1PipboyToggle = slot == 0 && pipboyDown && runtime.pipboyToggleArmed;
        bool p1InventoryToggle = slot == 0 && inventoryDown && runtime.inventoryToggleArmed;

        if (p1PipboyToggle && pipboyModalActive) {
            runtime.pipboyToggleArmed = false;
            gLocalCoopModalControllerSlot = 0;
            enqueueInputEvent(KEY_ESCAPE);
            debugPrint("[PHOBOI INPUT] slot=0 global-ui=pipboy action=close\n");
        } else if (p1InventoryToggle && inventoryModalActive) {
            runtime.inventoryToggleArmed = false;
            gLocalCoopModalControllerSlot = 0;
            enqueueInputEvent(KEY_ESCAPE);
            debugPrint("[COOP INVENTORY] slot=0 global-ui=inventory action=close\n");
        } else if (canOwnGlobalUi && p1PipboyToggle) {
            runtime.pipboyToggleArmed = false;
            gLocalCoopModalControllerSlot = 0;
            enqueueInputEvent(KEY_LOWERCASE_P);
            modalActive = true;
            debugPrint("[PHOBOI INPUT] slot=0 global-ui=pipboy action=open\n");
        } else if (canOwnGlobalUi && p1InventoryToggle) {
            runtime.inventoryToggleArmed = false;
            gLocalCoopModalControllerSlot = 0;
            enqueueInputEvent(KEY_LOWERCASE_I);
            modalActive = true;
            debugPrint("[COOP INVENTORY] slot=0 global-ui=inventory action=open\n");
        } else if (canOpen && skilldexDown && !runtime.skilldexWasDown) {
            gLocalCoopModalControllerSlot = slot;
            gLocalCoopSkilldexInvokerSlot = slot;
            enqueueInputEvent(KEY_LOWERCASE_S);
            modalActive = true;
            debugPrint("[COOP SKILLDEX] slot=%d source=controller button=right-stick\n", slot);
        } else if (canOpen && slot == 0 && startDown && runtime.startToggleArmed) {
            // COOP_SYSTEM_MENU_RUNTIME_V1
            runtime.startToggleArmed = false;
            gLocalCoopModalControllerSlot = 0;
            localCoopSystemMenuOpen();
            modalActive = true;
            debugPrint("[COOP MENU] slot=0 source=controller button=start action=open-phoboi\n");
        }

        runtime.pipboyWasDown = pipboyDown;
        runtime.inventoryWasDown = inventoryDown;
        runtime.startWasDown = startDown;
        runtime.skilldexWasDown = skilldexDown;
    }
}

inline Object* localCoopHealingTarget(LocalCoopPlayer& player)
{
    Object* target = localCoopFocusFindInteractable(player);
    if (target != nullptr
        && PID_TYPE(target->pid) == OBJ_TYPE_CRITTER
        && (target->data.critter.combat.results & DAM_DEAD) == 0
        && !localCoopFocusIsEnemy(player.actor, target)) {
        return target;
    }

    // With no friendly critter aimed/selected, heal the invoking player's own
    // character. This makes the hotkeys useful without requiring a mouse.
    return player.actor;
}

inline bool localCoopUseHealingSkill(LocalCoopPlayer& player, int skill)
{
    Object* actor = player.actor;
    if (actor == nullptr
        || animationIsBusy(actor)
        || localCoopDangerBlocksMapExit()) {
        return false;
    }

    Object* target = player.actor;
    if (target == nullptr) {
        return false;
    }

    // Save diagnostic values before dispatch. Fallout skill use can run modal
    // engine work which may refresh bindings before it returns.
    int actorId = actor->id;
    int targetId = target->id;
    int rc = actionUseSkill(actor, target, skill);
    debugPrint("[COOP SKILL] slot=%d actorId=%d targetId=%d skill=%d rc=%d\\n",
        player.slot,
        actorId,
        targetId,
        skill,
        rc);
    return rc == 0;
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
        bool medicalDown = false;

        if (hasController) {
            int rightTrigger = SDL_GameControllerGetAxis(player.controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
            primaryDown = rightTrigger > 12000;
            secondaryDown = SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) != 0;
            reloadDown = SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_X) != 0;
            swapDown = SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_Y) != 0;
            medicalDown = SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) != 0;
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

        // Phone packets can briefly cross neutral while a touch remains held,
        // producing a second rising edge. Share a short cooldown between both
        // healing skills so modal Fallout skill work cannot be re-entered.
        if (medicalDown
            && !runtime.doctorWasDown
            && localCoopTickReached(now, runtime.nextHealingSkillTick)) {
            runtime.nextHealingSkillTick = now + 1000;
            localCoopUseHealingSkill(player, SKILL_FIRST_AID);
            localCoopUseHealingSkill(player, SKILL_DOCTOR);
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
        runtime.firstAidWasDown = false;
        runtime.doctorWasDown = medicalDown;
    }
}

inline void localCoopUpdateP1InputSource()
{
    LocalCoopPlayer& p1 = gLocalCoopPlayers[0];

    int mouseX = 0;
    int mouseY = 0;
    mouseGetPosition(&mouseX, &mouseY);
    int mouseButtons = mouse_get_last_buttons();
    bool mouseActivity = gLocalCoopP1LastMouseX != -1
        && (mouseX != gLocalCoopP1LastMouseX
            || mouseY != gLocalCoopP1LastMouseY
            || mouseButtons != gLocalCoopP1LastMouseButtons);
    gLocalCoopP1LastMouseX = mouseX;
    gLocalCoopP1LastMouseY = mouseY;
    gLocalCoopP1LastMouseButtons = mouseButtons;

    bool keyboardActivity = false;
    int keyboardCount = 0;
    const Uint8* keyboard = SDL_GetKeyboardState(&keyboardCount);
    if (keyboard != nullptr) {
        for (int i = 0; i < keyboardCount; i++) {
            if (keyboard[i]) { keyboardActivity = true; break; }
        }
    }

    // COOP_P1_HYBRID_INPUT_TRIGGER_FIX_V2
    bool controllerActivity = false;
    if (p1.controller != nullptr) {
        const SDL_GameControllerAxis stickAxes[] = {
            SDL_CONTROLLER_AXIS_LEFTX,
            SDL_CONTROLLER_AXIS_LEFTY,
            SDL_CONTROLLER_AXIS_RIGHTX,
            SDL_CONTROLLER_AXIS_RIGHTY,
        };
        for (SDL_GameControllerAxis axis : stickAxes) {
            int value = SDL_GameControllerGetAxis(p1.controller, axis);
            if (std::abs(value) > 9000) { controllerActivity = true; break; }
        }
        if (!controllerActivity) {
            int leftTrigger = SDL_GameControllerGetAxis(p1.controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
            int rightTrigger = SDL_GameControllerGetAxis(p1.controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
            // Trigger rest can be either 0 or SDL_JOYSTICK_AXIS_MIN depending on
            // backend. Only positive pull values count as controller activity.
            controllerActivity = leftTrigger > 12000 || rightTrigger > 12000;
        }
        if (!controllerActivity) {
            for (int button = 0; button < SDL_CONTROLLER_BUTTON_MAX; button++) {
                if (SDL_GameControllerGetButton(p1.controller, static_cast<SDL_GameControllerButton>(button))) {
                    controllerActivity = true; break;
                }
            }
        }
    }

    // Latest active device wins. Mouse/keyboard can immediately take the cursor
    // back; controller activity hides it again. Neither device is disabled.
    if (mouseActivity || keyboardActivity) {
        gLocalCoopP1ControllerActive = false;
        if (cursorIsHidden()) mouseShowCursor();
    } else if (controllerActivity) {
        gLocalCoopP1ControllerActive = true;
        if (!cursorIsHidden()) mouseHideCursor();
    }
}

inline void localCoopUpdateSharedCamera()
{
    if (gDude == nullptr || !tileIsValid(gDude->tile) || !tileIsValid(gCenterTile)) {
        gLocalCoopCameraTargetTile = -1;
        return;
    }

    int elevation = gDude->elevation;
    int minimumX = 0x7FFFFFFF;
    int minimumY = 0x7FFFFFFF;
    int maximumX = -0x7FFFFFFF;
    int maximumY = -0x7FFFFFFF;
    int count = 0;

    // Frame the extents rather than averaging positions. An average is pulled
    // toward a three-player cluster and can strand the fourth player at an edge;
    // a bounding-box midpoint gives every local actor equal screen margin.
    for (const LocalCoopPlayer& player : gLocalCoopPlayers) {
        Object* actor = player.actor;
        if (!player.humanOwned
            || actor == nullptr
            || actor->elevation != elevation
            || !tileIsValid(actor->tile)
            || (actor->flags & OBJECT_HIDDEN) != 0
            || (actor->data.critter.combat.results & DAM_DEAD) != 0) {
            continue;
        }

        int x = 0;
        int y = 0;
        if (tileToScreenXY(actor->tile, &x, &y, elevation) == 0) {
            minimumX = std::min(minimumX, x);
            minimumY = std::min(minimumY, y);
            maximumX = std::max(maximumX, x);
            maximumY = std::max(maximumY, y);
            count++;
        }
    }

    if (count == 0) {
        gLocalCoopCameraTargetTile = -1;
        return;
    }

    int targetX = minimumX + (maximumX - minimumX) / 2;
    int targetY = minimumY + (maximumY - minimumY) / 2;
    int targetTile = tileFromScreenXY(targetX, targetY, elevation, true);
    if (!tileIsValid(targetTile)) {
        return;
    }
    gLocalCoopCameraTargetTile = targetTile;

    Uint32 now = SDL_GetTicks();
    // Personal HUDs are rendered independently by localCoopPersonalUiTick.
    if (!localCoopTickReached(now, gLocalCoopNextCameraStepTick)) {
        return;
    }

    int distance = tileDistanceBetween(gCenterTile, targetTile);
    if (distance <= 0) {
        gLocalCoopNextCameraStepTick = now + 33;
        return;
    }

    // Ease instead of snapping. Far-away targets catch up faster, while the last
    // few hexes advance one at a time to avoid visible camera judder.
    int stepDistance = 1;
    if (distance > 12) {
        stepDistance = 4;
    } else if (distance > 7) {
        stepDistance = 3;
    } else if (distance > 3) {
        stepDistance = 2;
    }

    int nextCenter = targetTile;
    if (stepDistance < distance) {
        int rotation = tileGetRotationTo(gCenterTile, targetTile);
        nextCenter = tileGetTileInDirection(gCenterTile, rotation, stepDistance);
    }

    if (tileIsValid(nextCenter) && nextCenter != gCenterTile) {
        tileSetCenter(nextCenter,
            TILE_SET_CENTER_REFRESH_WINDOW | TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS);
    }
    gLocalCoopNextCameraStepTick = now + 33;
}

inline void localCoopSetRealtimeCombatActive(bool active)
{
    gLocalCoopRealtimeCombatActive = active;

    if (!active) {
        gLocalCoopLegacyYieldQueued = false;
        gLocalCoopNextLegacyYieldTick = 0;
        gLocalCoopNextCameraStepTick = 0;
        gLocalCoopCameraTargetTile = -1;
        localCoopRealtimeAiReset();
        for (int index = 0; index < kLocalCoopMaxPlayers; index++) {
            LocalCoopRuntimeSlot& runtime = gLocalCoopRuntimeSlots[index];
            runtime.aimTarget = nullptr;
            runtime.nextPrimaryAttackTick = 0;
            runtime.nextSecondaryAttackTick = 0;
            runtime.nextReloadTick = 0;
            runtime.nextHealingSkillTick = 0;
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

    // COOP_MAIN_MENU_RUNTIME_GATE_V1
    // The stock Fallout main menu must own its window and input completely.
    // Do not initialize players, create personal HUDs, hide the stock interface,
    // process gameplay controller binds, or advance the living world here.
    // If gameplay returned to the menu, tear down any presentation windows first.
    if (_main_menu_is_enabled()) {
        localCoopFpsDestroyWindow();
        localCoopPersonalUiShutdown();
        localCoopDestroyHud();
        if (cursorIsHidden()) mouseShowCursor();
        gLocalCoopRuntimeInsideTick = false;
        return;
    }

    if (!gLocalCoopInitialized) {
        localCoopInit();
    }

    localCoopRealtimeAiInstall();
    localCoopRuntimeEnsureTicker();
    localCoopPollControllers();
    localCoopUpdateP1InputSource();
    localCoopProcessJoinMenus();
    localCoopSystemMenuTick();
    localCoopAccessibilityTick();
    localCoopRestoreCharactersFromSave();
    localCoopKeepReservedActorsWithParty();

    // COOP_EXPLICIT_SIMULATION_PAUSE_RUNTIME_V1
    // Keep polling controllers and join UI while paused, but freeze the world.
    // Alt+Tab/focus loss is intentionally NOT part of this condition.
    if (localCoopSimulationPaused()) {
        gLocalCoopRuntimeInsideTick = false;
        return;
    }

    // COOP_UNIFIED_LIVING_WORLD_RUNTIME_V1
    // Advance the offline faction/economy/territory simulation only while the
    // gameplay world itself is running. Explicit co-op modal pauses freeze it.
    unifiedLivingRuntimeTick();

    // This should never become the player's normal state anymore. Keep the old
    // escape hatch only as a defensive breaker for an obscure legacy caller;
    // all known HUD/keyboard/mouse/controller/script attack paths are realtime.
    if (isInCombat()) {
        debugPrint("[COOP HYBRID] legacy turn state detected; forcing return to realtime world\n");
        gLocalCoopRealtimeCombatActive = true;
        _game_user_wants_to_quit = 1;
    }

    localCoopProcessPostgameWorldSwitch();
    localCoopProcessModalMenuInput();
    localCoopProcessCombatInput();
    localCoopRealtimeAiTick();

    if (!gLocalCoopDangerActive && gLocalCoopRealtimeAiActors.empty()) {
        gLocalCoopRealtimeCombatActive = false;
    }

    if (gDude != nullptr && gInterfaceBarWindow != -1) {
        interfaceRenderActionPoints(-1, -1);
    }

    // COOP_NATIVE_BILLBOARD_FPS_RUNTIME_V1
    localCoopUpdateSharedCamera();
    localCoopFpsTick();
    // Personal HUDs draw after FPS so all four remain readable.
    localCoopPersonalUiTick();
    localCoopSweepSharedInventory();

    gLocalCoopRuntimeInsideTick = false;
}

} // namespace fallout

#endif /* LOCAL_COOP_RUNTIME_H */
