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
#include "local_coop_danger.h"
#include "object.h"
#include "proto_types.h"
#include "scripts.h"
#include "stat.h"

namespace fallout {

// The fused co-op game has no combat/non-combat phase. Hostiles are registered
// here when a script or player attack starts an encounter, then act on independent
// wall-clock schedules in the normal live world. `local_coop_danger.h` tracks
// only whether map exits should be locked; it never changes movement/AP/UI mode.
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
    localCoopDangerBegin();
}

inline void localCoopRealtimeAiEngageHostile(Object* hostile, Object* preferredTarget)
{
    if (hostile == nullptr || preferredTarget == nullptr || gDude == nullptr) {
        return;
    }

    localCoopRealtimeAiRegisterWorldActor(hostile, preferredTarget);

    // Wake nearby members of the same team. This is encounter propagation only;
    // it does not create turns or alter the player's movement state.
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

inline Object* localCoopRealtimeAiFindScriptHostile(Object* preferredTarget)
{
    if (gDude == nullptr) {
        return nullptr;
    }

    Object* human = preferredTarget != nullptr && localCoopActorIsHumanOwned(preferredTarget)
        ? preferredTarget
        : gDude;

    Object** critters = nullptr;
    int count = objectListCreate(-1, human->elevation, OBJ_TYPE_CRITTER, &critters);
    if (count <= 0 || critters == nullptr) {
        return nullptr;
    }

    Object* best = nullptr;
    int bestDistance = 0x7FFFFFFF;
    for (int index = 0; index < count; index++) {
        Object* candidate = critters[index];
        if (!localCoopRealtimeAiActorCanAct(candidate)
            || candidate->data.critter.combat.team == human->data.critter.combat.team) {
            continue;
        }

        int distance = objectGetDistanceBetween(candidate, human);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = candidate;
        }
    }

    objectListFree(critters);
    return best;
}

// Script attack opcodes call this instead of queuing SCRIPT_REQUEST_COMBAT.
// The exact same attacker/defender intent becomes a realtime engagement and the
// interpreter continues running in the normal world immediately.
inline int localCoopRealtimeAiHandleScriptCombatRequest(CombatStartData* combat)
{
    if (!gLocalCoopInitialized || gDude == nullptr) {
        return -1;
    }

    Object* attacker = combat != nullptr ? combat->attacker : nullptr;
    Object* defender = combat != nullptr ? combat->defender : nullptr;

    Object* hostile = nullptr;
    Object* humanTarget = nullptr;

    if (attacker != nullptr && !localCoopActorIsHumanOwned(attacker)) {
        hostile = attacker;
        humanTarget = defender != nullptr && localCoopActorIsHumanOwned(defender)
            ? defender
            : localCoopRealtimeAiFindNearestHuman(attacker);
    } else if (defender != nullptr && !localCoopActorIsHumanOwned(defender)) {
        hostile = defender;
        humanTarget = attacker != nullptr && localCoopActorIsHumanOwned(attacker)
            ? attacker
            : localCoopRealtimeAiFindNearestHuman(defender);
    }

    if (hostile == nullptr) {
        humanTarget = attacker != nullptr && localCoopActorIsHumanOwned(attacker)
            ? attacker
            : (defender != nullptr && localCoopActorIsHumanOwned(defender) ? defender : gDude);
        hostile = localCoopRealtimeAiFindScriptHostile(humanTarget);
    }

    if (hostile != nullptr && humanTarget != nullptr) {
        localCoopRealtimeAiEngageHostile(hostile, humanTarget);
    } else {
        // A combat request without a resolvable pair still becomes danger rather
        // than entering the stock turn loop. It will clear once no hostile actor
        // is registered by the normal critter/script update path.
        localCoopDangerBegin();
        gLocalCoopRealtimeWorldCombatActive = true;
    }

    return 0;
}

// Compatibility bridge retained for any old combat AI dispatch that still gets
// reached by a save/script while conversion continues. New encounters do not use
// this path and never require isInCombat().
inline void localCoopRealtimeAiRegisterLegacyTurn(Object* actor, Object* preferredTarget)
{
    if (actor == nullptr) {
        return;
    }

    if (!isInCombat() || !gLocalCoopInitialized) {
        if (gLocalCoopInitialized && localCoopRealtimeAiActorCanAct(actor)) {
            localCoopRealtimeAiRegisterWorldActor(actor,
                preferredTarget != nullptr ? preferredTarget : localCoopRealtimeAiFindNearestHuman(actor));
            return;
        }
        combatAiStock(actor, preferredTarget);
        return;
    }

    if (localCoopActorIsHumanOwned(actor)
        || (actor->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0) {
        actor->data.critter.combat.ap = 0;
        return;
    }

    localCoopRealtimeAiRegisterWorldActor(actor,
        preferredTarget != nullptr ? preferredTarget : localCoopRealtimeAiFindNearestHuman(actor));
    actor->data.critter.combat.ap = 0;
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

    // AP exists only as a temporary compatibility budget for the original
    // attack/reload calculations. It never grants or removes a turn.
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
        localCoopDangerTouch();
        return;
    }

    actor->data.critter.combat.ap = 0;

    int moveRc = -1;
    if (reg_anim_begin(ANIMATION_REQUEST_UNRESERVED | ANIMATION_REQUEST_INSIGNIFICANT) != -1) {
        moveRc = animationRegisterRunToObject(actor, target, -1, 0);
        if (moveRc == -1) {
            reg_anim_clear(actor);
        } else {
            reg_anim_end();
        }
    }
    state.nextActionTick = now + (moveRc == -1 ? 300 : 450);
    localCoopDangerTouch();
}

inline void localCoopRealtimeAiTick()
{
    if (gLocalCoopRealtimeAiInsideTick || !gLocalCoopDangerActive) {
        return;
    }

    gLocalCoopRealtimeAiInsideTick = true;
    Uint32 now = SDL_GetTicks();
    int liveWorldActors = 0;

    for (auto it = gLocalCoopRealtimeAiActors.begin(); it != gLocalCoopRealtimeAiActors.end();) {
        Object* actor = objectFindById(it->first);
        if (actor == nullptr
            || (actor->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0) {
            it = gLocalCoopRealtimeAiActors.erase(it);
            continue;
        }

        Object* preferredTarget = nullptr;
        if (it->second.preferredTargetId != -1) {
            preferredTarget = objectFindById(it->second.preferredTargetId);
        }

        localCoopRealtimeAiRunWorldActor(actor, preferredTarget, it->second, now);
        if (localCoopRealtimeAiActorCanAct(actor)) {
            liveWorldActors++;
        }
        ++it;
    }

    if (liveWorldActors == 0) {
        gLocalCoopRealtimeWorldCombatActive = false;
        localCoopDangerEnd();
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
    gScriptCombatRequestRuntimeHandler = localCoopRealtimeAiHandleScriptCombatRequest;
}

inline void localCoopRealtimeAiReset()
{
    gLocalCoopRealtimeAiActors.clear();
    gLocalCoopRealtimeCombatClockTick = SDL_GetTicks();
    gLocalCoopRealtimeAiInsideTick = false;
    gLocalCoopRealtimeWorldCombatActive = false;
    localCoopDangerEnd();
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
