#ifndef LOCAL_COOP_H
#define LOCAL_COOP_H

#include <SDL.h>

#include <array>
#include <cmath>
#include <vector>

#include "animation.h"
#include "game.h"
#include "interface.h"
#include "inventory.h"
#include "item.h"
#include "local_coop_danger.h"
#include "object.h"
#include "party_member.h"
#include "proto_types.h"
#include "tile.h"

namespace fallout {

inline constexpr int kLocalCoopMaxPlayers = 4;
inline constexpr int kLocalCoopControllerDeadzone = 9000;
inline constexpr int kLocalCoopCameraTetherTiles = 18;

enum class LocalCoopUiMode {
    World,
    Inventory,
    Character,
    PipBoy,
    Loot,
    Dialogue,
    Barter,
};

struct LocalCoopPlayer {
    int slot = 0;
    SDL_GameController* controller = nullptr;
    SDL_JoystickID joystickId = -1;
    Object* actor = nullptr;
    LocalCoopUiMode uiMode = LocalCoopUiMode::World;
    bool connected = false;
    bool humanOwned = false;
    bool wantsRun = false;
    int activeHand = HAND_RIGHT;
    int moveX = 0;
    int moveY = 0;
    int aimX = 0;
    int aimY = 0;
};

inline std::array<LocalCoopPlayer, kLocalCoopMaxPlayers> gLocalCoopPlayers;
inline bool gLocalCoopInitialized = false;

inline void localCoopClearActorBindings()
{
    for (int index = 0; index < kLocalCoopMaxPlayers; index++) {
        gLocalCoopPlayers[index].actor = nullptr;
        gLocalCoopPlayers[index].humanOwned = false;
    }
}

inline void localCoopRefreshActorBindings()
{
    localCoopClearActorBindings();

    gLocalCoopPlayers[0].actor = gDude;
    gLocalCoopPlayers[0].humanOwned = gDude != nullptr;

    std::vector<Object*> party = get_all_party_members_objects(false);
    int slot = 1;
    for (Object* object : party) {
        if (object == nullptr || object == gDude) {
            continue;
        }

        if (slot >= kLocalCoopMaxPlayers) {
            break;
        }

        gLocalCoopPlayers[slot].actor = object;
        gLocalCoopPlayers[slot].humanOwned = true;
        slot++;
    }
}

inline int localCoopFindFreeControllerSlot()
{
    for (int index = 0; index < kLocalCoopMaxPlayers; index++) {
        if (!gLocalCoopPlayers[index].connected) {
            return index;
        }
    }

    return -1;
}

inline bool localCoopHasJoystickId(SDL_JoystickID joystickId)
{
    if (joystickId < 0) {
        return false;
    }

    for (const LocalCoopPlayer& player : gLocalCoopPlayers) {
        if (player.connected && player.joystickId == joystickId) {
            return true;
        }
    }

    return false;
}

inline void localCoopOpenController(int deviceIndex)
{
    if (!SDL_IsGameController(deviceIndex)) {
        return;
    }

    SDL_JoystickID deviceInstanceId = SDL_JoystickGetDeviceInstanceID(deviceIndex);
    if (deviceInstanceId < 0 || localCoopHasJoystickId(deviceInstanceId)) {
        return;
    }

    int slot = localCoopFindFreeControllerSlot();
    if (slot == -1) {
        return;
    }

    SDL_GameController* controller = SDL_GameControllerOpen(deviceIndex);
    if (controller == nullptr) {
        return;
    }

    SDL_Joystick* joystick = SDL_GameControllerGetJoystick(controller);
    LocalCoopPlayer& player = gLocalCoopPlayers[slot];
    player.slot = slot;
    player.controller = controller;
    player.joystickId = SDL_JoystickInstanceID(joystick);
    player.connected = true;
}

inline void localCoopClearController(LocalCoopPlayer& player)
{
    if (player.controller != nullptr) {
        SDL_GameControllerClose(player.controller);
    }

    int slot = player.slot;
    Object* actor = player.actor;
    bool humanOwned = player.humanOwned;
    LocalCoopUiMode uiMode = player.uiMode;
    int activeHand = player.activeHand;

    player = LocalCoopPlayer{};
    player.slot = slot;
    player.actor = actor;
    player.humanOwned = humanOwned;
    player.uiMode = uiMode;
    player.activeHand = activeHand;
}

inline void localCoopCloseControllerByJoystickId(SDL_JoystickID joystickId)
{
    for (LocalCoopPlayer& player : gLocalCoopPlayers) {
        if (player.connected && player.joystickId == joystickId) {
            localCoopClearController(player);
            break;
        }
    }
}

inline void localCoopRefreshControllers()
{
    for (LocalCoopPlayer& player : gLocalCoopPlayers) {
        if (player.connected
            && (player.controller == nullptr || !SDL_GameControllerGetAttached(player.controller))) {
            localCoopClearController(player);
        }
    }

    int joystickCount = SDL_NumJoysticks();
    for (int deviceIndex = 0; deviceIndex < joystickCount; deviceIndex++) {
        if (localCoopFindFreeControllerSlot() == -1) {
            break;
        }
        localCoopOpenController(deviceIndex);
    }
}

inline void localCoopInit()
{
    if (gLocalCoopInitialized) {
        return;
    }

    gLocalCoopInitialized = true;

    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK);
    SDL_GameControllerEventState(SDL_ENABLE);

    for (int index = 0; index < kLocalCoopMaxPlayers; index++) {
        gLocalCoopPlayers[index].slot = index;
        gLocalCoopPlayers[index].activeHand = HAND_RIGHT;
    }

    localCoopRefreshControllers();
    localCoopRefreshActorBindings();

    if (gInterfaceBarWindow != -1) {
        int hand = interfaceGetCurrentHand();
        if (hand == HAND_LEFT || hand == HAND_RIGHT) {
            gLocalCoopPlayers[0].activeHand = hand;
        }
    }
}

inline void localCoopShutdown()
{
    for (LocalCoopPlayer& player : gLocalCoopPlayers) {
        if (player.controller != nullptr) {
            SDL_GameControllerClose(player.controller);
        }
        player = LocalCoopPlayer{};
    }

    localCoopDangerEnd();
    gLocalCoopInitialized = false;
}

inline bool localCoopHandleEvent(const SDL_Event& event)
{
    switch (event.type) {
    case SDL_CONTROLLERDEVICEADDED:
        localCoopOpenController(event.cdevice.which);
        return true;
    case SDL_CONTROLLERDEVICEREMOVED:
        localCoopCloseControllerByJoystickId(event.cdevice.which);
        return true;
    default:
        break;
    }

    return false;
}

inline int localCoopReadAxis(SDL_GameController* controller, SDL_GameControllerAxis axis)
{
    int value = SDL_GameControllerGetAxis(controller, axis);
    if (std::abs(value) < kLocalCoopControllerDeadzone) {
        return 0;
    }

    return value;
}

inline int localCoopDirectionFromStick(int x, int y)
{
    if (x == 0 && y == 0) {
        return -1;
    }

    double angle = std::atan2(static_cast<double>(-y), static_cast<double>(x));
    double normalized = angle;
    if (normalized < 0.0) {
        normalized += 6.28318530717958647692;
    }

    int sector = static_cast<int>(std::floor((normalized + 0.52359877559829887308) / 1.04719755119659774615)) % 6;

    static const int rotations[6] = {
        ROTATION_E,
        ROTATION_NE,
        ROTATION_NW,
        ROTATION_W,
        ROTATION_SW,
        ROTATION_SE,
    };

    return rotations[sector];
}

inline bool localCoopActorIsHumanOwned(const Object* object)
{
    if (object == nullptr) {
        return false;
    }

    for (const LocalCoopPlayer& player : gLocalCoopPlayers) {
        if (player.humanOwned && player.actor == object) {
            return true;
        }
    }

    return false;
}

inline LocalCoopPlayer* localCoopGetPlayerForActor(Object* object)
{
    for (LocalCoopPlayer& player : gLocalCoopPlayers) {
        if (player.actor == object) {
            return &player;
        }
    }

    return nullptr;
}

inline int localCoopGetActiveHand(LocalCoopPlayer& player)
{
    if (player.slot == 0 && player.actor == gDude && gInterfaceBarWindow != -1) {
        int hand = interfaceGetCurrentHand();
        if (hand == HAND_LEFT || hand == HAND_RIGHT) {
            player.activeHand = hand;
        }
    }

    if (player.activeHand != HAND_LEFT && player.activeHand != HAND_RIGHT) {
        player.activeHand = HAND_RIGHT;
    }
    return player.activeHand;
}

inline bool localCoopSetActiveHand(int slot, int hand, bool animated = false)
{
    if (slot < 0 || slot >= kLocalCoopMaxPlayers || (hand != HAND_LEFT && hand != HAND_RIGHT)) {
        return false;
    }

    LocalCoopPlayer& player = gLocalCoopPlayers[slot];
    if (slot == 0 && player.actor == gDude && gInterfaceBarWindow != -1) {
        int current = interfaceGetCurrentHand();
        if (current != hand && interfaceBarSwapHands(animated) != 0) {
            return false;
        }
    }

    player.activeHand = hand;
    return true;
}

inline bool localCoopSwapActiveHand(int slot, bool animated = false)
{
    if (slot < 0 || slot >= kLocalCoopMaxPlayers) {
        return false;
    }

    LocalCoopPlayer& player = gLocalCoopPlayers[slot];
    int hand = localCoopGetActiveHand(player);
    return localCoopSetActiveHand(slot, hand == HAND_LEFT ? HAND_RIGHT : HAND_LEFT, animated);
}

inline Object* localCoopGetActiveItem(LocalCoopPlayer& player)
{
    if (player.actor == nullptr) {
        return nullptr;
    }

    return localCoopGetActiveHand(player) == HAND_LEFT
        ? critterGetItem1(player.actor)
        : critterGetItem2(player.actor);
}

inline int localCoopGetPrimaryHitMode(LocalCoopPlayer& player)
{
    int hand = localCoopGetActiveHand(player);
    Object* item = localCoopGetActiveItem(player);
    if (item == nullptr || itemGetType(item) != ITEM_TYPE_WEAPON) {
        return hand == HAND_LEFT ? HIT_MODE_PUNCH : HIT_MODE_KICK;
    }

    return hand == HAND_LEFT ? HIT_MODE_LEFT_WEAPON_PRIMARY : HIT_MODE_RIGHT_WEAPON_PRIMARY;
}

inline int localCoopGetSecondaryHitMode(LocalCoopPlayer& player)
{
    int hand = localCoopGetActiveHand(player);
    Object* item = localCoopGetActiveItem(player);
    if (item == nullptr || itemGetType(item) != ITEM_TYPE_WEAPON) {
        return hand == HAND_LEFT ? HIT_MODE_PUNCH : HIT_MODE_KICK;
    }

    return hand == HAND_LEFT ? HIT_MODE_LEFT_WEAPON_SECONDARY : HIT_MODE_RIGHT_WEAPON_SECONDARY;
}

inline void localCoopSetUiMode(int slot, LocalCoopUiMode mode)
{
    if (slot < 0 || slot >= kLocalCoopMaxPlayers) {
        return;
    }

    gLocalCoopPlayers[slot].uiMode = mode;
}

inline bool localCoopPlayerCanMove(const LocalCoopPlayer& player)
{
    return player.connected
        && player.humanOwned
        && player.actor != nullptr
        && player.uiMode == LocalCoopUiMode::World
        && (player.actor->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) == 0;
}

inline bool localCoopMoveRespectsSharedScreen(Object* actor, int destination)
{
    if (actor == nullptr || !tileIsValid(destination)) {
        return true;
    }

    if (localCoopDangerBlocksMapExit() && isExitGridAt(destination, actor->elevation)) {
        return false;
    }

    if (!tileIsValid(gCenterTile)) {
        return true;
    }

    int currentDistance = tileDistanceBetween(actor->tile, gCenterTile);
    int destinationDistance = tileDistanceBetween(destination, gCenterTile);

    if (currentDistance <= kLocalCoopCameraTetherTiles) {
        return destinationDistance <= kLocalCoopCameraTetherTiles;
    }

    return destinationDistance < currentDistance;
}

inline void localCoopPollControllers()
{
    if (!gLocalCoopInitialized) {
        return;
    }

    localCoopRefreshControllers();
    localCoopRefreshActorBindings();

    for (LocalCoopPlayer& player : gLocalCoopPlayers) {
        if (!player.connected || player.controller == nullptr) {
            continue;
        }

        player.moveX = localCoopReadAxis(player.controller, SDL_CONTROLLER_AXIS_LEFTX);
        player.moveY = localCoopReadAxis(player.controller, SDL_CONTROLLER_AXIS_LEFTY);
        player.aimX = localCoopReadAxis(player.controller, SDL_CONTROLLER_AXIS_RIGHTX);
        player.aimY = localCoopReadAxis(player.controller, SDL_CONTROLLER_AXIS_RIGHTY);
        player.wantsRun = SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER) != 0;

        if (!localCoopPlayerCanMove(player)) {
            continue;
        }

        Object* actor = player.actor;
        if (animationIsBusy(actor)) {
            continue;
        }

        int rotation = localCoopDirectionFromStick(player.moveX, player.moveY);
        if (rotation == -1) {
            continue;
        }

        int destination = tileGetTileInDirection(actor->tile, rotation, 1);
        if (!tileIsValid(destination)) {
            continue;
        }

        if (!localCoopMoveRespectsSharedScreen(actor, destination)) {
            continue;
        }

        if (_obj_blocking_at(actor, destination, actor->elevation) != nullptr) {
            continue;
        }

        if (reg_anim_begin(ANIMATION_REQUEST_UNRESERVED | ANIMATION_REQUEST_INSIGNIFICANT) == -1) {
            continue;
        }

        int rc;
        if (player.wantsRun) {
            rc = animationRegisterRunToTile(actor, destination, actor->elevation, -1, 0);
        } else {
            rc = animationRegisterMoveToTile(actor, destination, actor->elevation, -1, 0);
        }

        if (rc == -1) {
            reg_anim_clear(actor);
            continue;
        }

        reg_anim_end();
    }
}

inline Object* localCoopGetSharedInventoryOwner()
{
    return gDude;
}

inline void localCoopSweepSharedInventory()
{
    Object* sharedOwner = localCoopGetSharedInventoryOwner();
    if (sharedOwner == nullptr) {
        return;
    }

    localCoopRefreshActorBindings();

    for (int slot = 1; slot < kLocalCoopMaxPlayers; slot++) {
        Object* actor = gLocalCoopPlayers[slot].actor;
        if (actor == nullptr || actor == sharedOwner) {
            continue;
        }

        Inventory& inventory = actor->data.inventory;
        for (int index = inventory.length - 1; index >= 0; index--) {
            InventoryItem inventoryItem = inventory.items[index];
            Object* item = inventoryItem.item;
            if (item == nullptr) {
                continue;
            }

            if ((item->flags & OBJECT_EQUIPPED) != 0) {
                continue;
            }

            itemMoveForce(actor, sharedOwner, item, inventoryItem.quantity);
        }
    }
}

inline Object* localCoopGetEquippedArmor(int slot)
{
    if (slot < 0 || slot >= kLocalCoopMaxPlayers || gLocalCoopPlayers[slot].actor == nullptr) {
        return nullptr;
    }
    return critterGetArmor(gLocalCoopPlayers[slot].actor);
}

inline Object* localCoopGetEquippedLeftHand(int slot)
{
    if (slot < 0 || slot >= kLocalCoopMaxPlayers || gLocalCoopPlayers[slot].actor == nullptr) {
        return nullptr;
    }
    return critterGetItem1(gLocalCoopPlayers[slot].actor);
}

inline Object* localCoopGetEquippedRightHand(int slot)
{
    if (slot < 0 || slot >= kLocalCoopMaxPlayers || gLocalCoopPlayers[slot].actor == nullptr) {
        return nullptr;
    }
    return critterGetItem2(gLocalCoopPlayers[slot].actor);
}

inline bool localCoopEquipSharedItem(int slot, Object* item, int hand)
{
    if (slot < 0 || slot >= kLocalCoopMaxPlayers || item == nullptr) {
        return false;
    }

    Object* actor = gLocalCoopPlayers[slot].actor;
    Object* sharedOwner = localCoopGetSharedInventoryOwner();
    if (actor == nullptr || sharedOwner == nullptr) {
        return false;
    }

    if (actor != sharedOwner && item->owner != actor) {
        if (itemMoveForce(sharedOwner, actor, item, 1) != 0) {
            return false;
        }
    }

    if (_inven_wield(actor, item, hand) != 0) {
        return false;
    }

    if (actor == gDude && gInterfaceBarWindow != -1) {
        interfaceUpdateItems(false, INTERFACE_ITEM_ACTION_DEFAULT, INTERFACE_ITEM_ACTION_DEFAULT);
    }
    return true;
}

inline bool localCoopUnequipToSharedPool(int slot, int hand)
{
    if (slot < 0 || slot >= kLocalCoopMaxPlayers) {
        return false;
    }

    Object* actor = gLocalCoopPlayers[slot].actor;
    Object* sharedOwner = localCoopGetSharedInventoryOwner();
    if (actor == nullptr || sharedOwner == nullptr) {
        return false;
    }

    Object* item = hand == HAND_LEFT ? critterGetItem1(actor) : critterGetItem2(actor);
    if (item == nullptr) {
        return true;
    }

    if (_inven_unwield(actor, hand) != 0) {
        return false;
    }

    item->flags &= ~OBJECT_IN_ANY_HAND;
    if (actor != sharedOwner) {
        itemMoveForce(actor, sharedOwner, item, 1);
    }

    if (actor == gDude && gInterfaceBarWindow != -1) {
        interfaceUpdateItems(false, INTERFACE_ITEM_ACTION_DEFAULT, INTERFACE_ITEM_ACTION_DEFAULT);
    }
    return true;
}

} // namespace fallout

#endif /* LOCAL_COOP_H */
