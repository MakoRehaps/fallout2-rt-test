#ifndef LOCAL_COOP_INTERACTION_H
#define LOCAL_COOP_INTERACTION_H

#include <SDL.h>

#include "actions.h"
#include "item.h"
#include "local_coop.h"
#include "local_coop_focus.h"
#include "object.h"
#include "proto_types.h"

namespace fallout {

struct LocalCoopInteractionState {
    bool interactWasDown = false;
    Uint32 nextInteractTick = 0;
};

inline LocalCoopInteractionState gLocalCoopInteractionState;

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
            return _action_loot_container(actor, target) == 0;
        }

        if (_action_can_talk_to(actor, target) == 0) {
            return actionTalk(actor, target) == 0;
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
