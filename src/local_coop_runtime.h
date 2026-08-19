#ifndef LOCAL_COOP_RUNTIME_H
#define LOCAL_COOP_RUNTIME_H

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cstdint>

#include "combat.h"
#include "critter.h"
#include "input.h"
#include "item.h"
#include "kb.h"
#include "local_coop.h"
#include "object.h"
#include "proto_types.h"
#include "stat.h"
#include "tile.h"

namespace fallout {

// Realtime input state is intentionally separate from LocalCoopPlayer. The
// controller slots describe ownership/bindings; this structure describes the
// short-lived realtime combat state associated with each slot.
struct LocalCoopRuntimeSlot {
    Uint32 nextPrimaryAttackTick = 0;
    Uint32 nextSecondaryAttackTick = 0;
    Uint32 nextReloadTick = 0;
    bool primaryWasDown = false;
    bool reloadWasDown = false;
    bool secondaryWasDown = false;
    Object* aimTarget = nullptr;
};

inline std::array<LocalCoopRuntimeSlot, kLocalCoopMaxPlayers> gLocalCoopRuntimeSlots;
inline bool gLocalCoopRealtimeCombatActive = false;
inline bool gLocalCoopRuntimeTickerInstalled = false;
inline bool gLocalCoopRuntimeInsideTick = false;
inline bool gLocalCoopLegacyYieldQueued = false;

inline bool localCoopTickReached(Uint32 now, Uint32 target)
{
    return static_cast<Sint32>(now - target) >= 0;
}

inline int localCoopRotationDifference(int lhs, int rhs)
{
    int difference = std::abs(lhs - rhs) % 6;
    return std::min(difference, 6 - difference);
}

inline bool localCoopIsAttackableTarget(const Object* attacker, const Object* candidate)
{
    if (attacker == nullptr || candidate == nullptr || attacker == candidate) {
        return false;
    }

    if (PID_TYPE(candidate->pid) != OBJ_TYPE_CRITTER) {
        return false;
    }

    if (candidate->elevation != attacker->elevation) {
        return false;
    }

    if ((candidate->flags & OBJECT_HIDDEN) != 0) {
        return false;
    }

    if ((candidate->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0) {
        return false;
    }

    // Player-owned party actors are never valid friendly-fire auto targets.
    if (localCoopActorIsHumanOwned(candidate)) {
        return false;
    }

    return candidate->data.critter.combat.team != attacker->data.critter.combat.team;
}

inline Object* localCoopFindAimTarget(LocalCoopPlayer& player)
{
    Object* attacker = player.actor;
    if (attacker == nullptr) {
        return nullptr;
    }

    int aimRotation = localCoopDirectionFromStick(player.aimX, player.aimY);
    if (aimRotation == -1) {
        return nullptr;
    }

    Object** critters = nullptr;
    int critterCount = objectListCreate(-1, attacker->elevation, OBJ_TYPE_CRITTER, &critters);
    if (critterCount <= 0 || critters == nullptr) {
        return nullptr;
    }

    Object* bestTarget = nullptr;
    int bestScore = 0x7FFFFFFF;

    for (int index = 0; index < critterCount; index++) {
        Object* candidate = critters[index];
        if (!localCoopIsAttackableTarget(attacker, candidate)) {
            continue;
        }

        int targetRotation = tileGetRotationTo(attacker->tile, candidate->tile);
        int rotationDifference = localCoopRotationDifference(aimRotation, targetRotation);
        if (rotationDifference > 1) {
            continue;
        }

        int distance = objectGetDistanceBetween(attacker, candidate);
        // Prefer the target closest to the stick direction, then the nearest
        // target within that sector.
        int score = rotationDifference * 1000 + distance;
        if (score < bestScore) {
            bestScore = score;
            bestTarget = candidate;
        }
    }

    objectListFree(critters);
    return bestTarget;
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

    // AP is no longer a player turn budget in realtime mode. Preserve weapon
    // pacing by translating the old AP cost into a short realtime cooldown.
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

    // P1 already owns the shared pool, so the engine's normal reload search is
    // sufficient. P2-P4 borrow one compatible ammo stack for the reload and
    // any remainder is swept back to the party pool afterwards.
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
    Object* target = localCoopFindAimTarget(player);
    runtime.aimTarget = target;
    if (target == nullptr) {
        return false;
    }

    int hitMode = localCoopGetHitMode(actor, secondary);

    // Realtime player actions are cooldown-gated, not AP-budget-gated. Give the
    // legacy attack path enough AP to pass its validation and let the cooldown
    // below provide the actual pacing.
    actor->data.critter.combat.ap = 9999;

    int badShot = _combat_check_bad_shot(actor, target, hitMode, false);
    if (badShot == COMBAT_BAD_SHOT_NO_AMMO) {
        localCoopReloadFromSharedPool(player);
        return false;
    }

    if (badShot != COMBAT_BAD_SHOT_OK) {
        return false;
    }

    if (_combat_attack(actor, target, hitMode, HIT_LOCATION_UNCALLED) != 0) {
        return false;
    }

    // Restore a large AP reserve because _combat_attack subtracts the legacy AP
    // cost. NPCs retain normal AP handling in combat.cc.
    actor->data.critter.combat.ap = 9999;
    return true;
}

inline void localCoopSuppressHumanCompanionAi()
{
    if (!isInCombat()) {
        return;
    }

    // The legacy sequence still schedules NPC turns. Mark P2-P4 to lose that
    // legacy turn before combat.cc reaches them. Their controller input remains
    // live from the ticker and therefore they never fall back to combat AI.
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

inline void localCoopYieldLegacyPlayerTurn()
{
    if (!isInCombat() || !gLocalCoopRealtimeCombatActive) {
        gLocalCoopLegacyYieldQueued = false;
        return;
    }

    Object* current = _combat_whose_turn();
    if (current != gDude) {
        gLocalCoopLegacyYieldQueued = false;
        return;
    }

    // combat.cc normally blocks here waiting for the player to press Space.
    // Queue it once per P1 turn so the old sequence keeps feeding NPC AI turns
    // while all four human actors remain independently controllable.
    if (!gLocalCoopLegacyYieldQueued) {
        enqueueInputEvent(KEY_SPACE);
        gLocalCoopLegacyYieldQueued = true;
    }
}

inline void localCoopProcessCombatInput()
{
    if (!isInCombat()) {
        return;
    }

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
        runtime.aimTarget = localCoopFindAimTarget(player);

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
                // Avoid hammering validation every frame when no valid shot is
                // available.
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
    if (isInCombat()) {
        return;
    }

    for (LocalCoopPlayer& player : gLocalCoopPlayers) {
        if (!player.connected
            || !player.humanOwned
            || player.controller == nullptr
            || player.actor == nullptr
            || player.uiMode != LocalCoopUiMode::World) {
            continue;
        }

        LocalCoopRuntimeSlot& runtime = gLocalCoopRuntimeSlots[player.slot];
        int rightTrigger = SDL_GameControllerGetAxis(player.controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
        bool primaryDown = rightTrigger > 12000;

        if (primaryDown && !runtime.primaryWasDown) {
            Object* target = localCoopFindAimTarget(player);
            runtime.aimTarget = target;
            if (target != nullptr) {
                // Pre-mark companion turns before entering the blocking legacy
                // combat function. The background ticker takes over as soon as
                // combat begins.
                for (int slot = 1; slot < kLocalCoopMaxPlayers; slot++) {
                    Object* companion = gLocalCoopPlayers[slot].actor;
                    if (companion != nullptr) {
                        companion->data.critter.combat.results |= DAM_LOSE_TURN;
                    }
                }

                gLocalCoopRealtimeCombatActive = true;

                CombatStartData csd{};
                csd.attacker = player.actor;
                csd.defender = target;
                _combat(&csd);
            }
        }

        runtime.primaryWasDown = primaryDown;
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
        for (LocalCoopRuntimeSlot& slot : gLocalCoopRuntimeSlots) {
            slot.aimTarget = nullptr;
            slot.nextPrimaryAttackTick = 0;
            slot.nextSecondaryAttackTick = 0;
            slot.nextReloadTick = 0;
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

    localCoopRuntimeEnsureTicker();
    localCoopPollControllers();

    if (isInCombat()) {
        gLocalCoopRealtimeCombatActive = true;
        localCoopSuppressHumanCompanionAi();
        localCoopProcessCombatInput();
        localCoopYieldLegacyPlayerTurn();
    } else {
        if (gLocalCoopRealtimeCombatActive) {
            localCoopSetRealtimeCombatActive(false);
        }
        localCoopProcessWorldCombatStart();
    }

    localCoopUpdateSharedCamera();
    localCoopSweepSharedInventory();

    gLocalCoopRuntimeInsideTick = false;
}

} // namespace fallout

#endif /* LOCAL_COOP_RUNTIME_H */
