#ifndef LOCAL_COOP_ANALOG_AIM_H
#define LOCAL_COOP_ANALOG_AIM_H

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cmath>

#include "kb.h"
#include "local_coop.h"
#include "local_coop_focus.h"
#include "map.h"
#include "object.h"
#include "svga.h"
#include "tile.h"

namespace fallout {

// Continuous analog movement keeps a short path queued and rebuilds it only
// when the requested direction changes. Aiming remains full-angle screen-space
// rather than being reduced to one of Fallout's six hex directions.
inline constexpr float kLocalCoopRadialDeadzone = 0.20f;
inline constexpr int kLocalCoopWalkLookAheadTiles = 4;
inline constexpr int kLocalCoopRunLookAheadTiles = 7;
inline constexpr float kLocalCoopAutoRunMagnitude = 0.88f;
inline constexpr float kLocalCoopAimBeadLength = 170.0f;

struct LocalCoopAnalogState {
    bool steering = false;
    int steeringRotation = -1;
    float moveMagnitude = 0.0f;
    float aimMagnitude = 0.0f;
    float moveX = 0.0f;
    float moveY = 0.0f;
    float aimX = 0.0f;
    float aimY = 0.0f;
};

inline std::array<LocalCoopAnalogState, kLocalCoopMaxPlayers> gLocalCoopAnalogStates;

inline float localCoopNormalizeControllerAxis(Sint16 value)
{
    if (value >= 0) {
        return std::min(1.0f, static_cast<float>(value) / 32767.0f);
    }

    return std::max(-1.0f, static_cast<float>(value) / 32768.0f);
}

inline float localCoopReadRadialStick(SDL_GameController* controller,
    SDL_GameControllerAxis axisX,
    SDL_GameControllerAxis axisY,
    float& outX,
    float& outY)
{
    outX = 0.0f;
    outY = 0.0f;
    if (controller == nullptr) {
        return 0.0f;
    }

    float x = localCoopNormalizeControllerAxis(SDL_GameControllerGetAxis(controller, axisX));
    float y = localCoopNormalizeControllerAxis(SDL_GameControllerGetAxis(controller, axisY));
    float magnitude = std::sqrt(x * x + y * y);
    if (magnitude <= kLocalCoopRadialDeadzone) {
        return 0.0f;
    }

    float clampedMagnitude = std::min(1.0f, magnitude);
    float scaledMagnitude = (clampedMagnitude - kLocalCoopRadialDeadzone) / (1.0f - kLocalCoopRadialDeadzone);
    float inverseMagnitude = 1.0f / magnitude;
    outX = x * inverseMagnitude * scaledMagnitude;
    outY = y * inverseMagnitude * scaledMagnitude;
    return scaledMagnitude;
}

inline int localCoopDirectionFromNormalizedStick(float x, float y)
{
    if (std::fabs(x) < 0.001f && std::fabs(y) < 0.001f) {
        return -1;
    }

    double angle = std::atan2(static_cast<double>(-y), static_cast<double>(x));
    if (angle < 0.0) {
        angle += 6.28318530717958647692;
    }

    int sector = static_cast<int>(std::floor((angle + 0.52359877559829887308) / 1.04719755119659774615)) % 6;
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

inline int localCoopFindSteeringDestination(Object* actor, int rotation, int maxDistance)
{
    if (actor == nullptr || rotation < 0) {
        return -1;
    }

    int best = -1;
    for (int distance = 1; distance <= maxDistance; distance++) {
        int tile = tileGetTileInDirection(actor->tile, rotation, distance);
        if (!tileIsValid(tile)
            || !localCoopMoveRespectsSharedScreen(actor, tile)
            || _obj_blocking_at(actor, tile, actor->elevation) != nullptr) {
            break;
        }
        best = tile;
    }

    return best;
}

inline void localCoopStopAnalogSteering(LocalCoopPlayer& player, LocalCoopAnalogState& state)
{
    if (state.steering && player.actor != nullptr && animationIsBusy(player.actor)) {
        reg_anim_clear(player.actor);
    }
    state.steering = false;
    state.steeringRotation = -1;
}

inline void localCoopUpdateAnalogAxes(LocalCoopPlayer& player)
{
    LocalCoopAnalogState& state = gLocalCoopAnalogStates[player.slot];
    state.moveMagnitude = localCoopReadRadialStick(player.controller,
        SDL_CONTROLLER_AXIS_LEFTX,
        SDL_CONTROLLER_AXIS_LEFTY,
        state.moveX,
        state.moveY);
    state.aimMagnitude = localCoopReadRadialStick(player.controller,
        SDL_CONTROLLER_AXIS_RIGHTX,
        SDL_CONTROLLER_AXIS_RIGHTY,
        state.aimX,
        state.aimY);

    player.moveX = static_cast<int>(state.moveX * 32767.0f);
    player.moveY = static_cast<int>(state.moveY * 32767.0f);
    player.aimX = static_cast<int>(state.aimX * 32767.0f);
    player.aimY = static_cast<int>(state.aimY * 32767.0f);
}

inline bool localCoopPlayerOneKeyboardCanMove(const LocalCoopPlayer& player)
{
    return player.slot == 0
        && player.humanOwned
        && player.actor != nullptr
        && player.uiMode == LocalCoopUiMode::World
        && (player.actor->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) == 0;
}

inline void localCoopApplyPlayerOneKeyboardMovement(LocalCoopPlayer& player)
{
    if (!localCoopPlayerOneKeyboardCanMove(player)) {
        return;
    }

    int x = 0;
    int y = 0;
    if (gPressedPhysicalKeys[SDL_SCANCODE_A]) {
        x--;
    }
    if (gPressedPhysicalKeys[SDL_SCANCODE_D]) {
        x++;
    }
    if (gPressedPhysicalKeys[SDL_SCANCODE_W]) {
        y--;
    }
    if (gPressedPhysicalKeys[SDL_SCANCODE_S]) {
        y++;
    }

    if (x == 0 && y == 0) {
        return;
    }

    LocalCoopAnalogState& state = gLocalCoopAnalogStates[0];
    float magnitude = std::sqrt(static_cast<float>(x * x + y * y));
    state.moveX = static_cast<float>(x) / magnitude;
    state.moveY = static_cast<float>(y) / magnitude;
    state.moveMagnitude = 1.0f;

    player.moveX = static_cast<int>(state.moveX * 32767.0f);
    player.moveY = static_cast<int>(state.moveY * 32767.0f);
    player.wantsRun = gPressedPhysicalKeys[SDL_SCANCODE_LSHIFT]
        || gPressedPhysicalKeys[SDL_SCANCODE_RSHIFT]
        || (player.controller != nullptr
            && SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER) != 0);
}

inline void localCoopAnalogSteerPlayer(LocalCoopPlayer& player)
{
    LocalCoopAnalogState& state = gLocalCoopAnalogStates[player.slot];
    Object* actor = player.actor;
    bool canMove = localCoopPlayerCanMove(player) || localCoopPlayerOneKeyboardCanMove(player);
    if (!canMove || actor == nullptr) {
        localCoopStopAnalogSteering(player, state);
        return;
    }

    int rotation = localCoopDirectionFromNormalizedStick(state.moveX, state.moveY);
    if (rotation == -1 || state.moveMagnitude <= 0.0f) {
        localCoopStopAnalogSteering(player, state);
        return;
    }

    // Weapon animations temporarily own the actor, but danger/combat state does
    // not. As soon as the attack finishes the same movement input resumes.
    bool attacking = false;
    if (player.controller != nullptr) {
        int rightTrigger = SDL_GameControllerGetAxis(player.controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
        attacking = rightTrigger > 12000
            || SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) != 0;
    }
    if (attacking) {
        state.steering = false;
        state.steeringRotation = -1;
        return;
    }

    if (animationIsBusy(actor)) {
        if (state.steering && rotation == state.steeringRotation) {
            return;
        }

        reg_anim_clear(actor);
    }

    bool run = player.wantsRun || state.moveMagnitude >= kLocalCoopAutoRunMagnitude;
    int lookAhead = run ? kLocalCoopRunLookAheadTiles : kLocalCoopWalkLookAheadTiles;
    int destination = localCoopFindSteeringDestination(actor, rotation, lookAhead);
    if (!tileIsValid(destination)) {
        state.steering = false;
        state.steeringRotation = -1;
        return;
    }

    if (reg_anim_begin(ANIMATION_REQUEST_UNRESERVED | ANIMATION_REQUEST_INSIGNIFICANT) == -1) {
        return;
    }

    int rc = run
        ? animationRegisterRunToTile(actor, destination, actor->elevation, -1, 0)
        : animationRegisterMoveToTile(actor, destination, actor->elevation, -1, 0);
    if (rc == -1) {
        reg_anim_clear(actor);
        state.steering = false;
        state.steeringRotation = -1;
        return;
    }

    reg_anim_end();
    state.steering = true;
    state.steeringRotation = rotation;
}

inline bool localCoopControllerHasGameplayInput(const LocalCoopPlayer& player)
{
    if (!player.connected || player.controller == nullptr) {
        return false;
    }

    const LocalCoopAnalogState& state = gLocalCoopAnalogStates[player.slot];
    if (state.moveMagnitude > 0.0f || state.aimMagnitude > 0.0f
        || SDL_GameControllerGetAxis(player.controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 12000
        || SDL_GameControllerGetAxis(player.controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 12000) {
        return true;
    }

    for (int button = 0; button < SDL_CONTROLLER_BUTTON_MAX; button++) {
        if (SDL_GameControllerGetButton(
                player.controller,
                static_cast<SDL_GameControllerButton>(button))
            != 0) {
            return true;
        }
    }

    return false;
}

inline void localCoopAnalogAimPreRuntimeTick()
{
    if (!gLocalCoopInitialized) {
        localCoopInit();
    }

    localCoopRefreshControllers();
    localCoopRefreshActorBindings();

    for (LocalCoopPlayer& player : gLocalCoopPlayers) {
        bool hasController = player.connected && player.controller != nullptr;
        if (!hasController && player.slot != 0) {
            continue;
        }

        player.wantsRun = hasController
            && SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER) != 0;
        localCoopUpdateAnalogAxes(player);
        if (player.slot == 0) {
            if (hasController && localCoopControllerHasGameplayInput(player)) {
                player.controllerInputActive = true;
            }
            localCoopApplyPlayerOneKeyboardMovement(player);
        }
        localCoopAnalogSteerPlayer(player);
    }
}

inline void localCoopAnalogAimPostRuntimeTick()
{
    for (LocalCoopPlayer& player : gLocalCoopPlayers) {
        bool hasController = player.connected && player.controller != nullptr;
        if (!hasController && player.slot != 0) {
            continue;
        }
        localCoopUpdateAnalogAxes(player);
        if (player.slot == 0) {
            localCoopApplyPlayerOneKeyboardMovement(player);
        }
    }
}

inline bool localCoopAimBeadObjectPoint(Object* object, int& x, int& y)
{
    if (object == nullptr || !tileIsValid(object->tile)) {
        return false;
    }

    if (tileToScreenXY(object->tile, &x, &y, object->elevation) != 0) {
        return false;
    }

    x += 16;
    y += 8;
    return true;
}

inline Object* localCoopAimBeadTarget(LocalCoopPlayer& player)
{
    LocalCoopFocusSlot& focus = gLocalCoopFocusSlots[player.slot];

    // Prefer an acquired hostile target whenever it is still valid, regardless
    // of any old Fallout combat bit. Otherwise show the normal interaction focus.
    if (localCoopFocusIsEnemy(player.actor, focus.combatTarget)) {
        return focus.combatTarget;
    }

    return focus.outlinedTarget;
}

inline void localCoopAimBeadDrawCross(SDL_Renderer* renderer, int x, int y)
{
    SDL_RenderDrawLine(renderer, x - 5, y, x + 5, y);
    SDL_RenderDrawLine(renderer, x, y - 5, x, y + 5);
}

inline void localCoopRenderHealthBars(SDL_Renderer* renderer, int maxX, int maxY)
{
    constexpr int kHealthBarWidth = 40;
    constexpr int kHealthBarHeight = 5;
    constexpr int kHealthBarHeadGap = 7;

    Object* object = objectFindFirst();
    while (object != nullptr) {
        if (PID_TYPE(object->pid) == OBJ_TYPE_CRITTER
            && tileIsValid(object->tile)
            && object->elevation == gElevation
            && (object->flags & OBJECT_HIDDEN) == 0
            && (object->data.critter.combat.results & DAM_DEAD) == 0) {
            int maximumHp = std::max(1, critterGetStat(object, STAT_MAXIMUM_HIT_POINTS));
            int currentHp = std::max(0, std::min(object->data.critter.hp, maximumHp));

            Rect objectRect;
            objectGetRect(object, &objectRect);

            int x = (objectRect.left + objectRect.right - kHealthBarWidth) / 2;
            int y = objectRect.top - kHealthBarHeadGap - kHealthBarHeight;
            x = std::max(1, std::min(maxX - kHealthBarWidth - 1, x));
            y = std::max(1, std::min(maxY - kHealthBarHeight - 1, y));

            SDL_Rect border = {
                x - 1,
                y - 1,
                kHealthBarWidth + 2,
                kHealthBarHeight + 2,
            };
            SDL_Rect background = {
                x,
                y,
                kHealthBarWidth,
                kHealthBarHeight,
            };
            SDL_Rect health = {
                x,
                y,
                kHealthBarWidth * currentHp / maximumHp,
                kHealthBarHeight,
            };

            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 220);
            SDL_RenderFillRect(renderer, &border);
            SDL_SetRenderDrawColor(renderer, 35, 35, 35, 220);
            SDL_RenderFillRect(renderer, &background);

            int healthPercent = currentHp * 100 / maximumHp;
            if (healthPercent > 50) {
                SDL_SetRenderDrawColor(renderer, 55, 220, 75, 240);
            } else if (healthPercent > 25) {
                SDL_SetRenderDrawColor(renderer, 235, 190, 45, 240);
            } else {
                SDL_SetRenderDrawColor(renderer, 235, 55, 55, 240);
            }
            if (health.w > 0) {
                SDL_RenderFillRect(renderer, &health);
            }
        }

        object = objectFindNext();
    }
}

// Called from svga.cc after Fallout's RGB world texture has already been copied
// to SDL's renderer. This is deliberately outside the 8-bit palette/GNW window
// system: holding the right stick can no longer blank the screen black/white.
inline void localCoopAimBeadRenderOverlay()
{
    if (gSdlRenderer == nullptr || isoIsDisabled()) {
        return;
    }

    Uint8 oldR = 0;
    Uint8 oldG = 0;
    Uint8 oldB = 0;
    Uint8 oldA = 0;
    SDL_GetRenderDrawColor(gSdlRenderer, &oldR, &oldG, &oldB, &oldA);

    SDL_BlendMode oldBlendMode = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(gSdlRenderer, &oldBlendMode);
    SDL_SetRenderDrawBlendMode(gSdlRenderer, SDL_BLENDMODE_BLEND);

    int maxX = std::max(0, screenGetWidth() - 1);
    int maxY = std::max(0, screenGetVisibleHeight() - 1);

    // Every living critter on the current elevation gets a compact bar anchored
    // to its rendered sprite rectangle, so players, companions, neutral NPCs,
    // and enemies all use the same readable world-space health display.
    localCoopRenderHealthBars(gSdlRenderer, maxX, maxY);

    for (LocalCoopPlayer& player : gLocalCoopPlayers) {
        if (!player.connected
            || !player.humanOwned
            || player.actor == nullptr
            || player.uiMode != LocalCoopUiMode::World) {
            continue;
        }

        LocalCoopAnalogState& analog = gLocalCoopAnalogStates[player.slot];
        if (analog.aimMagnitude <= 0.0f) {
            continue;
        }

        int startX = 0;
        int startY = 0;
        if (!localCoopAimBeadObjectPoint(player.actor, startX, startY)) {
            continue;
        }

        Object* target = localCoopAimBeadTarget(player);
        int endX = startX;
        int endY = startY;
        bool hasTargetPoint = target != nullptr && localCoopAimBeadObjectPoint(target, endX, endY);
        if (!hasTargetPoint) {
            endX = startX + static_cast<int>(analog.aimX * kLocalCoopAimBeadLength);
            endY = startY + static_cast<int>(analog.aimY * kLocalCoopAimBeadLength);
        }

        startX = std::max(0, std::min(maxX, startX));
        startY = std::max(0, std::min(maxY, startY));
        endX = std::max(0, std::min(maxX, endX));
        endY = std::max(0, std::min(maxY, endY));

        bool hostile = target != nullptr && localCoopFocusIsEnemy(player.actor, target);
        if (hostile) {
            SDL_SetRenderDrawColor(gSdlRenderer, 255, 70, 70, 235);
        } else {
            SDL_SetRenderDrawColor(gSdlRenderer, 255, 255, 255, 225);
        }

        // Two parallel pixels make the bead readable after desktop scaling while
        // still remaining much thinner than the old full-window overlay.
        SDL_RenderDrawLine(gSdlRenderer, startX, startY, endX, endY);
        SDL_RenderDrawLine(gSdlRenderer, startX, startY + 1, endX, endY + 1);
        localCoopAimBeadDrawCross(gSdlRenderer, endX, endY);
    }

    SDL_SetRenderDrawBlendMode(gSdlRenderer, oldBlendMode);
    SDL_SetRenderDrawColor(gSdlRenderer, oldR, oldG, oldB, oldA);
}

inline void localCoopAimBeadDestroy()
{
    if (gSdlRenderOverlayProc == localCoopAimBeadRenderOverlay) {
        gSdlRenderOverlayProc = nullptr;
    }
}

inline void localCoopAimBeadTick()
{
    // Installation is cheap and persistent. The overlay callback itself checks
    // current right-stick magnitude every presented frame and draws nothing when
    // nobody is aiming, so releasing aim immediately restores the untouched world.
    gSdlRenderOverlayProc = localCoopAimBeadRenderOverlay;
}

} // namespace fallout

#endif /* LOCAL_COOP_ANALOG_AIM_H */
