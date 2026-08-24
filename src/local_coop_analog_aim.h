#ifndef LOCAL_COOP_ANALOG_AIM_H
#define LOCAL_COOP_ANALOG_AIM_H

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cmath>

#include "color.h"
#include "local_coop.h"
#include "local_coop_focus.h"
#include "map.h"
#include "object.h"
#include "svga.h"
#include "tile.h"
#include "window_manager.h"

namespace fallout {

// The original controller slice issued a fresh one-hex animation after every
// completed step. That works, but it makes held-stick movement visibly stop and
// restart at every hex. This layer keeps a short path queued and only rebuilds
// it when the stick changes direction, producing continuous Fallout animation
// while preserving the engine's hex collision/path rules.
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
inline int gLocalCoopAimBeadWindow = -1;
inline int gLocalCoopAimBeadWidth = 0;
inline int gLocalCoopAimBeadHeight = 0;

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

    // Keep all of the existing combat/focus/interaction code fed from the same
    // radially-deadzoned stick instead of the old per-axis square deadzone.
    player.moveX = static_cast<int>(state.moveX * 32767.0f);
    player.moveY = static_cast<int>(state.moveY * 32767.0f);
    player.aimX = static_cast<int>(state.aimX * 32767.0f);
    player.aimY = static_cast<int>(state.aimY * 32767.0f);
}

inline void localCoopAnalogSteerPlayer(LocalCoopPlayer& player)
{
    LocalCoopAnalogState& state = gLocalCoopAnalogStates[player.slot];
    Object* actor = player.actor;
    if (!localCoopPlayerCanMove(player) || actor == nullptr) {
        localCoopStopAnalogSteering(player, state);
        return;
    }

    int rotation = localCoopDirectionFromNormalizedStick(state.moveX, state.moveY);
    if (rotation == -1 || state.moveMagnitude <= 0.0f) {
        localCoopStopAnalogSteering(player, state);
        return;
    }

    // Never cancel a weapon animation to steer. Movement resumes naturally as
    // soon as the attack animation finishes.
    int rightTrigger = SDL_GameControllerGetAxis(player.controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
    bool attacking = rightTrigger > 12000
        || SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) != 0;
    if (attacking) {
        state.steering = false;
        state.steeringRotation = -1;
        return;
    }

    if (animationIsBusy(actor)) {
        if (state.steering && rotation == state.steeringRotation) {
            return;
        }

        // A direction change should feel like steering, not like waiting for a
        // mouse-click path to finish. Cancel only when movement input is active.
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

// Runs before the legacy controller runtime. By occupying the actor with a
// multi-hex movement path first, the old one-hex polling code sees the actor as
// busy and does not inject its stop/start step.
inline void localCoopAnalogAimPreRuntimeTick()
{
    if (!gLocalCoopInitialized) {
        localCoopInit();
    }

    localCoopRefreshControllers();
    localCoopRefreshActorBindings();

    for (LocalCoopPlayer& player : gLocalCoopPlayers) {
        if (!player.connected || player.controller == nullptr) {
            continue;
        }

        player.wantsRun = SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER) != 0;
        localCoopUpdateAnalogAxes(player);
        localCoopAnalogSteerPlayer(player);
    }
}

// The legacy runtime samples controller axes internally, so refresh our radial
// values again afterward before focus/interaction selection runs.
inline void localCoopAnalogAimPostRuntimeTick()
{
    for (LocalCoopPlayer& player : gLocalCoopPlayers) {
        if (!player.connected || player.controller == nullptr) {
            continue;
        }
        localCoopUpdateAnalogAxes(player);
    }
}

inline void localCoopAimBeadDestroy()
{
    if (gLocalCoopAimBeadWindow != -1) {
        windowDestroy(gLocalCoopAimBeadWindow);
    }
    gLocalCoopAimBeadWindow = -1;
    gLocalCoopAimBeadWidth = 0;
    gLocalCoopAimBeadHeight = 0;
}

inline bool localCoopAimBeadEnsureWindow()
{
    if (gIsoWindow == -1 || isoIsDisabled()) {
        return false;
    }

    int width = windowGetWidth(gIsoWindow);
    int height = windowGetHeight(gIsoWindow);
    if (width <= 0 || height <= 0) {
        return false;
    }

    if (gLocalCoopAimBeadWindow != -1
        && (width != gLocalCoopAimBeadWidth || height != gLocalCoopAimBeadHeight)) {
        localCoopAimBeadDestroy();
    }

    if (gLocalCoopAimBeadWindow == -1) {
        Rect isoRect {};
        if (windowGetRect(gIsoWindow, &isoRect) != 0) {
            return false;
        }

        gLocalCoopAimBeadWindow = windowCreate(isoRect.left,
            isoRect.top,
            width,
            height,
            0,
            WINDOW_TRANSPARENT | WINDOW_MOVE_ON_TOP);
        if (gLocalCoopAimBeadWindow == -1) {
            return false;
        }

        gLocalCoopAimBeadWidth = width;
        gLocalCoopAimBeadHeight = height;
    }

    return true;
}

inline bool localCoopAimBeadObjectPoint(Object* object, int& x, int& y)
{
    if (object == nullptr || !tileIsValid(object->tile)) {
        return false;
    }

    if (tileToScreenXY(object->tile, &x, &y, object->elevation) != 0) {
        return false;
    }

    // Hex screen coordinates are the upper-left of the 32x16 tile diamond.
    x += 16;
    y += 8;
    return true;
}

inline void localCoopAimBeadDrawCross(int win, int x, int y, int color)
{
    windowDrawLine(win, x - 5, y, x + 5, y, color);
    windowDrawLine(win, x, y - 5, x, y + 5, color);
}

inline void localCoopAimBeadTick()
{
    bool anyVisible = false;
    for (const LocalCoopPlayer& player : gLocalCoopPlayers) {
        if (!player.connected || !player.humanOwned || player.actor == nullptr || player.uiMode != LocalCoopUiMode::World) {
            continue;
        }

        const LocalCoopAnalogState& analog = gLocalCoopAnalogStates[player.slot];
        const LocalCoopFocusSlot& focus = gLocalCoopFocusSlots[player.slot];
        if (analog.aimMagnitude > 0.0f || (isInCombat() && focus.combatTarget != nullptr)) {
            anyVisible = true;
            break;
        }
    }

    if (!anyVisible) {
        if (gLocalCoopAimBeadWindow != -1) {
            windowHide(gLocalCoopAimBeadWindow);
        }
        return;
    }

    if (!localCoopAimBeadEnsureWindow()) {
        return;
    }

    windowFill(gLocalCoopAimBeadWindow, 0, 0, gLocalCoopAimBeadWidth, gLocalCoopAimBeadHeight, 0);

    for (LocalCoopPlayer& player : gLocalCoopPlayers) {
        if (!player.connected || !player.humanOwned || player.actor == nullptr || player.uiMode != LocalCoopUiMode::World) {
            continue;
        }

        LocalCoopAnalogState& analog = gLocalCoopAnalogStates[player.slot];
        LocalCoopFocusSlot& focus = gLocalCoopFocusSlots[player.slot];
        Object* target = isInCombat() ? focus.combatTarget : focus.outlinedTarget;
        if (analog.aimMagnitude <= 0.0f && target == nullptr) {
            continue;
        }

        int startX = 0;
        int startY = 0;
        if (!localCoopAimBeadObjectPoint(player.actor, startX, startY)) {
            continue;
        }

        int endX = startX;
        int endY = startY;
        bool hasTargetPoint = target != nullptr && localCoopAimBeadObjectPoint(target, endX, endY);
        if (!hasTargetPoint) {
            endX = startX + static_cast<int>(analog.aimX * kLocalCoopAimBeadLength);
            endY = startY + static_cast<int>(analog.aimY * kLocalCoopAimBeadLength);
        }

        endX = std::max(0, std::min(gLocalCoopAimBeadWidth - 1, endX));
        endY = std::max(0, std::min(gLocalCoopAimBeadHeight - 1, endY));

        bool hostile = target != nullptr && localCoopFocusIsEnemy(player.actor, target);
        int color = hostile ? _colorTable[31744] : _colorTable[32767];
        windowDrawLine(gLocalCoopAimBeadWindow, startX, startY, endX, endY, color);
        localCoopAimBeadDrawCross(gLocalCoopAimBeadWindow, endX, endY, color);
    }

    windowShow(gLocalCoopAimBeadWindow);
    windowRefresh(gLocalCoopAimBeadWindow);
}

} // namespace fallout

#endif /* LOCAL_COOP_ANALOG_AIM_H */
