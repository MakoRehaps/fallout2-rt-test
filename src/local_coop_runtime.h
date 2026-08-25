#ifndef LOCAL_COOP_RUNTIME_H
#define LOCAL_COOP_RUNTIME_H

#include <SDL.h>

#include <algorithm>
#include <array>

#include "combat.h"
#include "critter.h"
#include "game.h"
#include "input.h"
#include "item.h"
#include "kb.h"
#include "local_coop.h"
#include "local_coop_ai_realtime.h"
#include "local_coop_danger.h"
#include "local_coop_focus.h"
#include "object.h"
#include "proto_types.h"
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
    bool primaryWasDown = false;
    bool reloadWasDown = false;
    bool secondaryWasDown = false;
    bool swapWasDown = false;
    bool postgameSwitchWasDown = false;
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

    Object* sharedOwner = localCoopGetSharedInventoryOwner();
    if (sharedOwner == nullptr) {
        return false;
    }

    if (actor == sharedOwner) {
        return weaponAttemptReload(actor, weapon) != -1;
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
        return false;
    }

    if (itemMoveForce(sharedOwner, actor, compatibleAmmo, 1) != 0) {
        return false;
    }

    bool reloaded = weaponAttemptReload(actor, weapon) != -1;
    localCoopSweepSharedInventory();
    return reloaded;
}

inline bool localCoopPerformAttack(LocalCoopPlayer& player, bool secondary)
{
    Object* actor = player.actor;
    if (actor == nullptr) {
        return false;
    }

    // Realtime input should be able to interrupt a queued movement path. Do not
    // wait for a Diablo-style right-click/analog run to finish before firing.
    if (animationIsBusy(actor)) {
        reg_anim_clear(actor);
    }

    LocalCoopRuntimeSlot& runtime = gLocalCoopRuntimeSlots[player.slot];
    Object* target = localCoopFocusFindEnemy(player);
    runtime.aimTarget = target;
    if (target == nullptr) {
        return false;
    }

    int hitMode = localCoopGetHitMode(player, secondary);
    int savedActionPoints = actor->data.critter.combat.ap;
    actor->data.critter.combat.ap = 9999;

    int badShot = _combat_check_bad_shot(actor, target, hitMode, false);
    if (badShot == COMBAT_BAD_SHOT_NO_AMMO) {
        localCoopReloadFromSharedPool(player);
        actor->data.critter.combat.ap = savedActionPoints;
        return false;
    }

    if (badShot != COMBAT_BAD_SHOT_OK) {
        actor->data.critter.combat.ap = savedActionPoints;
        return false;
    }

    int rc = _combat_attack(actor, target, hitMode, HIT_LOCATION_UNCALLED);
    actor->data.critter.combat.ap = savedActionPoints;
    if (rc != 0) {
        return false;
    }

    localCoopRealtimeAiEngageHostile(target, actor);
    gLocalCoopRealtimeCombatActive = true;

    // AP is not a visible realtime resource. Stock attack helpers can touch its
    // HUD, so immediately restore the normal non-combat presentation for P1.
    if (actor == gDude && gInterfaceBarWindow != -1) {
        interfaceRenderActionPoints(-1, -1);
    }
    return true;
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

    for (LocalCoopPlayer& player : gLocalCoopPlayers) {
        if (!player.connected
            || !player.humanOwned
            || player.controller == nullptr
            || player.actor == nullptr
            || player.uiMode != LocalCoopUiMode::World
            || (player.actor->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0) {
            continue;
        }

        LocalCoopRuntimeSlot& runtime = gLocalCoopRuntimeSlots[player.slot];
        runtime.aimTarget = localCoopFocusFindEnemy(player);

        int rightTrigger = SDL_GameControllerGetAxis(player.controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
        bool primaryDown = rightTrigger > 12000;
        bool secondaryDown = SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) != 0;
        bool reloadDown = SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_X) != 0;
        bool swapDown = SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_Y) != 0;
        if (player.slot == 0) {
            swapDown = swapDown || gPressedPhysicalKeys[SDL_SCANCODE_TAB];
        }

        if (swapDown && !runtime.swapWasDown) {
            localCoopSwapActiveHand(player.slot, true);
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
            gLocalCoopFocusSlots[index].combatTarget = nullptr;
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

    if (isInCombat()) {
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

    // Never leave the AP strip lit as a pseudo turn meter in the realtime game.
    if (gDude != nullptr && gInterfaceBarWindow != -1) {
        interfaceRenderActionPoints(-1, -1);
    }

    localCoopUpdateSharedCamera();
    localCoopSweepSharedInventory();

    gLocalCoopRuntimeInsideTick = false;
}

} // namespace fallout

#endif /* LOCAL_COOP_RUNTIME_H */
