#ifndef LOCAL_COOP_AI_REALTIME_H
#define LOCAL_COOP_AI_REALTIME_H

#include <SDL.h>

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "actions.h"
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

struct LocalCoopRealtimeAiActorState {
    Uint32 nextActionTick = 0;
    Uint32 nextApRegenTick = 0;
    int currentActionPointsHundredths = -1;
    int apRegenDelayTicks = 0;
    int preferredTargetId = -1;
};

inline std::unordered_map<int, LocalCoopRealtimeAiActorState> gLocalCoopRealtimeAiActors;
inline Uint32 gLocalCoopRealtimeCombatClockTick = 0;
inline bool gLocalCoopRealtimeAiInsideTick = false;
inline bool gLocalCoopRealtimeWorldCombatActive = false;

struct LocalCoopRealtimePursuer {
    int pid = -1;
    int hitPoints = 1;
    int combatResults = 0;
    int team = 0;
    int rotation = 0;
    int rightWeaponPid = -1;
    int rightWeaponAmmo = 0;
};

inline std::vector<LocalCoopRealtimePursuer> gLocalCoopRealtimePursuers;

inline constexpr int kLocalCoopRealtimeTeamWakeDistance = 18;
inline constexpr int kLocalCoopRealtimeMaximumPursuers = 24;
inline constexpr int kLocalCoopRealtimePursuitExitDistance = 8;
inline constexpr int kLocalCoopRealtimeMapWidth = 200;
inline constexpr int kLocalCoopRealtimeMapHeight = 200;
inline int gLocalCoopRealtimePursuitDirection = 0;
inline constexpr int kLocalCoopRealtimeDisengageDistance = 28;

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

inline int localCoopRealtimeAiMaximumApHundredths(Object* actor)
{
    if (actor == nullptr) {
        return 0;
    }

    return std::max(100, critterGetStat(actor, STAT_MAXIMUM_ACTION_POINTS) * 100);
}

inline int localCoopRealtimeAiRegenAmountHundredths(Object* actor)
{
    if (actor == nullptr) {
        return 0;
    }

    // Directly normalized from cte_APRegenNPC in ap_regen.fos:
    // 360 + 44 * Agility fixed-point AP every real second.
    return std::max(1, 360 + 44 * critterGetStat(actor, STAT_AGILITY));
}

inline void localCoopRealtimeAiUpdateActionPoints(Object* actor,
    LocalCoopRealtimeAiActorState& state,
    Uint32 now)
{
    int maximum = localCoopRealtimeAiMaximumApHundredths(actor);
    if (state.currentActionPointsHundredths < 0) {
        state.currentActionPointsHundredths = maximum;
        state.nextApRegenTick = now + 1000;
        state.apRegenDelayTicks = 0;
        return;
    }

    state.currentActionPointsHundredths = std::min(state.currentActionPointsHundredths, maximum);
    int processedTicks = 0;
    while (static_cast<Sint32>(now - state.nextApRegenTick) >= 0 && processedTicks < 10) {
        if (state.apRegenDelayTicks > 0) {
            state.apRegenDelayTicks--;
        } else if (state.currentActionPointsHundredths < maximum) {
            state.currentActionPointsHundredths = std::min(maximum,
                state.currentActionPointsHundredths + localCoopRealtimeAiRegenAmountHundredths(actor));
        }
        state.nextApRegenTick += 1000;
        processedTicks++;
    }

    if (processedTicks == 10 && static_cast<Sint32>(now - state.nextApRegenTick) >= 0) {
        state.nextApRegenTick = now + 1000;
    }
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
    if (actor == nullptr || actor->id == -1 || !localCoopRealtimeAiActorCanAct(actor)) {
        return;
    }

    Uint32 now = SDL_GetTicks();
    auto result = gLocalCoopRealtimeAiActors.emplace(actor->id, LocalCoopRealtimeAiActorState {});
    LocalCoopRealtimeAiActorState& state = result.first->second;
    if (result.second || state.nextActionTick == 0) {
        state.nextActionTick = now + localCoopRealtimeAiInitialStagger(actor);
    }
    if (result.second || state.currentActionPointsHundredths < 0) {
        state.currentActionPointsHundredths = localCoopRealtimeAiMaximumApHundredths(actor);
        state.nextApRegenTick = now + 1000;
        state.apRegenDelayTicks = 0;
    }
    state.preferredTargetId = preferredTarget != nullptr ? preferredTarget->id : -1;

    gLocalCoopRealtimeWorldCombatActive = true;
    localCoopDangerSetLiveHostiles(static_cast<int>(gLocalCoopRealtimeAiActors.size()));
}

inline void localCoopRealtimeAiEngageHostile(Object* hostile, Object* preferredTarget)
{
    if (hostile == nullptr || preferredTarget == nullptr || gDude == nullptr) {
        return;
    }

    localCoopRealtimeAiRegisterWorldActor(hostile, preferredTarget);

    Object** critters = nullptr;
    int count = objectListCreate(-1, hostile->elevation, OBJ_TYPE_CRITTER, &critters);
    if (count <= 0 || critters == nullptr) {
        return;
    }

    for (int index = 0; index < count; index++) {
        Object* candidate = critters[index];
        if (candidate == nullptr
            || candidate == hostile
            || candidate->data.critter.combat.team != hostile->data.critter.combat.team
            || localCoopActorIsHumanOwned(candidate)
            || objectGetDistanceBetween(hostile, candidate) > kLocalCoopRealtimeTeamWakeDistance) {
            continue;
        }
        localCoopRealtimeAiRegisterWorldActor(candidate, preferredTarget);
    }

    objectListFree(critters);
    localCoopDangerSetLiveHostiles(static_cast<int>(gLocalCoopRealtimeAiActors.size()));
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
    }

    return 0;
}

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

    // The stock combat loop can still reach combatAi while a legacy caller is
    // being broken back into the realtime world. Convert the NPC into a realtime
    // actor, then consume its entire legacy turn immediately. Leaving AP here
    // causes _combat_turn to call combatAi again and again ("extra APs"), which
    // floods attacks and can keep the human actor permanently animation-locked.
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

    localCoopRealtimeAiUpdateActionPoints(actor, state, now);

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

    int actionCostHundredths = actionSlice * 100;
    if (state.currentActionPointsHundredths < actionCostHundredths) {
        // A critter with an empty AP pool waits for the timed regeneration event
        // instead of re-entering stock AI over and over.
        state.nextActionTick = now + 100;
        return;
    }

    int savedActionPoints = actor->data.critter.combat.ap;
    actor->data.critter.combat.ap = std::max(20, actionSlice);
    int badShot = _combat_check_bad_shot(actor, target, hitMode, false);
    if (badShot == COMBAT_BAD_SHOT_NO_AMMO && weapon != nullptr) {
        weaponAttemptReload(actor, weapon);
        badShot = _combat_check_bad_shot(actor, target, hitMode, false);
    }

    if (badShot == COMBAT_BAD_SHOT_OK) {
        int attackRc = _combat_attack(actor, target, hitMode, HIT_LOCATION_UNCALLED);
        actor->data.critter.combat.ap = savedActionPoints;
        if (attackRc == 0) {
            state.currentActionPointsHundredths = std::max(0,
                state.currentActionPointsHundredths - actionCostHundredths);
            state.apRegenDelayTicks = std::max(state.apRegenDelayTicks, 1);
            state.nextActionTick = now + localCoopRealtimeAiCooldownForSlice(actionSlice);
            debugPrint("[COOP REALTIME AI] actorId=%d attack cost=%d ap=%d next=%u\n",
                actor->id,
                actionCostHundredths,
                state.currentActionPointsHundredths,
                state.nextActionTick);
        } else {
            state.nextActionTick = now + 250;
            debugPrint("[COOP REALTIME AI] actorId=%d attack-sequence-failed rc=%d\n",
                actor->id,
                attackRc);
        }
        return;
    }

    actor->data.critter.combat.ap = savedActionPoints;

    constexpr int kMoveCostHundredths = 100;
    if (state.currentActionPointsHundredths < kMoveCostHundredths) {
        state.nextActionTick = now + 100;
        return;
    }

    int moveRc = -1;
    if (reg_anim_begin(ANIMATION_REQUEST_UNRESERVED | ANIMATION_REQUEST_INSIGNIFICANT) != -1) {
        // Use stock walking for generic critters. Several small-creature FRMs
        // have no valid run sequence; requesting one can visually snap them
        // across the map.
        moveRc = animationRegisterMoveToObject(actor, target, -1, 0);
        if (moveRc == -1) {
            reg_anim_clear(actor);
        } else {
            reg_anim_end();
        }
    }
    if (moveRc != -1) {
        state.currentActionPointsHundredths = std::max(0,
            state.currentActionPointsHundredths - kMoveCostHundredths);
    }
    state.nextActionTick = now + (moveRc == -1 ? 300 : 450);
}

inline bool localCoopRealtimeAiThreatStillEngaged(Object* actor)
{
    if (!localCoopRealtimeAiActorCanAct(actor)) {
        return false;
    }

    Object* target = localCoopRealtimeAiFindNearestHuman(actor);
    if (target == nullptr) {
        return false;
    }

    int distance = objectGetDistanceBetween(actor, target);
    if (distance <= kLocalCoopRealtimeDisengageDistance) {
        return true;
    }

    return _can_see(actor, target);
}

inline void localCoopRealtimeAiTick()
{
    if (gLocalCoopRealtimeAiInsideTick) {
        return;
    }

    if (!gLocalCoopDangerActive && gLocalCoopRealtimeAiActors.empty()) {
        return;
    }

    gLocalCoopRealtimeAiInsideTick = true;
    Uint32 now = SDL_GetTicks();
    int liveWorldActors = 0;

    for (auto it = gLocalCoopRealtimeAiActors.begin(); it != gLocalCoopRealtimeAiActors.end();) {
        Object* actor = objectFindById(it->first);
        if (actor == nullptr
            || (actor->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0
            || !localCoopRealtimeAiThreatStillEngaged(actor)) {
            it = gLocalCoopRealtimeAiActors.erase(it);
            continue;
        }

        Object* preferredTarget = nullptr;
        if (it->second.preferredTargetId != -1) {
            preferredTarget = objectFindById(it->second.preferredTargetId);
        }

        localCoopRealtimeAiRunWorldActor(actor, preferredTarget, it->second, now);
        liveWorldActors++;
        ++it;
    }

    localCoopDangerSetLiveHostiles(liveWorldActors);
    gLocalCoopRealtimeWorldCombatActive = liveWorldActors > 0;

    // Expire the post-damage flag on wall-clock time. AI registration, movement
    // and attacks that miss or deal zero damage do not refresh this cooldown.
    (void)localCoopDangerBlocksMapExit();

    gLocalCoopRealtimeAiInsideTick = false;
}

inline void localCoopRealtimeAiCapturePursuers(int exitTile, int roadDirection)
{
    gLocalCoopRealtimePursuers.clear();
    gLocalCoopRealtimePursuitDirection = roadDirection;

    for (const auto& entry : gLocalCoopRealtimeAiActors) {
        if (static_cast<int>(gLocalCoopRealtimePursuers.size())
            >= kLocalCoopRealtimeMaximumPursuers) {
            break;
        }

        Object* actor = objectFindById(entry.first);
        if (actor == nullptr
            || !localCoopRealtimeAiThreatStillEngaged(actor)
            || !tileIsValid(exitTile)
            || tileDistanceBetween(actor->tile, exitTile)
                > kLocalCoopRealtimePursuitExitDistance) {
            continue;
        }

        LocalCoopRealtimePursuer pursuer;
        pursuer.pid = actor->pid;
        pursuer.hitPoints = std::max(1, actor->data.critter.hp);
        pursuer.combatResults = actor->data.critter.combat.results
            & ~(DAM_DEAD | DAM_KNOCKED_OUT);
        pursuer.team = actor->data.critter.combat.team;
        pursuer.rotation = actor->rotation;

        Object* weapon = critterGetItem2(actor);
        if (weapon != nullptr && itemGetType(weapon) == ITEM_TYPE_WEAPON) {
            pursuer.rightWeaponPid = weapon->pid;
            pursuer.rightWeaponAmmo = ammoGetQuantity(weapon);
        }

        gLocalCoopRealtimePursuers.push_back(pursuer);

        // Remove the pursuer before the current map is saved. It now belongs to
        // the road pursuit and must not also remain behind as a duplicate.
        reg_anim_clear(actor);
        objectDestroy(actor, nullptr);
    }

    debugPrint("[COOP PURSUIT] captured=%d exitTile=%d direction=%d radius=%d\n",
        static_cast<int>(gLocalCoopRealtimePursuers.size()),
        exitTile,
        roadDirection,
        kLocalCoopRealtimePursuitExitDistance);
    gLocalCoopRealtimeAiActors.clear();
    gLocalCoopRealtimeWorldCombatActive = false;
    localCoopDangerSetLiveHostiles(0);
}

inline int localCoopRealtimeAiFindPursuitEntryTile(
    Object* actor,
    int ordinal)
{
    if (actor == nullptr || gDude == nullptr || !tileIsValid(gDude->tile)) {
        return -1;
    }

    int partyX = gDude->tile % kLocalCoopRealtimeMapWidth;
    int partyY = gDude->tile / kLocalCoopRealtimeMapWidth;
    for (int depth = 3; depth <= 14; depth++) {
        for (int spread = 0; spread <= 16; spread++) {
            int signedSpread = spread == 0
                ? 0
                : ((spread + ordinal) % 2 == 0 ? spread : -spread);
            int x = partyX;
            int y = partyY;
            switch (gLocalCoopRealtimePursuitDirection) {
            case 0: // Party travelled north; pursuers enter from the south.
                x = std::max(1, std::min(
                    kLocalCoopRealtimeMapWidth - 2,
                    partyX + signedSpread));
                y = kLocalCoopRealtimeMapHeight - 1 - depth;
                break;
            case 1: // East; pursuers enter from the west.
                x = depth;
                y = std::max(1, std::min(
                    kLocalCoopRealtimeMapHeight - 2,
                    partyY + signedSpread));
                break;
            case 2: // South; pursuers enter from the north.
                x = std::max(1, std::min(
                    kLocalCoopRealtimeMapWidth - 2,
                    partyX + signedSpread));
                y = depth;
                break;
            case 3: // West; pursuers enter from the east.
                x = kLocalCoopRealtimeMapWidth - 1 - depth;
                y = std::max(1, std::min(
                    kLocalCoopRealtimeMapHeight - 2,
                    partyY + signedSpread));
                break;
            default:
                break;
            }

            int tile = y * kLocalCoopRealtimeMapWidth + x;
            if (tileIsValid(tile)
                && _obj_blocking_at(actor, tile, gDude->elevation) == nullptr) {
                return tile;
            }
        }
    }

    return localCoopFindSpawnTile(gDude, 12);
}

inline void localCoopRealtimeAiRestorePursuers()
{
    if (gDude == nullptr || gLocalCoopRealtimePursuers.empty()) {
        return;
    }

    int restored = 0;
    int ordinal = 0;
    for (const LocalCoopRealtimePursuer& pursuer : gLocalCoopRealtimePursuers) {
        Object* actor = nullptr;
        if (pursuer.pid == -1
            || objectCreateWithPid(&actor, pursuer.pid) == -1
            || actor == nullptr) {
            continue;
        }

        actor->flags &= ~OBJECT_HIDDEN;
        actor->data.critter.combat.results = pursuer.combatResults;
        actor->data.critter.combat.team = pursuer.team;
        actor->rotation = pursuer.rotation;
        critterUpdateDerivedStats(actor);
        actor->data.critter.hp = std::min(
            pursuer.hitPoints,
            critterGetStat(actor, STAT_MAXIMUM_HIT_POINTS));
        actor->data.critter.combat.ap =
            critterGetStat(actor, STAT_MAXIMUM_ACTION_POINTS);

        int spawnTile = localCoopRealtimeAiFindPursuitEntryTile(actor, ordinal++);
        if (spawnTile == -1
            || objectSetLocation(actor, spawnTile, gDude->elevation, nullptr) == -1) {
            objectDestroy(actor, nullptr);
            continue;
        }

        if (pursuer.rightWeaponPid != -1) {
            Object* weapon = nullptr;
            if (objectCreateWithPid(&weapon, pursuer.rightWeaponPid) == 0
                && weapon != nullptr) {
                if (ammoGetCapacity(weapon) > 0) {
                    ammoSetQuantity(weapon, std::max(0, pursuer.rightWeaponAmmo));
                }
                if (itemAdd(actor, weapon, 1) == 0) {
                    _invenWieldFunc(actor, weapon, HAND_RIGHT, false);
                } else {
                    objectDestroy(weapon, nullptr);
                }
            }
        }

        localCoopRealtimeAiRegisterWorldActor(actor, gDude);
        restored++;
    }

    debugPrint("[COOP PURSUIT] restored=%d requested=%d map=%d\n",
        restored,
        static_cast<int>(gLocalCoopRealtimePursuers.size()),
        gMapHeader.field_34);
    gLocalCoopRealtimePursuers.clear();
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
    gLocalCoopRealtimePursuers.clear();
    gLocalCoopRealtimePursuitDirection = 0;
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
