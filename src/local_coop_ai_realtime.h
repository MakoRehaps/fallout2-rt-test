#ifndef LOCAL_COOP_AI_REALTIME_H
#define LOCAL_COOP_AI_REALTIME_H

#include <SDL.h>

#include <algorithm>
#include <unordered_map>

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

// Legacy combat still owns encounter setup, combat scripts, stand-up handling,
// sequence bookkeeping, death cleanup, and end conditions. It no longer grants
// NPCs permission to run their AI. The legacy `_combat_ai` call registers the
// actor here; this ticker gives each registered NPC its own independent
// wall-clock action schedule and invokes the untouched Fallout AI brain with
// only one action-sized AP budget at a time.
struct LocalCoopRealtimeAiActorState {
    Uint32 nextActionTick = 0;
    int preferredTargetId = -1;
};

inline std::unordered_map<int, LocalCoopRealtimeAiActorState> gLocalCoopRealtimeAiActors;
inline Uint32 gLocalCoopRealtimeCombatClockTick = 0;
inline bool gLocalCoopRealtimeAiInsideTick = false;

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

inline void localCoopRealtimeAiRegisterLegacyTurn(Object* actor, Object* preferredTarget)
{
    if (actor == nullptr) {
        return;
    }

    // Outside an active co-op combat, preserve Fallout's stock behavior. This
    // also makes the dispatcher safe if some script calls the AI directly.
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
        // Extremely defensive fallback for temporary/script-created actors
        // without a stable object ID. Do not leave them with a full legacy AP
        // bar, but also do not store a raw pointer that might be freed later.
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

inline void localCoopRealtimeAiRunActor(Object* actor,
    Object* preferredTarget,
    LocalCoopRealtimeAiActorState& state,
    Uint32 now)
{
    if (!localCoopRealtimeAiActorCanAct(actor) || animationIsBusy(actor)) {
        actor->data.critter.combat.ap = 0;
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

    // This is the original Fallout AI implementation. The only difference is
    // that it is invoked from the realtime ticker, not by turn ownership, and
    // receives enough AP for roughly one meaningful action.
    combatAiStock(actor, preferredTarget);

    actor->data.critter.combat.ap = 0;
    state.nextActionTick = now + localCoopRealtimeAiCooldownForSlice(actionSlice);
}

inline void localCoopRealtimeAiTick()
{
    if (gLocalCoopRealtimeAiInsideTick || !isInCombat()) {
        return;
    }

    gLocalCoopRealtimeAiInsideTick = true;
    Uint32 now = SDL_GetTicks();

    for (auto it = gLocalCoopRealtimeAiActors.begin(); it != gLocalCoopRealtimeAiActors.end();) {
        Object* actor = objectFindById(it->first);
        if (actor == nullptr) {
            it = gLocalCoopRealtimeAiActors.erase(it);
            continue;
        }

        Object* preferredTarget = nullptr;
        if (it->second.preferredTargetId != -1) {
            preferredTarget = objectFindById(it->second.preferredTargetId);
        }

        localCoopRealtimeAiRunActor(actor, preferredTarget, it->second, now);
        ++it;
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
