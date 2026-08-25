#ifndef LOCAL_COOP_H
#define LOCAL_COOP_H

#include <SDL.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "animation.h"
#include "color.h"
#include "art.h"
#include "debug.h"
#include "game.h"
#include "interface.h"
#include "inventory.h"
#include "item.h"
#include "local_coop_character_state.h"
#include "local_coop_danger.h"
#include "object.h"
#include "party_member.h"
#include "proto.h"
#include "proto_types.h"
#include "stat.h"
#include "svga.h"
#include "text_font.h"
#include "window_manager.h"
#include "tile.h"

namespace fallout {

void scriptsRequestWorldMap();

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

enum class LocalCoopActionMode {
    Interact,
    Aim,
};

struct LocalCoopPlayer {
    int slot = 0;
    SDL_GameController* controller = nullptr;
    SDL_JoystickID joystickId = -1;
    Object* actor = nullptr;
    LocalCoopUiMode uiMode = LocalCoopUiMode::World;
    bool connected = false;
    bool humanOwned = false;
    bool slotLocked = false;
    bool joinMenuActive = false;
    bool joinStartWasDown = false;
    bool joinLeftWasDown = false;
    bool joinRightWasDown = false;
    bool joinConfirmWasDown = false;
    bool joinCancelWasDown = false;
    bool joinGenderWasDown = false;
    bool wantsRun = false;
    int archetype = 0;
    int gender = GENDER_MALE;
    int joinWindow = -1;
    char controllerGuid[64] {};
    int activeHand = HAND_RIGHT;
    int moveX = 0;
    int moveY = 0;
    int aimX = 0;
    int aimY = 0;
    LocalCoopActionMode actionMode = LocalCoopActionMode::Interact;
    bool hexAimHeld = false;
    int hexAimTile = -1;
    bool controllerInputActive = false;
    bool sneaking = false;
};

inline std::array<LocalCoopPlayer, kLocalCoopMaxPlayers> gLocalCoopPlayers;
inline bool gLocalCoopInitialized = false;
inline uint32_t gLocalCoopAppliedCharacterStateRevision = 0xFFFFFFFF;
inline int gLocalCoopModalControllerSlot = -1;
inline int gLocalCoopSkilldexInvokerSlot = -1;

inline int localCoopFindSpawnTile(Object* anchor, int distance);

inline void localCoopClearActorBindings()
{
    for (int index = 0; index < kLocalCoopMaxPlayers; index++) {
        gLocalCoopPlayers[index].actor = nullptr;
        gLocalCoopPlayers[index].humanOwned = false;
    }
}

inline void localCoopRefreshActorBindings()
{
    gLocalCoopPlayers[0].actor = gDude;
    gLocalCoopPlayers[0].humanOwned = gDude != nullptr;
    gLocalCoopPlayers[0].slotLocked = true;

    // P2-P4 are independent synthetic player critters. Recruited companions
    // remain in the stock party and are never consumed as controller bodies.
    for (int slot = 1; slot < kLocalCoopMaxPlayers; slot++) {
        LocalCoopPlayer& player = gLocalCoopPlayers[slot];
        player.humanOwned = player.slotLocked && player.actor != nullptr;
    }
}

inline int localCoopFindFreeControllerSlot()
{
    for (int index = 0; index < kLocalCoopMaxPlayers; index++) {
        const LocalCoopPlayer& player = gLocalCoopPlayers[index];
        // P1 is permanently reserved for the story actor, but the first
        // controller must still be allowed to claim that reserved slot.
        if (!player.connected && (index == 0 || !player.slotLocked)) {
            return index;
        }
    }

    return -1;
}

inline int localCoopFindReservedControllerSlot(const char* guid)
{
    if (guid != nullptr && *guid != '\0') {
        for (int slot = 0; slot < kLocalCoopMaxPlayers; slot++) {
            const LocalCoopPlayer& player = gLocalCoopPlayers[slot];
            if (player.slotLocked
                && !player.connected
                && strcmp(player.controllerGuid, guid) == 0) {
                return slot;
            }
        }
    }

    // When every numbered slot is already reserved, permit a replacement
    // controller to reclaim the only disconnected reservation.
    if (localCoopFindFreeControllerSlot() == -1) {
        int candidate = -1;
        for (int slot = 0; slot < kLocalCoopMaxPlayers; slot++) {
            const LocalCoopPlayer& player = gLocalCoopPlayers[slot];
            if (!player.slotLocked || player.connected) {
                continue;
            }
            if (candidate != -1) {
                return -1;
            }
            candidate = slot;
        }
        return candidate;
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

    SDL_JoystickGUID deviceGuid = SDL_JoystickGetDeviceGUID(deviceIndex);
    char guid[64] {};
    SDL_JoystickGetGUIDString(deviceGuid, guid, sizeof(guid));

    int slot = localCoopFindReservedControllerSlot(guid);
    if (slot == -1) {
        slot = localCoopFindFreeControllerSlot();
    }
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
    snprintf(player.controllerGuid, sizeof(player.controllerGuid), "%s", guid);

    if (player.slotLocked && player.actor != nullptr) {
        int spawnTile = localCoopFindSpawnTile(gDude, 3);
        if (spawnTile != -1) {
            objectSetLocation(player.actor, spawnTile, gDude->elevation, nullptr);
        }
        player.actor->flags &= ~(OBJECT_HIDDEN | OBJECT_NO_BLOCK);
        player.humanOwned = true;

        LocalCoopCharacterSlotState& saved =
            localCoopCharacterStateGet().slots[slot];
        snprintf(saved.controllerGuid, sizeof(saved.controllerGuid), "%s", guid);
        debugPrint("[COOP JOIN] slot=%d reconnected\n", slot);
    }
}

inline void localCoopClearController(LocalCoopPlayer& player)
{
    if (player.controller != nullptr) {
        SDL_GameControllerClose(player.controller);
    }

    if (player.slotLocked && player.slot > 0 && player.actor != nullptr) {
        reg_anim_clear(player.actor);
        player.actor->flags |= OBJECT_HIDDEN | OBJECT_NO_BLOCK;
        debugPrint("[COOP JOIN] slot=%d disconnected; ghost reserved\n", player.slot);
    }

    if (player.joinWindow != -1) {
        windowDestroy(player.joinWindow);
    }

    int slot = player.slot;
    Object* actor = player.actor;
    bool humanOwned = player.humanOwned;
    bool slotLocked = player.slotLocked;
    LocalCoopUiMode uiMode = player.uiMode;
    int activeHand = player.activeHand;
    int archetype = player.archetype;
    int gender = player.gender;
    LocalCoopActionMode actionMode = player.actionMode;
    bool controllerInputActive = player.controllerInputActive;
    char controllerGuid[64] {};
    snprintf(controllerGuid, sizeof(controllerGuid), "%s", player.controllerGuid);

    player = LocalCoopPlayer {};
    player.slot = slot;
    player.actor = actor;
    player.humanOwned = humanOwned;
    player.slotLocked = slotLocked;
    player.uiMode = uiMode;
    player.activeHand = activeHand;
    player.archetype = archetype;
    player.gender = gender;
    player.actionMode = actionMode;
    player.controllerInputActive = controllerInputActive;
    snprintf(player.controllerGuid, sizeof(player.controllerGuid), "%s", controllerGuid);
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
    gLocalCoopPlayers[0].slotLocked = true;

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
        if (player.joinWindow != -1) {
            windowDestroy(player.joinWindow);
        }
        if (player.slot > 0 && player.actor != nullptr) {
            player.actor->flags &= ~(OBJECT_NO_REMOVE | OBJECT_NO_SAVE);
        }
        player = LocalCoopPlayer {};
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

inline constexpr int kLocalCoopArchetypeCount = 4;
inline constexpr const char* kLocalCoopArchetypeNames[kLocalCoopArchetypeCount] = {
    "WASTELAND FIGHTER",
    "SCOUT",
    "MEDIC",
    "TECH SPECIALIST",
};

inline constexpr int kLocalCoopArchetypeStats[kLocalCoopArchetypeCount][PRIMARY_STAT_COUNT] = {
    { 7, 5, 7, 4, 5, 7, 5 },
    { 5, 8, 5, 4, 6, 8, 4 },
    { 4, 6, 5, 6, 8, 6, 5 },
    { 4, 6, 4, 5, 9, 6, 6 },
};

inline bool localCoopApplyPlayerOneArchetype(int archetype, int gender)
{
    if (gDude == nullptr) {
        return false;
    }

    archetype = std::max(0, std::min(archetype, kLocalCoopArchetypeCount - 1));
    gender = gender == GENDER_FEMALE ? GENDER_FEMALE : GENDER_MALE;

    for (int stat = 0; stat < PRIMARY_STAT_COUNT; stat++) {
        if (critterSetBaseStat(gDude, stat, kLocalCoopArchetypeStats[archetype][stat]) != 0) {
            return false;
        }
    }
    if (critterSetBaseStat(gDude, STAT_GENDER, gender) != 0) {
        return false;
    }

    critterUpdateDerivedStats(gDude);
    gDude->data.critter.hp = critterGetStat(gDude, STAT_MAXIMUM_HIT_POINTS);
    gDude->data.critter.combat.ap = critterGetStat(gDude, STAT_MAXIMUM_ACTION_POINTS);

    LocalCoopPlayer& player = gLocalCoopPlayers[0];
    player.slot = 0;
    player.actor = gDude;
    player.humanOwned = true;
    player.slotLocked = true;
    player.archetype = archetype;
    player.gender = gender;
    player.actionMode = LocalCoopActionMode::Interact;
    player.hexAimHeld = false;
    player.hexAimTile = -1;

    debugPrint("[COOP CREATE] P1 archetype=%s gender=%d\n",
        kLocalCoopArchetypeNames[archetype],
        gender);
    return true;
}

inline int localCoopFindSpawnTile(Object* anchor, int distance)
{
    if (anchor == nullptr || !tileIsValid(anchor->tile)) {
        return -1;
    }

    for (int ring = 1; ring <= distance; ring++) {
        for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
            int tile = tileGetTileInDirection(anchor->tile, rotation, ring);
            if (tileIsValid(tile)
                && _obj_blocking_at(anchor, tile, anchor->elevation) == nullptr) {
                return tile;
            }
        }
    }
    return -1;
}

inline bool localCoopCreatePlayerActor(int slot)
{
    if (slot <= 0 || slot >= kLocalCoopMaxPlayers || gDude == nullptr) {
        return false;
    }

    LocalCoopPlayer& player = gLocalCoopPlayers[slot];
    int archetype = std::max(0, std::min(player.archetype, kLocalCoopArchetypeCount - 1));
    int pid = protoConfigureLocalCoopPlayer(
        slot,
        kLocalCoopArchetypeStats[archetype],
        player.gender);
    if (pid == -1) {
        return false;
    }

    Object* actor = nullptr;
    if (objectCreateWithPid(&actor, pid) == -1 || actor == nullptr) {
        return false;
    }

    actor->flags |= OBJECT_NO_REMOVE | OBJECT_NO_SAVE | OBJECT_LIGHT_THRU;
    actor->flags &= ~OBJECT_HIDDEN;
    actor->data.critter.combat.results = 0;
    critterUpdateDerivedStats(actor);
    actor->data.critter.hp = critterGetStat(actor, STAT_MAXIMUM_HIT_POINTS);
    actor->data.critter.combat.ap = critterGetStat(actor, STAT_MAXIMUM_ACTION_POINTS);

    int spawnTile = localCoopFindSpawnTile(gDude, 3);
    if (spawnTile == -1
        || objectSetLocation(actor, spawnTile, gDude->elevation, nullptr) == -1) {
        actor->flags &= ~(OBJECT_NO_REMOVE | OBJECT_NO_SAVE);
        objectDestroy(actor, nullptr);
        return false;
    }

    player.actor = actor;
    player.humanOwned = true;
    player.slotLocked = true;
    player.archetype = archetype;

    LocalCoopCharacterSlotState& saved =
        localCoopCharacterStateGet().slots[slot];
    saved.locked = 1;
    saved.archetype = static_cast<uint8_t>(archetype);
    saved.gender = static_cast<uint8_t>(player.gender);
    snprintf(
        saved.controllerGuid,
        sizeof(saved.controllerGuid),
        "%s",
        player.controllerGuid);

    debugPrint(
        "[COOP JOIN] slot=%d locked archetype=%s pid=%d tile=%d\n",
        slot,
        kLocalCoopArchetypeNames[archetype],
        pid,
        spawnTile);
    return true;
}

inline void localCoopRestoreCharactersFromSave()
{
    localCoopCharacterStateEnsureInitialized();
    if (gLocalCoopAppliedCharacterStateRevision
        == gLocalCoopCharacterStateRevision) {
        return;
    }

    gLocalCoopAppliedCharacterStateRevision =
        gLocalCoopCharacterStateRevision;
    const LocalCoopCharacterState& saved =
        localCoopCharacterStateGetConst();

    for (int slot = 1; slot < kLocalCoopMaxPlayers; slot++) {
        LocalCoopPlayer& player = gLocalCoopPlayers[slot];
        if (player.actor != nullptr) {
            reg_anim_clear(player.actor);
            player.actor->flags &= ~(OBJECT_NO_REMOVE | OBJECT_NO_SAVE);
            objectDestroy(player.actor, nullptr);
            player.actor = nullptr;
        }

        const LocalCoopCharacterSlotState& savedSlot = saved.slots[slot];
        player.slotLocked = savedSlot.locked != 0;
        player.humanOwned = false;
        player.archetype = std::max(
            0,
            std::min(
                static_cast<int>(savedSlot.archetype),
                kLocalCoopArchetypeCount - 1));
        player.gender = savedSlot.gender == GENDER_FEMALE
            ? GENDER_FEMALE
            : GENDER_MALE;
        snprintf(
            player.controllerGuid,
            sizeof(player.controllerGuid),
            "%s",
            savedSlot.controllerGuid);

        if (!player.slotLocked || !localCoopCreatePlayerActor(slot)) {
            continue;
        }

        if (!player.connected || player.controller == nullptr) {
            player.actor->flags |= OBJECT_HIDDEN | OBJECT_NO_BLOCK;
        }
    }
}

inline void localCoopKeepReservedActorsWithParty()
{
    if (gDude == nullptr || !tileIsValid(gDude->tile)) {
        return;
    }

    for (int slot = 1; slot < kLocalCoopMaxPlayers; slot++) {
        LocalCoopPlayer& player = gLocalCoopPlayers[slot];
        Object* actor = player.actor;
        if (!player.slotLocked || actor == nullptr) {
            continue;
        }

        bool needsWarp = !tileIsValid(actor->tile)
            || actor->elevation != gDude->elevation
            || tileDistanceBetween(actor->tile, gDude->tile) > kLocalCoopCameraTetherTiles;
        if (needsWarp) {
            int spawnTile = localCoopFindSpawnTile(gDude, 3);
            if (spawnTile != -1) {
                reg_anim_clear(actor);
                objectSetLocation(actor, spawnTile, gDude->elevation, nullptr);
            }
        }

        if (player.connected && player.controller != nullptr) {
            actor->flags &= ~(OBJECT_HIDDEN | OBJECT_NO_BLOCK);
            player.humanOwned = true;
        } else {
            actor->flags |= OBJECT_HIDDEN | OBJECT_NO_BLOCK;
            player.humanOwned = true;
        }
    }
}

inline void localCoopDrawJoinMenu(LocalCoopPlayer& player)
{
    if (player.joinWindow == -1) {
        return;
    }

    windowFill(player.joinWindow, 0, 0, 420, 230, _colorTable[0]);
    windowDrawBorder(player.joinWindow);

    char title[64];
    snprintf(title, sizeof(title), "PLAYER %d - CREATE CHARACTER", player.slot + 1);
    windowDrawText(player.joinWindow, title, 380, 20, 18, _colorTable[992]);
    windowDrawText(
        player.joinWindow,
        "LEFT/RIGHT: ARCHETYPE    Y: GENDER",
        380,
        20,
        48,
        _colorTable[992]);

    char choice[96];
    snprintf(
        choice,
        sizeof(choice),
        "<  %s  >",
        kLocalCoopArchetypeNames[player.archetype]);
    windowDrawText(player.joinWindow, choice, 380, 20, 92, _colorTable[992]);

    const int* stats = kLocalCoopArchetypeStats[player.archetype];
    char special[128];
    snprintf(
        special,
        sizeof(special),
        "ST %d  PE %d  EN %d  CH %d  IN %d  AG %d  LK %d",
        stats[STAT_STRENGTH],
        stats[STAT_PERCEPTION],
        stats[STAT_ENDURANCE],
        stats[STAT_CHARISMA],
        stats[STAT_INTELLIGENCE],
        stats[STAT_AGILITY],
        stats[STAT_LUCK]);
    windowDrawText(player.joinWindow, special, 380, 20, 126, _colorTable[992]);
    windowDrawText(
        player.joinWindow,
        player.gender == GENDER_FEMALE ? "GENDER: FEMALE" : "GENDER: MALE",
        380,
        20,
        154,
        _colorTable[992]);
    windowDrawText(
        player.joinWindow,
        "A: JOIN AND LOCK SLOT    B: CANCEL",
        380,
        20,
        194,
        _colorTable[992]);
    windowRefresh(player.joinWindow);
}

inline void localCoopOpenJoinMenu(LocalCoopPlayer& player)
{
    if (player.joinMenuActive || player.slot <= 0 || player.slotLocked) {
        return;
    }

    player.joinWindow = windowCreate(
        (screenGetWidth() - 420) / 2,
        (screenGetVisibleHeight() - 230) / 2,
        420,
        230,
        _colorTable[0],
        WINDOW_MOVE_ON_TOP);
    if (player.joinWindow == -1) {
        return;
    }

    player.joinMenuActive = true;
    localCoopDrawJoinMenu(player);
}

inline void localCoopCloseJoinMenu(LocalCoopPlayer& player)
{
    if (player.joinWindow != -1) {
        windowDestroy(player.joinWindow);
    }
    player.joinWindow = -1;
    player.joinMenuActive = false;
}

inline void localCoopProcessJoinMenus()
{
    for (int slot = 1; slot < kLocalCoopMaxPlayers; slot++) {
        LocalCoopPlayer& player = gLocalCoopPlayers[slot];
        if (!player.connected || player.controller == nullptr) {
            continue;
        }

        bool startDown =
            SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_START) != 0;
        bool leftDown =
            SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT) != 0;
        bool rightDown =
            SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) != 0;
        bool confirmDown =
            SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_A) != 0;
        bool cancelDown =
            SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_B) != 0;
        bool genderDown =
            SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_Y) != 0;

        if (!player.slotLocked
            && startDown
            && !player.joinStartWasDown
            && !player.joinMenuActive) {
            localCoopOpenJoinMenu(player);
        }

        bool dirty = false;
        if (player.joinMenuActive) {
            if (leftDown && !player.joinLeftWasDown) {
                player.archetype =
                    (player.archetype + kLocalCoopArchetypeCount - 1)
                    % kLocalCoopArchetypeCount;
                dirty = true;
            }
            if (rightDown && !player.joinRightWasDown) {
                player.archetype =
                    (player.archetype + 1) % kLocalCoopArchetypeCount;
                dirty = true;
            }
            if (genderDown && !player.joinGenderWasDown) {
                player.gender =
                    player.gender == GENDER_MALE ? GENDER_FEMALE : GENDER_MALE;
                dirty = true;
            }
            if (confirmDown && !player.joinConfirmWasDown) {
                if (localCoopCreatePlayerActor(slot)) {
                    localCoopCloseJoinMenu(player);
                }
            } else if (cancelDown && !player.joinCancelWasDown) {
                localCoopCloseJoinMenu(player);
            } else if (dirty) {
                localCoopDrawJoinMenu(player);
            }
        }

        player.joinStartWasDown = startDown;
        player.joinLeftWasDown = leftDown;
        player.joinRightWasDown = rightDown;
        player.joinConfirmWasDown = confirmDown;
        player.joinCancelWasDown = cancelDown;
        player.joinGenderWasDown = genderDown;
    }
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

inline bool localCoopSyncActiveHandVisual(int slot)
{
    if (slot < 0 || slot >= kLocalCoopMaxPlayers) {
        return false;
    }

    LocalCoopPlayer& player = gLocalCoopPlayers[slot];
    Object* actor = player.actor;
    if (actor == nullptr) {
        return false;
    }

    int hand = player.activeHand;
    if (hand != HAND_LEFT && hand != HAND_RIGHT) {
        hand = HAND_RIGHT;
        player.activeHand = hand;
    }

    Object* item = hand == HAND_LEFT ? critterGetItem1(actor) : critterGetItem2(actor);
    int weaponAnimationCode = 0;
    if (item != nullptr && itemGetType(item) == ITEM_TYPE_WEAPON) {
        weaponAnimationCode = weaponGetAnimationCode(item);
    }

    int fid = buildFid(FID_TYPE(actor->fid), actor->fid & 0xFFF, ANIM_STAND, weaponAnimationCode, actor->rotation + 1);
    if (!artExists(fid)) {
        debugPrint("[COOP HAND] visual FID missing slot=%d hand=%d itemPid=%d fid=%08X\n",
            slot,
            hand,
            item != nullptr ? item->pid : -1,
            fid);
        return false;
    }

    // Controller combat is realtime. Do not leave a queued Fallout hand-swap
    // animation between the logical equipped hand and the critter FID.
    reg_anim_clear(actor);
    _dude_stand(actor, actor->rotation, fid);
    return true;
}

inline bool localCoopSetActiveHand(int slot, int hand, bool animated = false)
{
    if (slot < 0 || slot >= kLocalCoopMaxPlayers || (hand != HAND_LEFT && hand != HAND_RIGHT)) {
        return false;
    }

    (void)animated;

    LocalCoopPlayer& player = gLocalCoopPlayers[slot];
    if (slot == 0 && player.actor == gDude && gInterfaceBarWindow != -1) {
        int current = interfaceGetCurrentHand();
        if (current != hand && interfaceBarSwapHands(false) != 0) {
            return false;
        }
    }

    player.activeHand = hand;
    localCoopSyncActiveHandVisual(slot);
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
        bool runButtonDown = SDL_GameControllerGetButton(
            player.controller,
            SDL_CONTROLLER_BUTTON_LEFTSHOULDER) != 0;
        int moveMagnitude = std::max(std::abs(player.moveX), std::abs(player.moveY));
        if (runButtonDown || moveMagnitude >= 24500) {
            player.wantsRun = true;
        } else if (moveMagnitude <= 18000) {
            player.wantsRun = false;
        }

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

        // A one-hex run animation is too short to settle into its run cycle and
        // looks like alternating walk/slide frames. Queue a short clear path
        // for running, while walking remains precise one-hex movement.
        int destination = actor->tile;
        int movementSteps = player.wantsRun ? 3 : 1;
        for (int step = 0; step < movementSteps; step++) {
            int candidate = tileGetTileInDirection(destination, rotation, 1);
            if (!tileIsValid(candidate)
                || !localCoopMoveRespectsSharedScreen(actor, candidate)
                || _obj_blocking_at(actor, candidate, actor->elevation) != nullptr) {
                break;
            }
            destination = candidate;
            if (isExitGridAt(destination, actor->elevation)) {
                break;
            }
        }
        if (destination == actor->tile) {
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

        if (isExitGridAt(destination, actor->elevation)) {
            localCoopMarkMapExitTile(destination);
            debugPrint(
                "[COOP MAP EXIT] slot=%d actorId=%d tile=%d elevation=%d\n",
                player.slot,
                actor->id,
                destination,
                actor->elevation);
            scriptsRequestWorldMap();
        }
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
    if (slot < 0 || slot >= kLocalCoopMaxPlayers || item == nullptr || (hand != HAND_LEFT && hand != HAND_RIGHT)) {
        return false;
    }

    LocalCoopPlayer& player = gLocalCoopPlayers[slot];
    Object* actor = player.actor;
    Object* sharedOwner = localCoopGetSharedInventoryOwner();
    if (actor == nullptr || sharedOwner == nullptr) {
        return false;
    }

    int previousActiveHand = localCoopGetActiveHand(player);
    Object* previousHandItem = hand == HAND_LEFT ? critterGetItem1(actor) : critterGetItem2(actor);
    bool borrowed = actor != sharedOwner && item->owner != actor;

    // Fallout's wield routine decides whether to apply the weapon FID from the
    // current interface hand. Select the requested hand first so the logical
    // slot, HUD hand, and critter weapon animation cannot diverge.
    if (!localCoopSetActiveHand(slot, hand, false)) {
        debugPrint("[COOP EQUIP] active-hand change failed slot=%d hand=%d pid=%d\n", slot, hand, item->pid);
        return false;
    }

    if (borrowed && itemMoveForce(sharedOwner, actor, item, 1) != 0) {
        localCoopSetActiveHand(slot, previousActiveHand, false);
        debugPrint("[COOP EQUIP] shared move failed slot=%d hand=%d pid=%d\n", slot, hand, item->pid);
        return false;
    }

    // The co-op inventory is live/non-pausing, so equip immediately instead of
    // relying on a reserved animation sequence that can collide with steering.
    int wieldRc = _invenWieldFunc(actor, item, hand, false);
    if (wieldRc != 0) {
        if (borrowed && item->owner == actor) {
            itemMoveForce(actor, sharedOwner, item, 1);
        }
        localCoopSetActiveHand(slot, previousActiveHand, false);
        debugPrint("[COOP EQUIP] wield failed slot=%d hand=%d pid=%d rc=%d actorFid=%08X\n",
            slot,
            hand,
            item->pid,
            wieldRc,
            actor->fid);
        return false;
    }

    if (previousHandItem != nullptr && previousHandItem != item && actor != sharedOwner && previousHandItem->owner == actor) {
        previousHandItem->flags &= ~OBJECT_IN_ANY_HAND;
        itemMoveForce(actor, sharedOwner, previousHandItem, 1);
    }

    localCoopSyncActiveHandVisual(slot);

    if (actor == gDude && gInterfaceBarWindow != -1) {
        interfaceUpdateItems(false, INTERFACE_ITEM_ACTION_DEFAULT, INTERFACE_ITEM_ACTION_DEFAULT);
    }

    Object* left = critterGetItem1(actor);
    Object* right = critterGetItem2(actor);
    debugPrint("[COOP EQUIP] success slot=%d hand=%d pid=%d flags=%08X leftPid=%d rightPid=%d actorFid=%08X\n",
        slot,
        hand,
        item->pid,
        item->flags,
        left != nullptr ? left->pid : -1,
        right != nullptr ? right->pid : -1,
        actor->fid);
    return true;
}

inline bool localCoopUnequipToSharedPool(int slot, int hand)
{
    if (slot < 0 || slot >= kLocalCoopMaxPlayers || (hand != HAND_LEFT && hand != HAND_RIGHT)) {
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

    int itemPid = item->pid;
    if (_invenUnwieldFunc(actor, hand, 0) != 0) {
        debugPrint("[COOP EQUIP] unwield failed slot=%d hand=%d pid=%d\n", slot, hand, itemPid);
        return false;
    }

    item->flags &= ~OBJECT_IN_ANY_HAND;
    if (actor != sharedOwner) {
        itemMoveForce(actor, sharedOwner, item, 1);
    }

    localCoopSyncActiveHandVisual(slot);

    if (actor == gDude && gInterfaceBarWindow != -1) {
        interfaceUpdateItems(false, INTERFACE_ITEM_ACTION_DEFAULT, INTERFACE_ITEM_ACTION_DEFAULT);
    }

    debugPrint("[COOP EQUIP] unequip success slot=%d hand=%d pid=%d\n", slot, hand, itemPid);
    return true;
}

} // namespace fallout

#endif /* LOCAL_COOP_H */
