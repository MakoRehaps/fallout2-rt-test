#ifndef LOCAL_COOP_AI_REALTIME_H
#define LOCAL_COOP_AI_REALTIME_H

#include <SDL.h>

#include <algorithm>
#include <unordered_map>

#include "combat_ai.h"
#include "critter.h"
#include "inventory.h"
#include "item.h"
#include "local_coop.h"
#include "proto_types.h"
#include "stat.h"

namespace fallout {

// The legacy combat loop still owns encounter setup, scripts, death handling,
// and cleanup, but NPCs no longer get to spend an entire AP bar in one burst.
// Each critter receives one action-sized AP slice when its realtime cooldown is
// ready. Rounds become a lightweight scheduler heartbeat instead of turns.
inline std::unordered_map<Object*, Uint32> gLocalCoopRealtimeAiNextTick;

inline void localCoopRealtimeAiReset()
{
    gLocalCoopRealtimeAiNextTick.clear();
}

inline Uint32 localCoopRealtimeAiCooldownForSlice(int actionPoints)
{
    int cooldown = actionPoints * 120;
    cooldown = std::max(300, std::min(cooldown, 1000));
    return static_cast<Uint32>(cooldown);
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

inline void localCoopRealtimeAiTurn(Object* actor, Object* preferredTarget)
{
    if (actor == nullptr) {
        return;
    }

    if (localCoopActorIsHumanOwned(actor)
        || (actor->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0) {
        actor->data.critter.combat.ap = 0;
        return;
    }

    Uint32 now = SDL_GetTicks();
    auto it = gLocalCoopRealtimeAiNextTick.find(actor);
    if (it == gLocalCoopRealtimeAiNextTick.end()) {
        // Stagger the first action slightly so a room full of enemies does not
        // all fire on the exact same scheduler heartbeat.
        Uint32 stagger = 80 + static_cast<Uint32>((actor->id & 0x7) * 25);
        gLocalCoopRealtimeAiNextTick.emplace(actor, now + stagger);
        actor->data.critter.combat.ap = 0;
        return;
    }

    if (static_cast<Sint32>(now - it->second) < 0 || animationIsBusy(actor)) {
        actor->data.critter.combat.ap = 0;
        return;
    }

    int actionSlice = localCoopRealtimeAiActionSlice(actor);
    if (actionSlice <= 0) {
        actor->data.critter.combat.ap = 0;
        it->second = now + 300;
        return;
    }

    actor->data.critter.combat.ap = actionSlice;

    // Call Fallout's original AI brain with only enough AP for roughly one
    // meaningful action. Movement, target choice, scripts, reload behavior and
    // attack calculations stay stock; the pacing is now realtime.
    _combat_ai(actor, preferredTarget);

    actor->data.critter.combat.ap = 0;
    it->second = now + localCoopRealtimeAiCooldownForSlice(actionSlice);
}

} // namespace fallout

#endif /* LOCAL_COOP_AI_REALTIME_H */
