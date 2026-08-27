#ifndef LOCAL_COOP_GROUP_ROOM_H
#define LOCAL_COOP_GROUP_ROOM_H

#include <SDL.h>

#include <array>
#include <cstdio>

#include "color.h"
#include "input.h"
#include "local_coop.h"
#include "mouse.h"
#include "svga.h"
#include "text_font.h"
#include "window_manager.h"

namespace fallout {

// COOP_TILELESS_GROUP_ROOM_V1
// A pure UI scene shown before the real Fallout 1 opening. It deliberately
// creates no map, no tiles, no scripts, no critters and no world transitions.
// Controllers press Start once to JOIN, release it, then press Start again to
// vote READY. When every joined controller is ready, the caller continues into
// the normal opening movie and real campaign map.
inline bool localCoopRunTilelessGroupRoom()
{
    localCoopInit();

    std::array<bool, kLocalCoopMaxPlayers> joined {};
    std::array<bool, kLocalCoopMaxPlayers> ready {};
    std::array<bool, kLocalCoopMaxPlayers> startWasDown {};

    // P1 is always part of the session. If P1 has a controller, Start is still
    // used for the ready vote; keyboard Enter can ready P1 as a fallback.
    joined[0] = true;

    const int width = 720;
    const int height = 440;
    int win = windowCreate(
        (screenGetWidth() - width) / 2,
        (screenGetVisibleHeight() - height) / 2,
        width,
        height,
        _colorTable[0],
        WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    if (win == -1) {
        return true; // fail open rather than block a new game
    }

    bool oldCursorHidden = cursorIsHidden();
    mouseShowCursor();

    auto draw = [&]() {
        windowFill(win, 0, 0, width, height, _colorTable[0]);
        windowDrawBorder(win);
        windowDrawText(win, "PHOBOI CO-OP GROUP ROOM", width - 48, 24, 22, _colorTable[992]);
        windowDrawText(win, "NO MAP / NO TILES - FORM PARTY BEFORE THE VAULT INTRO", width - 48, 24, 52, _colorTable[992]);
        windowDrawText(win, "PRESS START TO JOIN. RELEASE. PRESS START AGAIN TO VOTE READY.", width - 48, 24, 82, _colorTable[32747]);

        for (int slot = 0; slot < kLocalCoopMaxPlayers; ++slot) {
            const LocalCoopPlayer& player = gLocalCoopPlayers[slot];
            char line[180];
            const char* state = ready[slot]
                ? "READY"
                : joined[slot]
                    ? "JOINED - PRESS START TO READY"
                    : player.connected
                        ? "PRESS START TO JOIN"
                        : "WAITING FOR CONTROLLER";
            if (slot == 0 && player.controller == nullptr && joined[slot] && !ready[slot]) {
                state = "JOINED - ENTER OR START TO READY";
            }
            std::snprintf(line, sizeof(line), "PLAYER %d   %s", slot + 1, state);
            windowDrawText(win, line, width - 72, 38, 132 + slot * 54,
                ready[slot] ? _colorTable[32747] : _colorTable[992]);
        }

        windowDrawText(win, "ALL JOINED PLAYERS MUST VOTE READY", width - 48, 24, height - 54, _colorTable[992]);
        windowDrawText(win, "ESC = CANCEL", width - 48, 24, height - 30, _colorTable[992]);
        windowRefresh(win);
    };

    draw();
    bool accepted = false;
    bool dirty = false;
    bool enterWasDown = false;

    while (_game_user_wants_to_quit == 0) {
        sharedFpsLimiter.mark();
        inputGetInput();
        localCoopRefreshControllers();

        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        bool enterDown = keys != nullptr && keys[SDL_SCANCODE_RETURN] != 0;
        bool escapeDown = keys != nullptr && keys[SDL_SCANCODE_ESCAPE] != 0;
        if (escapeDown) {
            break;
        }

        for (int slot = 0; slot < kLocalCoopMaxPlayers; ++slot) {
            LocalCoopPlayer& player = gLocalCoopPlayers[slot];
            bool startDown = player.controller != nullptr
                && SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_START) != 0;

            bool edge = startDown && !startWasDown[slot];
            if (slot == 0 && enterDown && !enterWasDown) {
                edge = true;
            }

            if (edge) {
                if (!joined[slot]) {
                    joined[slot] = true;
                    ready[slot] = false;
                    dirty = true;
                    debugPrint("[COOP GROUP] slot=%d joined\n", slot);
                } else {
                    ready[slot] = !ready[slot];
                    dirty = true;
                    debugPrint("[COOP GROUP] slot=%d ready=%d\n", slot, ready[slot] ? 1 : 0);
                }
            }
            startWasDown[slot] = startDown;
        }
        enterWasDown = enterDown;

        int joinedCount = 0;
        int readyCount = 0;
        for (int slot = 0; slot < kLocalCoopMaxPlayers; ++slot) {
            if (joined[slot]) {
                ++joinedCount;
                if (ready[slot]) ++readyCount;
            }
        }

        // Require P1 plus at least one ready vote. Solo remains possible: P1 can
        // ready and continue even if no additional controller joins.
        if (joinedCount > 0 && readyCount == joinedCount) {
            accepted = true;
            break;
        }

        if (dirty) {
            draw();
            dirty = false;
        }
        renderPresent();
        sharedFpsLimiter.throttle();
    }

    windowDestroy(win);
    if (oldCursorHidden) mouseHideCursor();
    return accepted;
}

} // namespace fallout

#endif
