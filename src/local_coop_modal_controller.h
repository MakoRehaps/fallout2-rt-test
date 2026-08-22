#ifndef LOCAL_COOP_MODAL_CONTROLLER_H
#define LOCAL_COOP_MODAL_CONTROLLER_H

#include <SDL.h>

#include <algorithm>
#include <array>

#include "color.h"
#include "game.h"
#include "input.h"
#include "kb.h"
#include "local_coop.h"
#include "svga.h"
#include "window_manager.h"

namespace fallout {

inline constexpr int kLocalCoopPipboyFirstContentEvent = 505;
inline constexpr int kLocalCoopPipboyLastContentEvent = 528;
inline constexpr int kLocalCoopPipboyContentCapacity = 24;
inline constexpr int kLocalCoopPipboyMarkerWidth = 12;
inline constexpr int kLocalCoopPipboyMarkerHeight = 16;

struct LocalCoopModalControllerState {
    int skilldexIndex = 0;
    int pipboyIndex = 0;
    int pipboyTabIndex = 0;

    bool upWasDown = false;
    bool downWasDown = false;
    bool confirmWasDown = false;
    bool cancelWasDown = false;
    bool leftShoulderWasDown = false;
    bool rightShoulderWasDown = false;
    bool alarmWasDown = false;

    bool skilldexActiveLastTick = false;
    bool pipboyActiveLastTick = false;

    bool pipboyMarkerValid = false;
    int pipboyMarkerWindow = -1;
    Rect pipboyMarkerRect{};
    int pipboyMarkerWidth = 0;
    int pipboyMarkerHeight = 0;
    std::array<unsigned char, kLocalCoopPipboyMarkerWidth * kLocalCoopPipboyMarkerHeight> pipboyMarkerPixels{};
};

inline LocalCoopModalControllerState gLocalCoopModalControllerState;
inline bool gLocalCoopModalControllerTickerInstalled = false;

inline void localCoopPipboyRestoreMarker()
{
    LocalCoopModalControllerState& state = gLocalCoopModalControllerState;
    if (!state.pipboyMarkerValid || state.pipboyMarkerWindow == -1) {
        return;
    }

    Window* window = windowGetWindow(state.pipboyMarkerWindow);
    if (window != nullptr && window->buffer != nullptr) {
        for (int y = 0; y < state.pipboyMarkerHeight; y++) {
            unsigned char* dest = window->buffer
                + (state.pipboyMarkerRect.top + y) * window->width
                + state.pipboyMarkerRect.left;
            const unsigned char* src = state.pipboyMarkerPixels.data()
                + y * kLocalCoopPipboyMarkerWidth;
            std::copy(src, src + state.pipboyMarkerWidth, dest);
        }
        windowRefreshRect(window->id, &state.pipboyMarkerRect);
    }

    state.pipboyMarkerValid = false;
    state.pipboyMarkerWindow = -1;
    state.pipboyMarkerWidth = 0;
    state.pipboyMarkerHeight = 0;
}

inline Window* localCoopFindPipboyWindow()
{
    int windowId = windowGetAtPoint(screenGetWidth() / 2, screenGetHeight() / 2);
    if (windowId == -1) {
        return nullptr;
    }

    Window* window = windowGetWindow(windowId);
    if (window == nullptr) {
        return nullptr;
    }

    for (Button* button = window->buttonListHead; button != nullptr; button = button->next) {
        if (button->leftMouseUpEventCode >= 500 && button->leftMouseUpEventCode <= 528) {
            return window;
        }
    }

    return nullptr;
}

inline int localCoopCollectPipboyContentButtons(Window* window,
    std::array<Button*, kLocalCoopPipboyContentCapacity>& buttons)
{
    int count = 0;
    if (window == nullptr) {
        return 0;
    }

    for (Button* button = window->buttonListHead;
         button != nullptr && count < kLocalCoopPipboyContentCapacity;
         button = button->next) {
        int code = button->leftMouseUpEventCode;
        if (code >= kLocalCoopPipboyFirstContentEvent
            && code <= kLocalCoopPipboyLastContentEvent) {
            buttons[count++] = button;
        }
    }

    std::sort(buttons.begin(), buttons.begin() + count, [](const Button* lhs, const Button* rhs) {
        if (lhs->rect.top != rhs->rect.top) {
            return lhs->rect.top < rhs->rect.top;
        }
        return lhs->rect.left < rhs->rect.left;
    });

    return count;
}

inline void localCoopPipboyDrawMarker(Window* window, Button* button)
{
    localCoopPipboyRestoreMarker();

    if (window == nullptr || button == nullptr || window->buffer == nullptr) {
        return;
    }

    LocalCoopModalControllerState& state = gLocalCoopModalControllerState;

    int left = std::max(0, button->rect.left - kLocalCoopPipboyMarkerWidth - 2);
    int top = std::max(0, button->rect.top);
    int width = std::min(kLocalCoopPipboyMarkerWidth, window->width - left);
    int buttonHeight = button->rect.bottom - button->rect.top + 1;
    int height = std::min(kLocalCoopPipboyMarkerHeight,
        std::min(buttonHeight, window->height - top));
    if (width <= 0 || height <= 0) {
        return;
    }

    state.pipboyMarkerRect.left = left;
    state.pipboyMarkerRect.top = top;
    state.pipboyMarkerRect.right = left + width - 1;
    state.pipboyMarkerRect.bottom = top + height - 1;
    state.pipboyMarkerWidth = width;
    state.pipboyMarkerHeight = height;
    state.pipboyMarkerWindow = window->id;

    for (int y = 0; y < height; y++) {
        const unsigned char* src = window->buffer + (top + y) * window->width + left;
        unsigned char* dest = state.pipboyMarkerPixels.data() + y * kLocalCoopPipboyMarkerWidth;
        std::copy(src, src + width, dest);
    }

    state.pipboyMarkerValid = true;
    windowDrawText(window->id, ">", width, left, top, _colorTable[992]);
    windowRefreshRect(window->id, &state.pipboyMarkerRect);
}

inline void localCoopSkilldexControllerTick(SDL_GameController* controller)
{
    LocalCoopModalControllerState& state = gLocalCoopModalControllerState;

    bool upDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP) != 0;
    bool downDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN) != 0;
    bool confirmDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A) != 0;
    bool cancelDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_B) != 0;

    if (upDown && !state.upWasDown) {
        state.skilldexIndex--;
        if (state.skilldexIndex < 0) {
            state.skilldexIndex = 7;
        }
    }

    if (downDown && !state.downWasDown) {
        state.skilldexIndex++;
        if (state.skilldexIndex > 7) {
            state.skilldexIndex = 0;
        }
    }

    if (confirmDown && !state.confirmWasDown) {
        // Stock Skilldex skill events are 501-508.
        enqueueInputEvent(501 + state.skilldexIndex);
    }

    if (cancelDown && !state.cancelWasDown) {
        enqueueInputEvent(500);
    }

    state.upWasDown = upDown;
    state.downWasDown = downDown;
    state.confirmWasDown = confirmDown;
    state.cancelWasDown = cancelDown;
}

inline void localCoopPipboyControllerTick(SDL_GameController* controller)
{
    LocalCoopModalControllerState& state = gLocalCoopModalControllerState;

    Window* window = localCoopFindPipboyWindow();
    std::array<Button*, kLocalCoopPipboyContentCapacity> contentButtons{};
    int contentCount = localCoopCollectPipboyContentButtons(window, contentButtons);

    if (contentCount <= 0) {
        state.pipboyIndex = 0;
        localCoopPipboyRestoreMarker();
    } else {
        state.pipboyIndex = std::max(0, std::min(state.pipboyIndex, contentCount - 1));
    }

    bool upDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP) != 0;
    bool downDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN) != 0;
    bool confirmDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A) != 0;
    bool cancelDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_B) != 0;
    bool leftShoulderDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER) != 0;
    bool rightShoulderDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) != 0;
    bool alarmDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_Y) != 0;

    bool selectionMoved = false;
    if (contentCount > 0 && upDown && !state.upWasDown) {
        state.pipboyIndex--;
        if (state.pipboyIndex < 0) {
            state.pipboyIndex = contentCount - 1;
        }
        selectionMoved = true;
    }

    if (contentCount > 0 && downDown && !state.downWasDown) {
        state.pipboyIndex++;
        if (state.pipboyIndex >= contentCount) {
            state.pipboyIndex = 0;
        }
        selectionMoved = true;
    }

    if ((selectionMoved || !state.pipboyMarkerValid) && contentCount > 0) {
        localCoopPipboyDrawMarker(window, contentButtons[state.pipboyIndex]);
    }

    if (confirmDown && !state.confirmWasDown && contentCount > 0) {
        int eventCode = contentButtons[state.pipboyIndex]->leftMouseUpEventCode;
        localCoopPipboyRestoreMarker();
        enqueueInputEvent(eventCode);
        state.pipboyIndex = 0;
    }

    // Side buttons 500-502 are the three content tabs. 503 is stock close and
    // is deliberately handled by B/Escape instead of being part of the cycle.
    if (leftShoulderDown && !state.leftShoulderWasDown) {
        localCoopPipboyRestoreMarker();
        state.pipboyTabIndex--;
        if (state.pipboyTabIndex < 0) {
            state.pipboyTabIndex = 2;
        }
        enqueueInputEvent(500 + state.pipboyTabIndex);
        state.pipboyIndex = 0;
    }

    if (rightShoulderDown && !state.rightShoulderWasDown) {
        localCoopPipboyRestoreMarker();
        state.pipboyTabIndex++;
        if (state.pipboyTabIndex > 2) {
            state.pipboyTabIndex = 0;
        }
        enqueueInputEvent(500 + state.pipboyTabIndex);
        state.pipboyIndex = 0;
    }

    if (alarmDown && !state.alarmWasDown) {
        localCoopPipboyRestoreMarker();
        enqueueInputEvent(504);
        state.pipboyIndex = 0;
    }

    if (cancelDown && !state.cancelWasDown) {
        localCoopPipboyRestoreMarker();
        enqueueInputEvent(KEY_ESCAPE);
    }

    state.upWasDown = upDown;
    state.downWasDown = downDown;
    state.confirmWasDown = confirmDown;
    state.cancelWasDown = cancelDown;
    state.leftShoulderWasDown = leftShoulderDown;
    state.rightShoulderWasDown = rightShoulderDown;
    state.alarmWasDown = alarmDown;
}

inline void localCoopModalControllerResetInputEdges()
{
    LocalCoopModalControllerState& state = gLocalCoopModalControllerState;
    state.upWasDown = false;
    state.downWasDown = false;
    state.confirmWasDown = false;
    state.cancelWasDown = false;
    state.leftShoulderWasDown = false;
    state.rightShoulderWasDown = false;
    state.alarmWasDown = false;
}

inline void localCoopModalControllerTick()
{
    LocalCoopModalControllerState& state = gLocalCoopModalControllerState;

    if (!gLocalCoopInitialized
        || !gLocalCoopPlayers[0].connected
        || gLocalCoopPlayers[0].controller == nullptr
        || !gLocalCoopPlayers[0].humanOwned) {
        localCoopPipboyRestoreMarker();
        localCoopModalControllerResetInputEdges();
        state.skilldexActiveLastTick = false;
        state.pipboyActiveLastTick = false;
        return;
    }

    SDL_GameController* controller = gLocalCoopPlayers[0].controller;
    bool skilldexActive = GameMode::isInGameMode(GameMode::kSkilldex);
    bool pipboyActive = GameMode::isInGameMode(GameMode::kPipboy);

    if (skilldexActive) {
        if (!state.skilldexActiveLastTick) {
            localCoopModalControllerResetInputEdges();
            state.skilldexIndex = 0;
        }
        localCoopSkilldexControllerTick(controller);
    } else if (state.skilldexActiveLastTick) {
        localCoopModalControllerResetInputEdges();
    }

    if (pipboyActive) {
        if (!state.pipboyActiveLastTick) {
            localCoopModalControllerResetInputEdges();
            state.pipboyIndex = 0;
            state.pipboyTabIndex = 0;
        }
        localCoopPipboyControllerTick(controller);
    } else if (state.pipboyActiveLastTick) {
        localCoopPipboyRestoreMarker();
        localCoopModalControllerResetInputEdges();
    }

    state.skilldexActiveLastTick = skilldexActive;
    state.pipboyActiveLastTick = pipboyActive;
}

inline void localCoopModalControllerTicker()
{
    localCoopModalControllerTick();
}

inline void localCoopModalControllerEnsureTicker()
{
    if (!gLocalCoopModalControllerTickerInstalled) {
        tickersAdd(localCoopModalControllerTicker);
        gLocalCoopModalControllerTickerInstalled = true;
    }
}

} // namespace fallout

#endif /* LOCAL_COOP_MODAL_CONTROLLER_H */
