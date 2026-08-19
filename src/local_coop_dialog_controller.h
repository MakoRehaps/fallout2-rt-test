#ifndef LOCAL_COOP_DIALOG_CONTROLLER_H
#define LOCAL_COOP_DIALOG_CONTROLLER_H

#include <SDL.h>

#include <algorithm>

#include "game.h"
#include "input.h"
#include "local_coop.h"
#include "window_manager.h"

namespace fallout {

// Stock Fallout dialogue creates its response buttons in the options window at
// this fixed location. Each option already has keyboard/event codes assigned:
//   mouse-enter: 1200 + option index
//   mouse-exit:  1300 + option index
//   activate:    '1' + option index (49 + index)
// Reuse those events directly instead of moving a virtual mouse cursor.
inline constexpr int kLocalCoopDialogOptionsProbeX = 130;
inline constexpr int kLocalCoopDialogOptionsProbeY = 338;
inline constexpr int kLocalCoopDialogOptionEnterBase = 1200;
inline constexpr int kLocalCoopDialogOptionExitBase = 1300;
inline constexpr int kLocalCoopDialogOptionActivateBase = 49;
inline constexpr int kLocalCoopDialogMaxControllerOptions = 9;

struct LocalCoopDialogControllerState {
    int selectedIndex = 0;
    int lastOptionCount = 0;
    bool upWasDown = false;
    bool downWasDown = false;
    bool confirmWasDown = false;
    bool activeLastTick = false;
};

inline LocalCoopDialogControllerState gLocalCoopDialogControllerState;
inline bool gLocalCoopDialogControllerTickerInstalled = false;

inline Window* localCoopDialogFindOptionsWindow()
{
    int windowId = windowGetAtPoint(
        kLocalCoopDialogOptionsProbeX,
        kLocalCoopDialogOptionsProbeY);
    if (windowId == -1) {
        return nullptr;
    }

    Window* window = windowGetWindow(windowId);
    if (window == nullptr) {
        return nullptr;
    }

    // Validate that this really is the dialogue options window by checking for
    // at least one of the stock option event codes rather than trusting screen
    // coordinates alone.
    for (Button* button = window->buttonListHead; button != nullptr; button = button->next) {
        if (button->mouseEnterEventCode >= kLocalCoopDialogOptionEnterBase
            && button->mouseEnterEventCode < kLocalCoopDialogOptionEnterBase + 30) {
            return window;
        }
    }

    return nullptr;
}

inline int localCoopDialogCountControllerOptions(Window* window)
{
    if (window == nullptr) {
        return 0;
    }

    int highestIndex = -1;
    for (Button* button = window->buttonListHead; button != nullptr; button = button->next) {
        int eventCode = button->mouseEnterEventCode;
        if (eventCode < kLocalCoopDialogOptionEnterBase
            || eventCode >= kLocalCoopDialogOptionEnterBase + 30) {
            continue;
        }

        int index = eventCode - kLocalCoopDialogOptionEnterBase;
        if (button->leftMouseUpEventCode != kLocalCoopDialogOptionActivateBase + index) {
            continue;
        }

        highestIndex = std::max(highestIndex, index);
    }

    // The original keyboard path only has the ten digit keys, and 0 has special
    // legacy behavior, so expose choices 1-9 through this controller adapter.
    return std::min(highestIndex + 1, kLocalCoopDialogMaxControllerOptions);
}

inline void localCoopDialogQueueSelectionVisual(int oldIndex, int newIndex)
{
    if (oldIndex >= 0) {
        enqueueInputEvent(kLocalCoopDialogOptionExitBase + oldIndex);
    }
    if (newIndex >= 0) {
        enqueueInputEvent(kLocalCoopDialogOptionEnterBase + newIndex);
    }
}

inline void localCoopDialogControllerReset()
{
    gLocalCoopDialogControllerState = LocalCoopDialogControllerState{};
}

inline void localCoopDialogControllerTick()
{
    LocalCoopDialogControllerState& state = gLocalCoopDialogControllerState;

    bool active = gLocalCoopInitialized
        && GameMode::isInGameMode(GameMode::kDialog)
        && gLocalCoopPlayers[0].connected
        && gLocalCoopPlayers[0].controller != nullptr
        && gLocalCoopPlayers[0].humanOwned;

    if (!active) {
        if (state.activeLastTick) {
            localCoopDialogControllerReset();
        }
        return;
    }

    state.activeLastTick = true;

    Window* optionsWindow = localCoopDialogFindOptionsWindow();
    int optionCount = localCoopDialogCountControllerOptions(optionsWindow);
    if (optionCount <= 0) {
        state.lastOptionCount = 0;
        state.selectedIndex = 0;
        return;
    }

    if (state.lastOptionCount != optionCount) {
        int oldIndex = state.lastOptionCount > 0 ? state.selectedIndex : -1;
        state.selectedIndex = std::max(0, std::min(state.selectedIndex, optionCount - 1));
        state.lastOptionCount = optionCount;
        localCoopDialogQueueSelectionVisual(oldIndex, state.selectedIndex);
    }

    SDL_GameController* controller = gLocalCoopPlayers[0].controller;
    bool upDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP) != 0;
    bool downDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN) != 0;
    bool confirmDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A) != 0;

    if (upDown && !state.upWasDown) {
        int oldIndex = state.selectedIndex;
        state.selectedIndex--;
        if (state.selectedIndex < 0) {
            state.selectedIndex = optionCount - 1;
        }
        localCoopDialogQueueSelectionVisual(oldIndex, state.selectedIndex);
    }

    if (downDown && !state.downWasDown) {
        int oldIndex = state.selectedIndex;
        state.selectedIndex++;
        if (state.selectedIndex >= optionCount) {
            state.selectedIndex = 0;
        }
        localCoopDialogQueueSelectionVisual(oldIndex, state.selectedIndex);
    }

    if (confirmDown && !state.confirmWasDown) {
        // Feed the exact same numeric event used by Fallout's keyboard dialogue
        // path. The stock dialogue code remains responsible for running the
        // option procedure, reactions, script state, and refreshing choices.
        enqueueInputEvent(kLocalCoopDialogOptionActivateBase + state.selectedIndex);
    }

    state.upWasDown = upDown;
    state.downWasDown = downDown;
    state.confirmWasDown = confirmDown;
}

inline void localCoopDialogControllerTicker()
{
    localCoopDialogControllerTick();
}

inline void localCoopDialogControllerEnsureTicker()
{
    if (!gLocalCoopDialogControllerTickerInstalled) {
        tickersAdd(localCoopDialogControllerTicker);
        gLocalCoopDialogControllerTickerInstalled = true;
    }
}

} // namespace fallout

#endif /* LOCAL_COOP_DIALOG_CONTROLLER_H */
