#ifndef LOCAL_COOP_FOCUS_H
#define LOCAL_COOP_FOCUS_H

#include <algorithm>
#include <array>

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

    // Give the stick a small acquisition buffer outside exact firing range so
    // melee/short-range players can lock while closing distance, but never let
    // a controller snap to an irrelevant critter across the entire map.
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

    return target->data.critter.combat.team != actor->data.critter.combat.team;
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

inline Object* localCoopFocusFindEnemy(LocalCoopPlayer& player)
{
    Object* actor = player.actor;
    if (actor == nullptr) {
        return nullptr;
    }

    LocalCoopFocusSlot& focus = gLocalCoopFocusSlots[player.slot];
    int aimRotation = localCoopDirectionFromStick(player.aimX, player.aimY);

    if (aimRotation == -1 && localCoopFocusIsEnemy(actor, focus.combatTarget)) {
        return focus.combatTarget;
    }

    Object** critters = nullptr;
    int count = objectListCreate(-1, actor->elevation, OBJ_TYPE_CRITTER, &critters);
    if (count <= 0 || critters == nullptr) {
        focus.combatTarget = nullptr;
        return nullptr;
    }

    Object* best = nullptr;
    int bestScore = 0x7FFFFFFF;

    for (int index = 0; index < count; index++) {
        Object* candidate = critters[index];
        if (!localCoopFocusIsEnemy(actor, candidate)) {
            continue;
        }

        int distance = objectGetDistanceBetween(actor, candidate);
        int rotationDifference = 0;
        if (aimRotation != -1) {
            int targetRotation = tileGetRotationTo(actor->tile, candidate->tile);
            rotationDifference = localCoopFocusRotationDifference(aimRotation, targetRotation);
            if (rotationDifference > 1) {
                continue;
            }
        }

        int score = rotationDifference * 1000 + distance;
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
    int bestScore = 0x7FFFFFFF;

    for (int rotationOffset = -1; rotationOffset <= 1; rotationOffset++) {
        int rotation = (aimRotation + rotationOffset + ROTATION_COUNT) % ROTATION_COUNT;
        int angularPenalty = std::abs(rotationOffset) * 100;

        for (int distance = 1; distance <= 6; distance++) {
            int tile = tileGetTileInDirection(actor->tile, rotation, distance);
            if (!tileIsValid(tile)) {
                break;
            }

            Object* object = objectFindFirstAtLocation(actor->elevation, tile);
            while (object != nullptr) {
                if (localCoopFocusIsInteractable(actor, object)) {
                    int score = angularPenalty + distance;
                    int type = PID_TYPE(object->pid);
                    if (type == OBJ_TYPE_CRITTER) {
                        score -= 20;
                    } else if (type == OBJ_TYPE_SCENERY) {
                        score -= 10;
                    }

                    if (score < bestScore) {
                        bestScore = score;
                        best = object;
                    }
                }

                object = objectFindNextAtLocation();
            }
        }
    }

    Object* object = objectFindFirstAtLocation(actor->elevation, actor->tile);
    while (object != nullptr) {
        if (localCoopFocusIsInteractable(actor, object) && object != actor) {
            int score = PID_TYPE(object->pid) == OBJ_TYPE_ITEM ? 2 : 5;
            if (score < bestScore) {
                bestScore = score;
                best = object;
            }
        }
        object = objectFindNextAtLocation();
    }

    focus.interactionTarget = best;
    return best;
}

inline Object* localCoopFocusUpdateForPlayer(LocalCoopPlayer& player)
{
    Object* focusTarget = nullptr;
    bool hostile = false;

    if (player.uiMode == LocalCoopUiMode::World) {
        bool activelyAiming = localCoopDirectionFromStick(player.aimX, player.aimY) != -1;

        if (isInCombat() || activelyAiming) {
            focusTarget = localCoopFocusFindEnemy(player);
            hostile = focusTarget != nullptr;
        }

        if (focusTarget == nullptr && player.slot == 0) {
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
