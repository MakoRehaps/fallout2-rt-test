#ifndef LOCAL_COOP_GENERIC_UI_CONTROLLER_H
#define LOCAL_COOP_GENERIC_UI_CONTROLLER_H

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cstdlib>

#include "color.h"
#include "game.h"
#include "input.h"
#include "kb.h"
#include "local_coop.h"
#include "svga.h"
#include "window_manager.h"

namespace fallout {

inline constexpr int kLocalCoopGenericUiButtonCapacity = 128;
inline constexpr int kLocalCoopGenericMarkerWidth = 10;
inline constexpr int kLocalCoopGenericMarkerHeight = 14;

struct LocalCoopGenericUiControllerState {
    int selectedButtonId = -1;
    int activeWindowId = -1;
    bool upWasDown = false;
    bool downWasDown = false;
    bool leftWasDown = false;
    bool rightWasDown = false;
    bool confirmWasDown = false;
    bool cancelWasDown = false;
    bool activeLastTick = false;

    bool markerValid = false;
    int markerWindowId = -1;
    Rect markerRect {};
    int markerWidth = 0;
    int markerHeight = 0;
    std::array<unsigned char, kLocalCoopGenericMarkerWidth * kLocalCoopGenericMarkerHeight> markerPixels {};
};

inline LocalCoopGenericUiControllerState gLocalCoopGenericUiControllerState;
inline bool gLocalCoopGenericUiControllerTickerInstalled = false;

inline bool localCoopGenericUiModeActive()
{
    return GameMode::isInGameMode(GameMode::kLoot)
        || GameMode::isInGameMode(GameMode::kBarter)
        || GameMode::isInGameMode(GameMode::kEditor)
        || GameMode::isInGameMode(GameMode::kHero);
}

inline bool localCoopGenericUiButtonActionable(const Button* button)
{
    if (button == nullptr || (button->flags & BUTTON_FLAG_DISABLED) != 0) {
        return false;
    }

    return button->leftMouseUpEventCode != -1
        || button->leftMouseUpProc != nullptr;
}

inline int localCoopGenericUiCollectButtons(Window* window,
    std::array<Button*, kLocalCoopGenericUiButtonCapacity>& buttons)
{
    if (window == nullptr) {
        return 0;
    }

    int count = 0;
    for (Button* button = window->buttonListHead;
         button != nullptr && count < kLocalCoopGenericUiButtonCapacity;
         button = button->next) {
        if (localCoopGenericUiButtonActionable(button)) {
            buttons[count++] = button;
        }
    }

    return count;
}

inline Window* localCoopGenericUiFindBestWindow()
{
    const int width = screenGetWidth();
    const int height = screenGetHeight();
    if (width <= 0 || height <= 0) {
        return nullptr;
    }

    const std::array<std::array<int, 2>, 9> probes = { {
        { width / 2, height / 2 },
        { width / 4, height / 4 },
        { width * 3 / 4, height / 4 },
        { width / 4, height * 3 / 4 },
        { width * 3 / 4, height * 3 / 4 },
        { width / 2, height / 4 },
        { width / 2, height * 3 / 4 },
        { width / 4, height / 2 },
        { width * 3 / 4, height / 2 },
    } };

    Window* bestWindow = nullptr;
    int bestButtonCount = 0;
    std::array<int, 9> seenIds {};
    int seenCount = 0;

    for (const auto& probe : probes) {
        int windowId = windowGetAtPoint(probe[0], probe[1]);
        if (windowId <= 0) {
            continue;
        }

        bool seen = false;
        for (int index = 0; index < seenCount; index++) {
            if (seenIds[index] == windowId) {
                seen = true;
                break;
            }
        }
        if (seen) {
            continue;
        }
        seenIds[seenCount++] = windowId;

        Window* window = windowGetWindow(windowId);
        std::array<Button*, kLocalCoopGenericUiButtonCapacity> buttons {};
        int count = localCoopGenericUiCollectButtons(window, buttons);
        if (count > bestButtonCount) {
            bestButtonCount = count;
            bestWindow = window;
        }
    }

    return bestWindow;
}

inline void localCoopGenericUiRestoreMarker()
{
    LocalCoopGenericUiControllerState& state = gLocalCoopGenericUiControllerState;
    if (!state.markerValid || state.markerWindowId == -1) {
        return;
    }

    Window* window = windowGetWindow(state.markerWindowId);
    if (window != nullptr && window->buffer != nullptr) {
        for (int y = 0; y < state.markerHeight; y++) {
            unsigned char* dest = window->buffer
                + (state.markerRect.top + y) * window->width
                + state.markerRect.left;
            const unsigned char* src = state.markerPixels.data()
                + y * kLocalCoopGenericMarkerWidth;
            std::copy(src, src + state.markerWidth, dest);
        }
        windowRefreshRect(window->id, &state.markerRect);
    }

    state.markerValid = false;
    state.markerWindowId = -1;
    state.markerWidth = 0;
    state.markerHeight = 0;
}

inline void localCoopGenericUiDrawMarker(Window* window, Button* button)
{
    localCoopGenericUiRestoreMarker();

    if (window == nullptr || button == nullptr || window->buffer == nullptr) {
        return;
    }

    LocalCoopGenericUiControllerState& state = gLocalCoopGenericUiControllerState;

    int left = std::max(0, button->rect.left - kLocalCoopGenericMarkerWidth - 2);
    int top = std::max(0, button->rect.top);
    int width = std::min(kLocalCoopGenericMarkerWidth, window->width - left);
    int buttonHeight = button->rect.bottom - button->rect.top + 1;
    int height = std::min(kLocalCoopGenericMarkerHeight,
        std::min(buttonHeight, window->height - top));
    if (width <= 0 || height <= 0) {
        return;
    }

    state.markerRect.left = left;
    state.markerRect.top = top;
    state.markerRect.right = left + width - 1;
    state.markerRect.bottom = top + height - 1;
    state.markerWidth = width;
    state.markerHeight = height;
    state.markerWindowId = window->id;

    for (int y = 0; y < height; y++) {
        const unsigned char* src = window->buffer + (top + y) * window->width + left;
        unsigned char* dest = state.markerPixels.data() + y * kLocalCoopGenericMarkerWidth;
        std::copy(src, src + width, dest);
    }

    state.markerValid = true;
    windowDrawText(window->id, ">", width, left, top, _colorTable[992]);
    windowRefreshRect(window->id, &state.markerRect);
}

inline Button* localCoopGenericUiFindButtonById(
    const std::array<Button*, kLocalCoopGenericUiButtonCapacity>& buttons,
    int count,
    int id)
{
    for (int index = 0; index < count; index++) {
        if (buttons[index] != nullptr && buttons[index]->id == id) {
            return buttons[index];
        }
    }

    return nullptr;
}

inline bool localCoopGenericUiHasReleaseEvent(
    const std::array<Button*, kLocalCoopGenericUiButtonCapacity>& buttons,
    int count,
    int eventCode)
{
    for (int index = 0; index < count; index++) {
        if (buttons[index] != nullptr
            && buttons[index]->leftMouseUpEventCode == eventCode) {
            return true;
        }
    }

    return false;
}

inline bool localCoopGenericUiIsPerkPicker(
    const std::array<Button*, kLocalCoopGenericUiButtonCapacity>& buttons,
    int count)
{
    // The character-editor perk/trait/Tag pickers share the same stock event
    // layout: 500 Done, 501 mouse-list area, 502 Cancel, and 574/575 arrows.
    // Event 501 is intentionally mouse-position based, so controller input must
    // bypass it and feed the picker's native keyboard list events instead.
    return GameMode::isInGameMode(GameMode::kEditor)
        && localCoopGenericUiHasReleaseEvent(buttons, count, 500)
        && localCoopGenericUiHasReleaseEvent(buttons, count, 501)
        && localCoopGenericUiHasReleaseEvent(buttons, count, 502);
}

inline Button* localCoopGenericUiDefaultButton(
    const std::array<Button*, kLocalCoopGenericUiButtonCapacity>& buttons,
    int count)
{
    Button* best = nullptr;
    for (int index = 0; index < count; index++) {
        Button* candidate = buttons[index];
        if (candidate == nullptr) {
            continue;
        }

        if (best == nullptr
            || candidate->rect.top < best->rect.top
            || (candidate->rect.top == best->rect.top && candidate->rect.left < best->rect.left)) {
            best = candidate;
        }
    }

    return best;
}

inline Button* localCoopGenericUiMoveSelection(
    const std::array<Button*, kLocalCoopGenericUiButtonCapacity>& buttons,
    int count,
    Button* current,
    int directionX,
    int directionY)
{
    if (current == nullptr) {
        return localCoopGenericUiDefaultButton(buttons, count);
    }

    int currentX = (current->rect.left + current->rect.right) / 2;
    int currentY = (current->rect.top + current->rect.bottom) / 2;

    Button* best = nullptr;
    int bestScore = 0x7FFFFFFF;

    for (int index = 0; index < count; index++) {
        Button* candidate = buttons[index];
        if (candidate == nullptr || candidate == current) {
            continue;
        }

        int candidateX = (candidate->rect.left + candidate->rect.right) / 2;
        int candidateY = (candidate->rect.top + candidate->rect.bottom) / 2;
        int dx = candidateX - currentX;
        int dy = candidateY - currentY;

        if ((directionX < 0 && dx >= 0)
            || (directionX > 0 && dx <= 0)
            || (directionY < 0 && dy >= 0)
            || (directionY > 0 && dy <= 0)) {
            continue;
        }

        int forwardDistance = directionX != 0 ? std::abs(dx) : std::abs(dy);
        int crossDistance = directionX != 0 ? std::abs(dy) : std::abs(dx);
        int score = forwardDistance * 100 + crossDistance;
        if (score < bestScore) {
            bestScore = score;
            best = candidate;
        }
    }

    return best != nullptr ? best : current;
}

inline void localCoopGenericUiResetEdges()
{
    LocalCoopGenericUiControllerState& state = gLocalCoopGenericUiControllerState;
    state.upWasDown = false;
    state.downWasDown = false;
    state.leftWasDown = false;
    state.rightWasDown = false;
    state.confirmWasDown = false;
    state.cancelWasDown = false;
}

inline void localCoopGenericUiHandlePerkPicker(SDL_GameController* controller,
    LocalCoopGenericUiControllerState& state)
{
    localCoopGenericUiRestoreMarker();
    state.selectedButtonId = -1;

    bool upDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP) != 0;
    bool downDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN) != 0;
    bool leftDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT) != 0;
    bool rightDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) != 0;
    bool confirmDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A) != 0;
    bool cancelDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_B) != 0;

    if (upDown && !state.upWasDown) {
        enqueueInputEvent(KEY_ARROW_UP);
    }
    if (downDown && !state.downWasDown) {
        enqueueInputEvent(KEY_ARROW_DOWN);
    }
    if (leftDown && !state.leftWasDown) {
        enqueueInputEvent(KEY_PAGE_UP);
    }
    if (rightDown && !state.rightWasDown) {
        enqueueInputEvent(KEY_PAGE_DOWN);
    }
    if (confirmDown && !state.confirmWasDown) {
        enqueueInputEvent(KEY_RETURN);
    }
    if (cancelDown && !state.cancelWasDown) {
        enqueueInputEvent(KEY_ESCAPE);
    }

    state.upWasDown = upDown;
    state.downWasDown = downDown;
    state.leftWasDown = leftDown;
    state.rightWasDown = rightDown;
    state.confirmWasDown = confirmDown;
    state.cancelWasDown = cancelDown;
    state.activeLastTick = true;
}

inline void localCoopGenericUiControllerTick()
{
    LocalCoopGenericUiControllerState& state = gLocalCoopGenericUiControllerState;
    bool active = localCoopGenericUiModeActive();

    if (!active
        || !gLocalCoopInitialized
        || !gLocalCoopPlayers[0].connected
        || gLocalCoopPlayers[0].controller == nullptr) {
        if (state.activeLastTick) {
            localCoopGenericUiRestoreMarker();
        }
        state.selectedButtonId = -1;
        state.activeWindowId = -1;
        state.activeLastTick = false;
        localCoopGenericUiResetEdges();
        return;
    }

    SDL_GameController* controller = gLocalCoopPlayers[0].controller;
    Window* window = localCoopGenericUiFindBestWindow();
    if (window == nullptr) {
        state.activeLastTick = active;
        return;
    }

    std::array<Button*, kLocalCoopGenericUiButtonCapacity> buttons {};
    int buttonCount = localCoopGenericUiCollectButtons(window, buttons);
    if (buttonCount <= 0) {
        state.activeLastTick = active;
        return;
    }

    if (localCoopGenericUiIsPerkPicker(buttons, buttonCount)) {
        if (!state.activeLastTick || state.activeWindowId != window->id) {
            localCoopGenericUiResetEdges();
            state.activeWindowId = window->id;
        }
        localCoopGenericUiHandlePerkPicker(controller, state);
        return;
    }

    bool selectionChanged = false;
    if (!state.activeLastTick || state.activeWindowId != window->id) {
        localCoopGenericUiRestoreMarker();
        localCoopGenericUiResetEdges();
        state.activeWindowId = window->id;
        Button* first = localCoopGenericUiDefaultButton(buttons, buttonCount);
        state.selectedButtonId = first != nullptr ? first->id : -1;
        selectionChanged = true;
    }

    Button* current = localCoopGenericUiFindButtonById(buttons,
        buttonCount,
        state.selectedButtonId);
    if (current == nullptr) {
        current = localCoopGenericUiDefaultButton(buttons, buttonCount);
        state.selectedButtonId = current != nullptr ? current->id : -1;
        selectionChanged = true;
    }

    bool upDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP) != 0;
    bool downDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN) != 0;
    bool leftDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT) != 0;
    bool rightDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) != 0;
    bool confirmDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A) != 0;
    bool cancelDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_B) != 0;

    Button* next = current;
    if (upDown && !state.upWasDown) {
        next = localCoopGenericUiMoveSelection(buttons, buttonCount, current, 0, -1);
    } else if (downDown && !state.downWasDown) {
        next = localCoopGenericUiMoveSelection(buttons, buttonCount, current, 0, 1);
    } else if (leftDown && !state.leftWasDown) {
        next = localCoopGenericUiMoveSelection(buttons, buttonCount, current, -1, 0);
    } else if (rightDown && !state.rightWasDown) {
        next = localCoopGenericUiMoveSelection(buttons, buttonCount, current, 1, 0);
    }

    if (next != nullptr && next != current) {
        state.selectedButtonId = next->id;
        current = next;
        selectionChanged = true;
    }

    if (current != nullptr && (selectionChanged || !state.markerValid || state.markerWindowId != window->id)) {
        localCoopGenericUiDrawMarker(window, current);
    }

    if (confirmDown && !state.confirmWasDown && current != nullptr) {
        localCoopGenericUiRestoreMarker();
        _win_button_press_and_release(current->id);
    }

    if (cancelDown && !state.cancelWasDown) {
        localCoopGenericUiRestoreMarker();
        enqueueInputEvent(KEY_ESCAPE);
    }

    state.upWasDown = upDown;
    state.downWasDown = downDown;
    state.leftWasDown = leftDown;
    state.rightWasDown = rightDown;
    state.confirmWasDown = confirmDown;
    state.cancelWasDown = cancelDown;
    state.activeLastTick = active;
}

inline void localCoopGenericUiControllerTicker()
{
    localCoopGenericUiControllerTick();
}

inline void localCoopGenericUiControllerEnsureTicker()
{
    if (!gLocalCoopGenericUiControllerTickerInstalled) {
        tickersAdd(localCoopGenericUiControllerTicker);
        gLocalCoopGenericUiControllerTickerInstalled = true;
    }
}

} // namespace fallout

#endif /* LOCAL_COOP_GENERIC_UI_CONTROLLER_H */
