#ifndef LOCAL_COOP_INTERACTION_H
#define LOCAL_COOP_INTERACTION_H

#include <SDL.h>

#include "actions.h"
#include "item.h"
#include "local_coop.h"
#include "object.h"
#include "proto_types.h"
#include "tile.h"

namespace fallout {

struct LocalCoopInteractionState {
    bool interactWasDown = false;
    Uint32 nextInteractTick = 0;
};

inline LocalCoopInteractionState gLocalCoopInteractionState;

inline Object* localCoopFindInteractionObjectAtTile(Object* actor, int tile)
{
    if (actor == nullptr || !tileIsValid(tile)) {
        return nullptr;
    }

    Object* best = nullptr;
    Object* object = objectFindFirstAtLocation(actor->elevation, tile);
    while (object != nullptr) {
        if (object != actor && (object->flags & OBJECT_HIDDEN) == 0) {
            int objectType = PID_TYPE(object->pid);

            // Prefer living critters (dialogue) and containers/scenery over
            // loose floor items if several objects share the same tile.
            if (objectType == OBJ_TYPE_CRITTER
                && (object->data.critter.combat.results & DAM_DEAD) == 0) {
                return object;
            }

            if (objectType == OBJ_TYPE_SCENERY) {
                best = object;
            } else if (best == nullptr && objectType == OBJ_TYPE_ITEM) {
                best = object;
            } else if (best == nullptr && objectType == OBJ_TYPE_CRITTER) {
                best = object;
            }
        }

        object = objectFindNextAtLocation();
    }

    return best;
}

inline Object* localCoopFindPlayerOneInteractionTarget()
{
    LocalCoopPlayer& player = gLocalCoopPlayers[0];
    Object* actor = player.actor;
    if (actor == nullptr) {
        return nullptr;
    }

    // Right stick chooses the interaction direction when held. Otherwise use
    // the actor's current facing so interaction remains natural with one stick.
    int rotation = localCoopDirectionFromStick(player.aimX, player.aimY);
    if (rotation == -1) {
        rotation = actor->rotation;
    }

    int frontTile = tileGetTileInDirection(actor->tile, rotation, 1);
    Object* target = localCoopFindInteractionObjectAtTile(actor, frontTile);
    if (target != nullptr) {
        return target;
    }

    // Loose items and some scenery can occupy the player's current tile.
    return localCoopFindInteractionObjectAtTile(actor, actor->tile);
}

inline bool localCoopPlayerOneInteract()
{
    LocalCoopPlayer& player = gLocalCoopPlayers[0];
    Object* actor = player.actor;
    if (actor == nullptr || actor != gDude || player.uiMode != LocalCoopUiMode::World) {
        return false;
    }

    Object* target = localCoopFindPlayerOneInteractionTarget();
    if (target == nullptr) {
        return false;
    }

    int objectType = PID_TYPE(target->pid);
    if (objectType == OBJ_TYPE_CRITTER) {
        if ((target->data.critter.combat.results & DAM_DEAD) != 0) {
            return _action_loot_container(actor, target) == 0;
        }

        if (_action_can_talk_to(actor, target) == 0) {
            int rc = actionTalk(actor, target);
            return rc == 0;
        }

        return _action_use_an_object(actor, target) == 0;
    }

    if (objectType == OBJ_TYPE_ITEM) {
        if (itemGetType(target) == ITEM_TYPE_CONTAINER) {
            return _action_loot_container(actor, target) == 0;
        }

        return actionPickUp(actor, target) == 0;
    }

    return _action_use_an_object(actor, target) == 0;
}

inline void localCoopInteractionTick()
{
    if (!gLocalCoopInitialized) {
        return;
    }

    LocalCoopPlayer& player = gLocalCoopPlayers[0];
    if (!player.connected || player.controller == nullptr || !player.humanOwned || player.actor != gDude) {
        gLocalCoopInteractionState.interactWasDown = false;
        return;
    }

    bool interactDown = SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_A) != 0;
    Uint32 now = SDL_GetTicks();

    if (interactDown
        && !gLocalCoopInteractionState.interactWasDown
        && static_cast<Sint32>(now - gLocalCoopInteractionState.nextInteractTick) >= 0
        && player.uiMode == LocalCoopUiMode::World) {
        localCoopPlayerOneInteract();
        gLocalCoopInteractionState.nextInteractTick = now + 300;
    }

    gLocalCoopInteractionState.interactWasDown = interactDown;
}

} // namespace fallout

#endif /* LOCAL_COOP_INTERACTION_H */
