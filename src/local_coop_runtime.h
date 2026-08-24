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

inline int localCoopGetHitMode(Object* actor, bool secondary)
{
    Object* weapon = critterGetItem2(actor);
    if (weapon == nullptr) {
        return secondary ? HIT_MODE_KICK : HIT_MODE_PUNCH;
    }

    return secondary ? HIT_MODE_RIGHT_WEAPON_SECONDARY : HIT_MODE_RIGHT_WEAPON_PRIMARY;
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

    Object* weapon = critterGetItem2(actor);
    if (weapon == nullptr) {
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
    if (actor == nullptr || animationIsBusy(actor)) {
        return false;
    }

    LocalCoopRuntimeSlot& runtime = gLocalCoopRuntimeSlots[player.slot];
    Object* target = localCoopFocusFindEnemy(player);
    runtime.aimTarget = target;
    if (target == nullptr) {
        return false;
    }

    int hitMode = localCoopGetHitMode(actor, secondary);
    actor->data.critter.combat.ap = 9999;

    int badShot = _combat_check_bad_shot(actor, target, hitMode, false);
    if (badShot == COMBAT_BAD_SHOT_NO_AMMO) {
        localCoopReloadFromSharedPool(player);
        return false;
    }

    if (badShot != COMBAT_BAD_SHOT_OK) {
        return false;
    }

    // Directly sequence the stock Fallout attack animation/damage calculation.
    // Do not call _combat() here: the fused game has no player-facing transition
    // into the original turn loop. Combat is simply another realtime world action.
    if (_combat_attack(actor, target, hitMode, HIT_LOCATION_UNCALLED) != 0) {
        return false;
    }

    gLocalCoopRealtimeCombatActive = true;
    actor->data.critter.combat.ap = 9999;
    return true;
}

inline void localCoopSuppressHumanCompanionAi()
{
    if (!isInCombat()) {
        return;
    }

    for (int slot = 1; slot < kLocalCoopMaxPlayers; slot++) {
        Object* actor = gLocalCoopPlayers[slot].actor;
        if (actor == nullptr
            || !gLocalCoopPlayers[slot].humanOwned
            || (actor->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0) {
            continue;
        }

        actor->data.critter.combat.results |= DAM_LOSE_TURN;
    }
}

inline Uint32 localCoopLegacyHeartbeatDelay()
{
    return localCoopRealtimeAiHasRegisteredActors()
        ? kLocalCoopBookkeepingHeartbeatMs
        : kLocalCoopInitialSchedulerHeartbeatMs;
}

inline void localCoopYieldLegacyPlayerTurn()
{
    if (!isInCombat() || !gLocalCoopRealtimeCombatActive) {
        gLocalCoopLegacyYieldQueued = false;
        gLocalCoopNextLegacyYieldTick = 0;
        return;
    }

    Object* current = _combat_whose_turn();
    if (current != gDude) {
        gLocalCoopLegacyYieldQueued = false;
        return;
    }

    Uint32 now = SDL_GetTicks();
    if (!gLocalCoopLegacyYieldQueued
        && (gLocalCoopNextLegacyYieldTick == 0
            || localCoopTickReached(now, gLocalCoopNextLegacyYieldTick))) {
        enqueueInputEvent(KEY_SPACE);
        gLocalCoopLegacyYieldQueued = true;
        gLocalCoopNextLegacyYieldTick = now + localCoopLegacyHeartbeatDelay();
    }
}

inline void localCoopProcessPostgameWorldSwitch()
{
    if (isInCombat() || !unifiedCampaignBothGamesCompleted()) {
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
    // Controller attacks are valid directly in the world. The original beta
    // waited for isInCombat(), which forced the first trigger pull through
    // Fallout's modal/turn-based _combat() loop. Keep the same wall-clock
    // cooldowns but never require a legacy combat state.
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

        if (reloadDown && !runtime.reloadWasDown && localCoopTickReached(now, runtime.nextReloadTick)) {
            localCoopReloadFromSharedPool(player);
            runtime.nextReloadTick = now + 400;
        }

        if (primaryDown && localCoopTickReached(now, runtime.nextPrimaryAttackTick)) {
            int hitMode = localCoopGetHitMode(player.actor, false);
            if (localCoopPerformAttack(player, false)) {
                runtime.nextPrimaryAttackTick = now + localCoopGetAttackCooldown(player.actor, hitMode);
            } else {
                runtime.nextPrimaryAttackTick = now + 100;
            }
        }

        if (secondaryDown
            && !runtime.secondaryWasDown
            && localCoopTickReached(now, runtime.nextSecondaryAttackTick)) {
            int hitMode = localCoopGetHitMode(player.actor, true);
            if (localCoopPerformAttack(player, true)) {
                runtime.nextSecondaryAttackTick = now + localCoopGetAttackCooldown(player.actor, hitMode);
            } else {
                runtime.nextSecondaryAttackTick = now + 150;
            }
        }

        runtime.primaryWasDown = primaryDown;
        runtime.reloadWasDown = reloadDown;
        runtime.secondaryWasDown = secondaryDown;
    }
}

inline void localCoopProcessWorldCombatStart()
{
    // Kept as a compatibility hook for older callers. Player input no longer
    // enters Fallout's _combat() loop; localCoopProcessCombatInput performs the
    // attack directly in the normal world simulation.
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
        // Legacy combat can still be entered temporarily by untouched game
        // scripts while that conversion is being completed. Keep the old bridge
        // safe for those cases, but player input itself never starts this state.
        gLocalCoopRealtimeCombatActive = true;
        localCoopSuppressHumanCompanionAi();
        localCoopRealtimeAiTick();
        localCoopProcessCombatInput();
        localCoopYieldLegacyPlayerTurn();
    } else {
        localCoopProcessPostgameWorldSwitch();
        localCoopProcessCombatInput();
    }

    localCoopUpdateSharedCamera();
    localCoopSweepSharedInventory();

    gLocalCoopRuntimeInsideTick = false;
}

} // namespace fallout

#endif /* LOCAL_COOP_RUNTIME_H */