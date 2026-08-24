#ifndef LOCAL_COOP_FOCUS_H
#define LOCAL_COOP_FOCUS_H

#include <algorithm>
#include <array>
#include <cmath>

#include "actions.h"
#include "combat.h"
#include "item.h"
#include "local_coop.h"
#include "object.h"
#include "proto_types.h"
#include "tile.h"

namespace fallout {

struct LocalCoopFocusSlot {
    Object* combatTarget = nullptr;
    Object* interactionTarget = nullptr;
    Object* outlinedTarget = nullptr;
    int savedOutline = 0;
};

inline std::array<LocalCoopFocusSlot, kLocalCoopMaxPlayers> gLocalCoopFocusSlots;

inline int localCoopFocusRotationDifference(int lhs, int rhs)
{
    int difference = std::abs(lhs - rhs) % ROTATION_COUNT;
    return std::min(difference, ROTATION_COUNT - difference);
}

inline int localCoopFocusCombatRange(const Object* actor)
{
    if (actor == nullptr) {
        return 4;
    }

    Object* mutableActor = const_cast<Object*>(actor);
    int hitMode = HIT_MODE_PUNCH;
    Object* weapon = critterGetItem2(mutableActor);
    if (weapon != nullptr && itemGetType(weapon) == ITEM_TYPE_WEAPON) {
        hitMode = HIT_MODE_RIGHT_WEAPON_PRIMARY;
    }

    int weaponRange = weaponGetRange(mutableActor, hitMode);
    if (weaponRange < 1) {
        weaponRange = 1;
    }

    return std::max(4, std::min(60, weaponRange + 2));
}

inline bool localCoopFocusTargetStillUsable(const Object* actor, const Object* target, int maxDistance)
{
    return actor != nullptr
        && target != nullptr
        && actor != target
        && target->elevation == actor->elevation
        && (target->flags & OBJECT_HIDDEN) == 0
        && objectGetDistanceBetween(const_cast<Object*>(actor), const_cast<Object*>(target)) <= maxDistance;
}

inline bool localCoopFocusIsEnemy(const Object* actor, const Object* target)
{
    if (!localCoopFocusTargetStillUsable(actor, target, localCoopFocusCombatRange(actor))) {
        return false;
    }

    if (PID_TYPE(target->pid) != OBJ_TYPE_CRITTER) {
        return false;
    }

    if ((target->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0) {
        return false;
    }

    if (localCoopActorIsHumanOwned(target)) {
        return false;
    }

    if (target->data.critter.combat.team == actor->data.critter.combat.team) {
        return false;
    }

    return _can_see(const_cast<Object*>(actor), const_cast<Object*>(target));
}

inline bool localCoopFocusIsInteractable(const Object* actor, const Object* target)
{
    if (!localCoopFocusTargetStillUsable(actor, target, 12)) {
        return false;
    }

    int type = PID_TYPE(target->pid);
    return type == OBJ_TYPE_CRITTER
        || type == OBJ_TYPE_ITEM
        || type == OBJ_TYPE_SCENERY;
}

inline bool localCoopFocusOtherSlotUsesTarget(int slot, Object* target)
{
    if (target == nullptr) {
        return false;
    }

    for (int index = 0; index < kLocalCoopMaxPlayers; index++) {
        if (index != slot && gLocalCoopFocusSlots[index].outlinedTarget == target) {
            return true;
        }
    }

    return false;
}

inline void localCoopFocusReleaseOutline(int slot)
{
    if (slot < 0 || slot >= kLocalCoopMaxPlayers) {
        return;
    }

    LocalCoopFocusSlot& focus = gLocalCoopFocusSlots[slot];
    Object* object = focus.outlinedTarget;
    if (object == nullptr) {
        return;
    }

    if (localCoopFocusOtherSlotUsesTarget(slot, object)) {
        focus.outlinedTarget = nullptr;
        focus.savedOutline = 0;
        return;
    }

    objectDisableOutline(object, nullptr);
    objectClearOutline(object, nullptr);

    int oldType = focus.savedOutline & OUTLINE_TYPE_MASK;
    if (oldType != 0) {
        objectSetOutline(object, oldType, nullptr);
        if ((focus.savedOutline & OUTLINE_DISABLED) == 0) {
            objectEnableOutline(object, nullptr);
        }
    }

    focus.outlinedTarget = nullptr;
    focus.savedOutline = 0;
}

inline void localCoopFocusApplyOutline(int slot, Object* target, bool hostile)
{
    if (slot < 0 || slot >= kLocalCoopMaxPlayers) {
        return;
    }

    LocalCoopFocusSlot& focus = gLocalCoopFocusSlots[slot];
    if (focus.outlinedTarget == target) {
        return;
    }

    localCoopFocusReleaseOutline(slot);

    if (target == nullptr || (target->flags & OBJECT_NO_HIGHLIGHT) != 0) {
        return;
    }

    for (int index = 0; index < kLocalCoopMaxPlayers; index++) {
        if (index != slot && gLocalCoopFocusSlots[index].outlinedTarget == target) {
            focus.outlinedTarget = target;
            focus.savedOutline = 0;
            return;
        }
    }

    focus.savedOutline = target->outline;
    if ((target->outline & OUTLINE_TYPE_MASK) != 0) {
        objectDisableOutline(target, nullptr);
        objectClearOutline(target, nullptr);
    }

    int outlineType = hostile ? OUTLINE_TYPE_HOSTILE : OUTLINE_TYPE_ITEM;
    if (objectSetOutline(target, outlineType, nullptr) == 0) {
        objectEnableOutline(target, nullptr);
        focus.outlinedTarget = target;
    }
}

// Return a continuous screen-space angular error between the right stick and a
// candidate object. The old implementation reduced the stick to one of six hex
// rotations, which made targets snap across a very wide wedge. This keeps the
// hex world for movement while aiming behaves like a twin-stick action game.
inline double localCoopFocusAimError(const LocalCoopPlayer& player, const Object* target)
{
    if (player.actor == nullptr || target == nullptr || (player.aimX == 0 && player.aimY == 0)) {
        return 0.0;
    }

    int actorX = 0;
    int actorY = 0;
    int targetX = 0;
    int targetY = 0;
    if (tileToScreenXY(player.actor->tile, &actorX, &actorY, player.actor->elevation) != 0
        || tileToScreenXY(target->tile, &targetX, &targetY, target->elevation) != 0) {
        return 3.14159265358979323846;
    }

    double aimAngle = std::atan2(static_cast<double>(player.aimY), static_cast<double>(player.aimX));
    double targetAngle = std::atan2(static_cast<double>(targetY - actorY), static_cast<double>(targetX - actorX));
    double difference = std::fabs(aimAngle - targetAngle);
    if (difference > 3.14159265358979323846) {
        difference = 6.28318530717958647692 - difference;
    }
    return difference;
}

inline Object* localCoopFocusFindEnemy(LocalCoopPlayer& player)
{
    Object* actor = player.actor;
    if (actor == nullptr) {
        return nullptr;
    }

    LocalCoopFocusSlot& focus = gLocalCoopFocusSlots[player.slot];
    bool activelyAiming = player.aimX != 0 || player.aimY != 0;

    if (!activelyAiming && localCoopFocusIsEnemy(actor, focus.combatTarget)) {
        return focus.combatTarget;
    }

    Object** critters = nullptr;
    int count = objectListCreate(-1, actor->elevation, OBJ_TYPE_CRITTER, &critters);
    if (count <= 0 || critters == nullptr) {
        focus.combatTarget = nullptr;
        return nullptr;
    }

    Object* best = nullptr;
    double bestScore = 1.0e30;
    constexpr double kAimAcquireHalfAngle = 0.72; // about 41 degrees

    for (int index = 0; index < count; index++) {
        Object* candidate = critters[index];
        if (!localCoopFocusIsEnemy(actor, candidate)) {
            continue;
        }

        double angularError = 0.0;
        if (activelyAiming) {
            angularError = localCoopFocusAimError(player, candidate);
            if (angularError > kAimAcquireHalfAngle) {
                continue;
            }
        }

        int distance = objectGetDistanceBetween(actor, candidate);
        double score = angularError * 1000.0 + static_cast<double>(distance);
        if (score < bestScore) {
            bestScore = score;
            best = candidate;
        }
    }

    objectListFree(critters);
    focus.combatTarget = best;
    return best;
}

inline Object* localCoopFocusFindInteractable(LocalCoopPlayer& player)
{
    Object* actor = player.actor;
    if (actor == nullptr) {
        return nullptr;
    }

    LocalCoopFocusSlot& focus = gLocalCoopFocusSlots[player.slot];
    int aimRotation = localCoopDirectionFromStick(player.aimX, player.aimY);

    if (aimRotation == -1 && localCoopFocusIsInteractable(actor, focus.interactionTarget)) {
        return focus.interactionTarget;
    }

    if (aimRotation == -1) {
        aimRotation = actor->rotation;
    }

    Object* best = nullptr;
    double bestScore = 1.0e30;
    bool activelyAiming = player.aimX != 0 || player.aimY != 0;
    constexpr double kInteractAcquireHalfAngle = 0.82; // about 47 degrees

    // Scan critters/items/scenery directly so the non-combat bead selects the
    // object nearest the actual right-stick ray rather than one of six sectors.
    const int types[3] = { OBJ_TYPE_CRITTER, OBJ_TYPE_ITEM, OBJ_TYPE_SCENERY };
    for (int typeIndex = 0; typeIndex < 3; typeIndex++) {
        Object** objects = nullptr;
        int count = objectListCreate(-1, actor->elevation, types[typeIndex], &objects);
        if (count <= 0 || objects == nullptr) {
            continue;
        }

        for (int index = 0; index < count; index++) {
            Object* candidate = objects[index];
            if (!localCoopFocusIsInteractable(actor, candidate)) {
                continue;
            }

            int distance = objectGetDistanceBetween(actor, candidate);
            double angularError = 0.0;
            if (activelyAiming) {
                angularError = localCoopFocusAimError(player, candidate);
                if (angularError > kInteractAcquireHalfAngle) {
                    continue;
                }
            } else {
                int targetRotation = tileGetRotationTo(actor->tile, candidate->tile);
                int rotationDifference = localCoopFocusRotationDifference(aimRotation, targetRotation);
                if (rotationDifference > 1) {
                    continue;
                }
                angularError = static_cast<double>(rotationDifference);
            }

            double typeBias = 0.0;
            int candidateType = PID_TYPE(candidate->pid);
            if (candidateType == OBJ_TYPE_CRITTER) {
                typeBias = -20.0;
            } else if (candidateType == OBJ_TYPE_SCENERY) {
                typeBias = -10.0;
            }

            double score = angularError * 1000.0 + static_cast<double>(distance) * 10.0 + typeBias;
            if (score < bestScore) {
                bestScore = score;
                best = candidate;
            }
        }

        objectListFree(objects);
    }

    // Items at the actor's feet should remain easy to pick up even when the
    // right stick is centered.
    if (!activelyAiming) {
        Object* object = objectFindFirstAtLocation(actor->elevation, actor->tile);
        while (object != nullptr) {
            if (localCoopFocusIsInteractable(actor, object) && object != actor) {
                int score = PID_TYPE(object->pid) == OBJ_TYPE_ITEM ? 2 : 5;
                if (static_cast<double>(score) < bestScore) {
                    bestScore = static_cast<double>(score);
                    best = object;
                }
            }
            object = objectFindNextAtLocation();
        }
    }

    focus.interactionTarget = best;
    return best;
}

inline Object* localCoopFocusUpdateForPlayer(LocalCoopPlayer& player)
{
    Object* focusTarget = nullptr;
    bool hostile = false;

    if (player.uiMode == LocalCoopUiMode::World) {
        bool activelyAiming = player.aimX != 0 || player.aimY != 0;

        if (isInCombat() || activelyAiming) {
            focusTarget = localCoopFocusFindEnemy(player);
            hostile = focusTarget != nullptr;
        }

        if (focusTarget == nullptr) {
            focusTarget = localCoopFocusFindInteractable(player);
            hostile = false;
        }
    }

    localCoopFocusApplyOutline(player.slot, focusTarget, hostile);
    return focusTarget;
}

inline void localCoopFocusTick()
{
    for (LocalCoopPlayer& player : gLocalCoopPlayers) {
        if (!player.connected || !player.humanOwned || player.actor == nullptr) {
            localCoopFocusReleaseOutline(player.slot);
            continue;
        }

        localCoopFocusUpdateForPlayer(player);
    }
}

} // namespace fallout

#endif /* LOCAL_COOP_FOCUS_H */
