#ifndef LOCAL_COOP_INTERACTION_H
#define LOCAL_COOP_INTERACTION_H

#include <SDL.h>

#include <array>

#include "actions.h"
#include "combat.h"
#include "critter.h"
#include "game_mouse.h"
#include "item.h"
#include "kb.h"
#include "local_coop.h"
#include "local_coop_ai_realtime.h"
#include "local_coop_focus.h"
#include "local_coop_loot_ui.h"
#include "map.h"
#include "mouse.h"
#include "object.h"
#include "proto_types.h"
#include "tile.h"
#include "window_manager.h"

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

inline bool localCoopMouseCanControlPlayerOne()
{
    if (!gLocalCoopInitialized) {
        return false;
    }

    LocalCoopPlayer& player = gLocalCoopPlayers[0];
    return player.actor != nullptr
        && player.actor == gDude
        && player.humanOwned
        && player.uiMode == LocalCoopUiMode::World
        && (player.actor->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) == 0;
}

inline bool localCoopMouseMovePlayerOneToCursor()
{
    if (!localCoopMouseCanControlPlayerOne()
        || gGameMouseHexCursor == nullptr
        || (gGameMouseHexCursor->flags & OBJECT_HIDDEN) != 0) {
        return false;
    }

    Object* actor = gLocalCoopPlayers[0].actor;
    int destination = gGameMouseHexCursor->tile;
    if (!tileIsValid(destination)) {
        return false;
    }

    if (destination == actor->tile) {
        return true;
    }

    if (!localCoopMoveRespectsSharedScreen(actor, destination)) {
        return true;
    }

    if (animationIsBusy(actor)) {
        return true;
    }

    if (_obj_blocking_at(actor, destination, actor->elevation) != nullptr) {
        return true;
    }

    if (reg_anim_begin(ANIMATION_REQUEST_UNRESERVED | ANIMATION_REQUEST_INSIGNIFICANT) == -1) {
        return true;
    }

    bool run = gPressedPhysicalKeys[SDL_SCANCODE_LSHIFT]
        || gPressedPhysicalKeys[SDL_SCANCODE_RSHIFT];
    int rc = run
        ? animationRegisterRunToTile(actor, destination, actor->elevation, -1, 0)
        : animationRegisterMoveToTile(actor, destination, actor->elevation, -1, 0);

    if (rc == -1) {
        reg_anim_clear(actor);
        return true;
    }

    reg_anim_end();
    return true;
}

inline int localCoopMouseTargetPriority(Object* actor, Object* object)
{
    if (object == nullptr
        || object == actor
        || object == gGameMouseHexCursor
        || object == gGameMouseBouncingCursor
        || (object->flags & OBJECT_HIDDEN) != 0) {
        return 100;
    }

    switch (FID_TYPE(object->fid)) {
    case OBJ_TYPE_CRITTER:
        return (object->data.critter.combat.results & DAM_DEAD) != 0 ? 1 : 0;
    case OBJ_TYPE_ITEM:
        return 2;
    case OBJ_TYPE_SCENERY:
        return 3;
    case OBJ_TYPE_WALL:
        return 4;
    default:
        return 100;
    }
}

inline Object* localCoopMouseFindTargetAtCursor()
{
    if (!localCoopMouseCanControlPlayerOne()
        || gGameMouseHexCursor == nullptr
        || !tileIsValid(gGameMouseHexCursor->tile)) {
        return nullptr;
    }

    Object* actor = gLocalCoopPlayers[0].actor;
    Object* best = nullptr;
    int bestPriority = 100;

    Object* object = objectFindFirstAtLocation(actor->elevation, gGameMouseHexCursor->tile);
    while (object != nullptr) {
        int priority = localCoopMouseTargetPriority(actor, object);
        if (priority < bestPriority) {
            best = object;
            bestPriority = priority;
            if (bestPriority == 0) {
                break;
            }
        }
        object = objectFindNextAtLocation();
    }

    return best;
}

inline bool localCoopMouseAttackPlayerOne(Object* target)
{
    if (!localCoopMouseCanControlPlayerOne() || target == nullptr) {
        return false;
    }

    LocalCoopPlayer& player = gLocalCoopPlayers[0];
    Object* actor = player.actor;
    if (target == actor
        || localCoopActorIsHumanOwned(target)
        || FID_TYPE(target->fid) != OBJ_TYPE_CRITTER
        || (target->data.critter.combat.results & DAM_DEAD) != 0) {
        return false;
    }

    if (animationIsBusy(actor)) {
        reg_anim_clear(actor);
    }

    Object* weapon = localCoopGetActiveItem(player);
    int hitMode = localCoopGetPrimaryHitMode(player);
    int savedActionPoints = actor->data.critter.combat.ap;

    actor->data.critter.combat.ap = 9999;
    int badShot = _combat_check_bad_shot(actor, target, hitMode, false);
    if (badShot == COMBAT_BAD_SHOT_NO_AMMO && weapon != nullptr) {
        weaponAttemptReload(actor, weapon);
        badShot = _combat_check_bad_shot(actor, target, hitMode, false);
    }

    if (badShot == COMBAT_BAD_SHOT_OK
        && _combat_attack(actor, target, hitMode, HIT_LOCATION_UNCALLED) == 0) {
        localCoopRealtimeAiEngageHostile(target, actor);
    }

    actor->data.critter.combat.ap = savedActionPoints;
    if (gInterfaceBarWindow != -1) {
        interfaceRenderActionPoints(-1, -1);
    }
    return true;
}

inline bool localCoopMouseInteractOrAttackPlayerOne()
{
    if (!localCoopMouseCanControlPlayerOne()) {
        return false;
    }

    Object* actor = gLocalCoopPlayers[0].actor;
    Object* target = localCoopMouseFindTargetAtCursor();
    if (target == nullptr) {
        return true;
    }

    switch (FID_TYPE(target->fid)) {
    case OBJ_TYPE_CRITTER:
        if ((target->data.critter.combat.results & DAM_DEAD) != 0) {
            return localCoopLiveLootRequest(actor, target);
        }

        if (localCoopActorIsHumanOwned(target)) {
            return true;
        }

        if (target->data.critter.combat.team != actor->data.critter.combat.team) {
            return localCoopMouseAttackPlayerOne(target);
        }

        if (_action_can_talk_to(actor, target) == 0) {
            return actionTalk(actor, target) == 0;
        }

        return _action_use_an_object(actor, target) == 0;

    case OBJ_TYPE_ITEM:
        if (itemGetType(target) == ITEM_TYPE_CONTAINER) {
            return localCoopLiveLootRequest(actor, target);
        }

        if (actionPickUp(actor, target) == 0) {
            localCoopSweepSharedInventory();
        }
        return true;

    case OBJ_TYPE_SCENERY:
    case OBJ_TYPE_WALL:
        _action_use_an_object(actor, target);
        return true;

    default:
        return true;
    }
}

inline bool localCoopHandlePlayerOneMouseInput(int keyCode)
{
    if (keyCode != -2 || !localCoopMouseCanControlPlayerOne()) {
        return false;
    }

    int mouseX;
    int mouseY;
    mouseGetPosition(&mouseX, &mouseY);

    if (windowGetAtPoint(mouseX, mouseY) != gIsoWindow) {
        return false;
    }

    int mouseState = mouseGetEvent();

    if ((mouseState & MOUSE_EVENT_RIGHT_BUTTON_DOWN) != 0
        && (mouseState & MOUSE_EVENT_RIGHT_BUTTON_REPEAT) == 0) {
        localCoopMouseMovePlayerOneToCursor();
        return true;
    }

    if ((mouseState & MOUSE_EVENT_LEFT_BUTTON_UP) != 0) {
        localCoopMouseInteractOrAttackPlayerOne();
        return true;
    }

    return false;
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

        if (slot > 0 && player.uiMode == LocalCoopUiMode::World) {
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
            } else {
                localCoopCompanionPickup(slot);
            }
            state.nextInteractTick = now + 300;
        }

        state.interactWasDown = interactDown;
    }
}

} // namespace fallout

#endif /* LOCAL_COOP_INTERACTION_H */
