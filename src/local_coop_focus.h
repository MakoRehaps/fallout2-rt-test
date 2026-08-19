#ifndef LOCAL_COOP_FOCUS_H
#define LOCAL_COOP_FOCUS_H

#include <algorithm>
#include <array>

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

inline bool localCoopFocusTargetStillUsable(const Object* actor, const Object* target, int maxDistance)
{
    return actor != nullptr
        && target != nullptr
        && actor != target
        && target->elevation == actor->elevation
        && (target->flags & OBJECT_HIDDEN) == 0
        && objectGetDistanceBetween(actor, const_cast<Object*>(target)) <= maxDistance;
}

inline bool localCoopFocusIsEnemy(const Object* actor, const Object* target)
{
    if (!localCoopFocusTargetStillUsable(actor, target, 60)) {
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

    // Neutral right stick means keep the current target if it is still valid.
    // This gives the controller a sticky soft lock instead of requiring the
    // player to hold a virtual mouse pointer over an enemy.
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

    // Search a small wedge in front of the player. This replaces virtual-mouse
    // clicking with controller-native spatial focus. A can then directly call
    // the engine's talk/use/loot/pickup action for the focused object.
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

                    // Conversations, bodies and containers should beat loose
                    // floor items when several interactables overlap.
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

    // Also allow picking up an item standing on the same tile.
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
        Object* enemy = localCoopFocusFindEnemy(player);
        if (enemy != nullptr) {
            focusTarget = enemy;
            hostile = true;
        } else if (player.slot == 0) {
            focusTarget = localCoopFocusFindInteractable(player);
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
