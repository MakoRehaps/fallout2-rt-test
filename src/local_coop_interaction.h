#ifndef LOCAL_COOP_INTERACTION_H
#define LOCAL_COOP_INTERACTION_H

#include <SDL.h>

#include <array>

#include "actions.h"
#include "item.h"
#include "local_coop.h"
#include "local_coop_focus.h"
#include "local_coop_loot_ui.h"
#include "object.h"
#include "proto_types.h"
#include "tile.h"

namespace fallout {

struct LocalCoopInteractionState {
    bool interactWasDown = false;
    Uint32 nextInteractTick = 0;
};

inline std::array<LocalCoopInteractionState, kLocalCoopMaxPlayers> gLocalCoopInteractionStates;

inline bool localCoopIsSimplePickup(Object* object)
{
    return object != nullptr
        && PID_TYPE(object->pid) == OBJ_TYPE_ITEM
        && itemGetType(object) != ITEM_TYPE_CONTAINER
        && object->owner == nullptr
        && (object->flags & OBJECT_HIDDEN) == 0;
}

inline Object* localCoopFindSimplePickup(LocalCoopPlayer& player)
{
    Object* actor = player.actor;
    if (actor == nullptr) {
        return nullptr;
    }

    // Prefer an item on the actor's tile, then search a short right-stick/facing
    // wedge. P2-P4 deliberately do not focus scenery, critters or containers;
    // those remain P1-authoritative world interactions.
    Object* object = objectFindFirstAtLocation(actor->elevation, actor->tile);
    while (object != nullptr) {
        if (object != actor && localCoopIsSimplePickup(object)) {
            return object;
        }
        object = objectFindNextAtLocation();
    }

    int aimRotation = localCoopDirectionFromStick(player.aimX, player.aimY);
    if (aimRotation == -1) {
        aimRotation = actor->rotation;
    }

    for (int rotationOffset = -1; rotationOffset <= 1; rotationOffset++) {
        int rotation = (aimRotation + rotationOffset + ROTATION_COUNT) % ROTATION_COUNT;
        for (int distance = 1; distance <= 3; distance++) {
            int tile = tileGetTileInDirection(actor->tile, rotation, distance);
            if (!tileIsValid(tile)) {
                break;
            }

            object = objectFindFirstAtLocation(actor->elevation, tile);
            while (object != nullptr) {
                if (localCoopIsSimplePickup(object)) {
                    return object;
                }
                object = objectFindNextAtLocation();
            }
        }
    }

    return nullptr;
}

inline bool localCoopPlayerOneInteract()
{
    LocalCoopPlayer& player = gLocalCoopPlayers[0];
    Object* actor = player.actor;
    if (actor == nullptr || actor != gDude || player.uiMode != LocalCoopUiMode::World) {
        return false;
    }

    // Controller-native interaction: no virtual mouse. Right-stick direction
    // establishes a soft focus wedge; A dispatches the stock Fallout action for
    // the focused object directly.
    Object* target = localCoopFocusFindInteractable(player);
    if (target == nullptr) {
        return false;
    }

    int objectType = PID_TYPE(target->pid);
    if (objectType == OBJ_TYPE_CRITTER) {
        if ((target->data.critter.combat.results & DAM_DEAD) != 0) {
            return localCoopLiveLootRequest(actor, target);
        }

        if (_action_can_talk_to(actor, target) == 0) {
            return actionTalk(actor, target) == 0;
        }

        return _action_use_an_object(actor, target) == 0;
    }

    if (objectType == OBJ_TYPE_ITEM) {
        if (itemGetType(target) == ITEM_TYPE_CONTAINER) {
            return localCoopLiveLootRequest(actor, target);
        }

        bool pickedUp = actionPickUp(actor, target) == 0;
        if (pickedUp) {
            localCoopSweepSharedInventory();
        }
        return pickedUp;
    }

    return _action_use_an_object(actor, target) == 0;
}

inline bool localCoopCompanionPickup(int slot)
{
    if (slot <= 0 || slot >= kLocalCoopMaxPlayers) {
        return false;
    }

    LocalCoopPlayer& player = gLocalCoopPlayers[slot];
    Object* actor = player.actor;
    if (actor == nullptr || !player.humanOwned || player.uiMode != LocalCoopUiMode::World) {
        return false;
    }

    Object* target = localCoopFindSimplePickup(player);
    if (target == nullptr) {
        return false;
    }

    bool pickedUp = actionPickUp(actor, target) == 0;
    if (pickedUp) {
        // The item exists in the companion inventory only long enough for the
        // stock pickup script/animation to run. The shared-pool sweep then moves
        // it to P1's party inventory unless it somehow became equipped.
        localCoopSweepSharedInventory();
    }
    return pickedUp;
}

inline void localCoopInteractionTick()
{
    if (!gLocalCoopInitialized) {
        return;
    }

    Uint32 now = SDL_GetTicks();

    for (int slot = 0; slot < kLocalCoopMaxPlayers; slot++) {
        LocalCoopPlayer& player = gLocalCoopPlayers[slot];
        LocalCoopInteractionState& state = gLocalCoopInteractionStates[slot];

        if (!player.connected
            || player.controller == nullptr
            || !player.humanOwned
            || player.actor == nullptr) {
            state.interactWasDown = false;
            continue;
        }

        bool interactDown = SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_A) != 0;

        if (slot > 0
            && !isInCombat()
            && player.uiMode == LocalCoopUiMode::World) {
            Object* pickup = localCoopFindSimplePickup(player);
            if (pickup != nullptr) {
                localCoopFocusApplyOutline(slot, pickup, false);
            }
        }

        if (interactDown
            && !state.interactWasDown
            && static_cast<Sint32>(now - state.nextInteractTick) >= 0
            && player.uiMode == LocalCoopUiMode::World) {
            if (slot == 0) {
                localCoopPlayerOneInteract();
            } else if (!isInCombat()) {
                localCoopCompanionPickup(slot);
            }
            state.nextInteractTick = now + 300;
        }

        state.interactWasDown = interactDown;
    }
}

} // namespace fallout

#endif /* LOCAL_COOP_INTERACTION_H */
