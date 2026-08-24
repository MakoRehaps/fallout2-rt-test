#ifndef LOCAL_COOP_AI_REALTIME_H
#define LOCAL_COOP_AI_REALTIME_H

#include <SDL.h>

#include <algorithm>
#include <unordered_map>

#include "animation.h"
#include "combat.h"
#include "combat_ai.h"
#include "critter.h"
#include "inventory.h"
#include "item.h"
#include "local_coop.h"
#include "object.h"
#include "proto_types.h"
#include "scripts.h"
#include "stat.h"

namespace fallout {

// Legacy combat and the new world-realtime layer share the same stable actor-ID
// registry. In legacy combat the untouched Fallout AI brain is sliced by a wall
// clock. Outside legacy combat we use a deliberately small realtime driver that
// reuses stock pathfinding, reload, bad-shot validation and attack sequencing,
// but never enters Fallout's turn loop.
struct LocalCoopRealtimeAiActorState {
    Uint32 nextActionTick = 0;
    int preferredTargetId = -1;
};

inline std::unordered_map<int, LocalCoopRealtimeAiActorState> gLocalCoopRealtimeAiActors;
inline Uint32 gLocalCoopRealtimeCombatClockTick = 0;
inline bool gLocalCoopRealtimeAiInsideTick = false;
inline bool gLocalCoopRealtimeWorldCombatActive = false;

inline Uint32 localCoopRealtimeAiCooldownForSlice(int actionPoints)
{
    int cooldown = actionPoints * 120;
    cooldown = std::max(300, std::min(cooldown, 1000));
    return static_cast<Uint32>(cooldown);
}

inline Uint32 localCoopRealtimeAiInitialStagger(const Object* actor)
{
    if (actor == nullptr) {
        return 100;
    }

    // Distribute the first action so a room full of enemies does not fire on
    // the same frame. IDs are stable while the object is alive.
    return 100 + static_cast<Uint32>((actor->id & 0x0F) * 35);
}

inline int localCoopRealtimeAiActionSlice(Object* actor)
{
    if (actor == nullptr) {
        return 0;
    }

    int maxActionPoints = critterGetStat(actor, STAT_MAXIMUM_ACTION_POINTS);
    if (maxActionPoints <= 0) {
        return 0;
    }

    int hitMode = HIT_MODE_PUNCH;
    Object* weapon = critterGetItem2(actor);
    if (weapon != nullptr && itemGetType(weapon) == ITEM_TYPE_WEAPON) {
        hitMode = HIT_MODE_RIGHT_WEAPON_PRIMARY;
    }

    int actionPoints = weaponGetActionPointCost(actor, hitMode, false);
    if (actionPoints <= 0) {
        actionPoints = 2;
    }

    return std::max(1, std::min(actionPoints, maxActionPoints));
}

inline bool localCoopRealtimeAiActorCanAct(Object* actor)
{
    if (actor == nullptr
        || gDude == nullptr
        || localCoopActorIsHumanOwned(actor)
        || (actor->flags & OBJECT_HIDDEN) != 0
        || actor->elevation != gDude->elevation
        || (actor->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0) {
        return false;
    }

    return true;
}

inline Object* localCoopRealtimeAiFindNearestHuman(Object* actor)
{
    if (actor == nullptr) {
        return nullptr;
    }

    Object* best = nullptr;
    int bestDistance = 0x7FFFFFFF;
    for (const LocalCoopPlayer& player : gLocalCoopPlayers) {
        Object* candidate = player.actor;
        if (!player.humanOwned
            || candidate == nullptr
            || candidate->elevation != actor->elevation
            || (candidate->flags & OBJECT_HIDDEN) != 0
            || (candidate->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0) {
            continue;
        }

        int distance = objectGetDistanceBetween(actor, candidate);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = candidate;
        }
    }

    return best;
}

inline void localCoopRealtimeAiRegisterWorldActor(Object* actor, Object* preferredTarget)
{
    if (actor == nullptr
        || actor->id == -1
        || !localCoopRealtimeAiActorCanAct(actor)) {
        return;
    }

    Uint32 now = SDL_GetTicks();
    auto result = gLocalCoopRealtimeAiActors.emplace(actor->id, LocalCoopRealtimeAiActorState {});
    LocalCoopRealtimeAiActorState& state = result.first->second;
    if (result.second || state.nextActionTick == 0) {
        state.nextActionTick = now + localCoopRealtimeAiInitialStagger(actor);
    }
    state.preferredTargetId = preferredTarget != nullptr ? preferredTarget->id : -1;
    gLocalCoopRealtimeWorldCombatActive = true;
}

inline void localCoopRealtimeAiEngageHostile(Object* hostile, Object* preferredTarget)
{
    if (hostile == nullptr || preferredTarget == nullptr || gDude == nullptr) {
        return;
    }

    localCoopRealtimeAiRegisterWorldActor(hostile, preferredTarget);

    // Wake nearby members of the same hostile team so initiating a fight feels
    // like a realtime encounter instead of one NPC responding in isolation.
    Object** critters = nullptr;
    int count = objectListCreate(-1, hostile->elevation, OBJ_TYPE_CRITTER, &critters);
    if (count <= 0 || critters == nullptr) {
        return;
    }

    constexpr int kRealtimeTeamWakeDistance = 18;
    for (int index = 0; index < count; index++) {
        Object* candidate = critters[index];
        if (candidate == nullptr
            || candidate == hostile
            || candidate->data.critter.combat.team != hostile->data.critter.combat.team
            || localCoopActorIsHumanOwned(candidate)
            || objectGetDistanceBetween(hostile, candidate) > kRealtimeTeamWakeDistance) {
            continue;
        }
        localCoopRealtimeAiRegisterWorldActor(candidate, preferredTarget);
    }

    objectListFree(critters);
}

inline void localCoopRealtimeAiRegisterLegacyTurn(Object* actor, Object* preferredTarget)
{
    if (actor == nullptr) {
        return;
    }

    // Outside stock combat, legacy script-side AI calls retain their old
    // behavior. Explicit realtime encounters are registered through
    // localCoopRealtimeAiEngageHostile instead.
    if (!isInCombat() || !gLocalCoopInitialized) {
        combatAiStock(actor, preferredTarget);
        return;
    }

    // Human-owned companions never fall through to Fallout's combat AI even if
    // some legacy path accidentally attempts to schedule them.
    if (localCoopActorIsHumanOwned(actor)
        || (actor->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0) {
        actor->data.critter.combat.ap = 0;
        return;
    }

    if (actor->id == -1) {
        actor->data.critter.combat.ap = 0;
        return;
    }

    Uint32 now = SDL_GetTicks();
    auto result = gLocalCoopRealtimeAiActors.emplace(actor->id, LocalCoopRealtimeAiActorState {});
    LocalCoopRealtimeAiActorState& state = result.first->second;

    if (result.second || state.nextActionTick == 0) {
        state.nextActionTick = now + localCoopRealtimeAiInitialStagger(actor);
    }

    state.preferredTargetId = preferredTarget != nullptr ? preferredTarget->id : -1;

    // The old turn has done its scripts/stand-up/bookkeeping work, but it does
    // not get to spend the AP bar that `_combat_set_move_all` just assigned.
    actor->data.critter.combat.ap = 0;
}

inline void localCoopRealtimeAiRunLegacyActor(Object* actor,
    Object* preferredTarget,
    LocalCoopRealtimeAiActorState& state,
    Uint32 now)
{
    if (!localCoopRealtimeAiActorCanAct(actor) || animationIsBusy(actor)) {
        if (actor != nullptr) {
            actor->data.critter.combat.ap = 0;
        }
        return;
    }

    if (static_cast<Sint32>(now - state.nextActionTick) < 0) {
        actor->data.critter.combat.ap = 0;
        return;
    }

    int actionSlice = localCoopRealtimeAiActionSlice(actor);
    if (actionSlice <= 0) {
        actor->data.critter.combat.ap = 0;
        state.nextActionTick = now + 300;
        return;
    }

    actor->data.critter.combat.ap = actionSlice;
    combatAiStock(actor, preferredTarget);
    actor->data.critter.combat.ap = 0;
    state.nextActionTick = now + localCoopRealtimeAiCooldownForSlice(actionSlice);
}

inline void localCoopRealtimeAiRunWorldActor(Object* actor,
    Object* preferredTarget,
    LocalCoopRealtimeAiActorState& state,
    Uint32 now)
{
    if (!localCoopRealtimeAiActorCanAct(actor)) {
        return;
    }

    Object* target = preferredTarget;
    if (target == nullptr
        || !localCoopActorIsHumanOwned(target)
        || target->elevation != actor->elevation
        || (target->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0) {
        target = localCoopRealtimeAiFindNearestHuman(actor);
        state.preferredTargetId = target != nullptr ? target->id : -1;
    }

    if (target == nullptr || animationIsBusy(actor)) {
        return;
    }

    if (static_cast<Sint32>(now - state.nextActionTick) < 0) {
        return;
    }

    int hitMode = HIT_MODE_PUNCH;
    Object* weapon = critterGetItem2(actor);
    if (weapon != nullptr && itemGetType(weapon) == ITEM_TYPE_WEAPON) {
        hitMode = HIT_MODE_RIGHT_WEAPON_PRIMARY;
    }

    int actionSlice = localCoopRealtimeAiActionSlice(actor);
    if (actionSlice <= 0) {
        actionSlice = 2;
    }

    // AP is no longer a turn permission gate; it is only supplied so stock
    // attack/reload calculations have the budget they expect internally.
    actor->data.critter.combat.ap = std::max(20, actionSlice);
    int badShot = _combat_check_bad_shot(actor, target, hitMode, false);
    if (badShot == COMBAT_BAD_SHOT_NO_AMMO && weapon != nullptr) {
        weaponAttemptReload(actor, weapon);
        badShot = _combat_check_bad_shot(actor, target, hitMode, false);
    }

    if (badShot == COMBAT_BAD_SHOT_OK) {
        _combat_attack(actor, target, hitMode, HIT_LOCATION_UNCALLED);
        actor->data.critter.combat.ap = 0;
        state.nextActionTick = now + localCoopRealtimeAiCooldownForSlice(actionSlice);
        return;
    }

    actor->data.critter.combat.ap = 0;

    // Out of range, blocked shot, or melee target too far away: chase using the
    // normal Fallout pathfinder/animation system. This is asynchronous and does
    // not claim a combat turn.
    int moveRc = animationRegisterRunToObject(actor, target, -1, 0);
    state.nextActionTick = now + (moveRc == -1 ? 300 : 450);
}

inline void localCoopRealtimeAiTick()
{
    if (gLocalCoopRealtimeAiInsideTick) {
        return;
    }

    bool legacyCombat = isInCombat();
    if (!legacyCombat && !gLocalCoopRealtimeWorldCombatActive) {
        return;
    }

    gLocalCoopRealtimeAiInsideTick = true;
    Uint32 now = SDL_GetTicks();
    int liveWorldActors = 0;

    for (auto it = gLocalCoopRealtimeAiActors.begin(); it != gLocalCoopRealtimeAiActors.end();) {
        Object* actor = objectFindById(it->first);
        if (actor == nullptr
            || (actor->data.critter.combat.results & DAM_DEAD) != 0) {
            it = gLocalCoopRealtimeAiActors.erase(it);
            continue;
        }

        Object* preferredTarget = nullptr;
        if (it->second.preferredTargetId != -1) {
            preferredTarget = objectFindById(it->second.preferredTargetId);
        }

        if (legacyCombat) {
            localCoopRealtimeAiRunLegacyActor(actor, preferredTarget, it->second, now);
        } else {
            localCoopRealtimeAiRunWorldActor(actor, preferredTarget, it->second, now);
            if (localCoopRealtimeAiActorCanAct(actor)) {
                liveWorldActors++;
            }
        }
        ++it;
    }

    if (!legacyCombat && liveWorldActors == 0) {
        gLocalCoopRealtimeWorldCombatActive = false;
    }

    gLocalCoopRealtimeAiInsideTick = false;
}

inline bool localCoopRealtimeAiHasRegisteredActors()
{
    return !gLocalCoopRealtimeAiActors.empty();
}

inline void localCoopRealtimeAiInstall()
{
    gCombatAiRuntimeHandler = localCoopRealtimeAiRegisterLegacyTurn;
}

inline void localCoopRealtimeAiReset()
{
    gLocalCoopRealtimeAiActors.clear();
    gLocalCoopRealtimeCombatClockTick = SDL_GetTicks();
    gLocalCoopRealtimeAiInsideTick = false;
    gLocalCoopRealtimeWorldCombatActive = false;
}

inline void localCoopRealtimeCombatAdvanceTime(int legacyRoundSeconds)
{
    (void)legacyRoundSeconds;

    Uint32 now = SDL_GetTicks();
    if (gLocalCoopRealtimeCombatClockTick == 0) {
        gLocalCoopRealtimeCombatClockTick = now;
        return;
    }

    Uint32 elapsed = now - gLocalCoopRealtimeCombatClockTick;

    // A large gap means a previous combat ended and another started. Do not
    // accidentally count out-of-combat wall time as combat time.
    if (elapsed > 5000) {
        gLocalCoopRealtimeCombatClockTick = now;
        return;
    }

    int elapsedSeconds = static_cast<int>(elapsed / 1000);
    if (elapsedSeconds <= 0) {
        return;
    }

    gameTimeAddSeconds(elapsedSeconds);
    gLocalCoopRealtimeCombatClockTick += static_cast<Uint32>(elapsedSeconds * 1000);
}

} // namespace fallout

#endif /* LOCAL_COOP_AI_REALTIME_H */