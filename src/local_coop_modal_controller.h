#ifndef LOCAL_COOP_MODAL_CONTROLLER_H
#define LOCAL_COOP_MODAL_CONTROLLER_H

#include <SDL.h>

#include "game.h"
#include "input.h"
#include "local_coop.h"

namespace fallout {

struct LocalCoopModalControllerState {
    int skilldexIndex = 0;
    bool upWasDown = false;
    bool downWasDown = false;
    bool confirmWasDown = false;
    bool cancelWasDown = false;
    bool activeLastTick = false;
};

inline LocalCoopModalControllerState gLocalCoopModalControllerState;
inline bool gLocalCoopModalControllerTickerInstalled = false;

inline void localCoopModalControllerReset()
{
    gLocalCoopModalControllerState = LocalCoopModalControllerState{};
}

inline void localCoopModalControllerTick()
{
    LocalCoopModalControllerState& state = gLocalCoopModalControllerState;

    bool active = gLocalCoopInitialized
        && gLocalCoopPlayers[0].connected
        && gLocalCoopPlayers[0].controller != nullptr
        && gLocalCoopPlayers[0].humanOwned
        && GameMode::isInGameMode(GameMode::kSkilldex);

    if (!active) {
        if (state.activeLastTick) {
            localCoopModalControllerReset();
        }
        return;
    }

    state.activeLastTick = true;

    SDL_GameController* controller = gLocalCoopPlayers[0].controller;
    bool upDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP) != 0;
    bool downDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN) != 0;
    bool confirmDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A) != 0;
    bool cancelDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_B) != 0;

    // Fallout's Skilldex exposes eight skill button events, 501-508, and 500
    // for cancel. Drive those existing events directly instead of moving a
    // pointer over the buttons.
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
