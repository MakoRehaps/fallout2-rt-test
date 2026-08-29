#ifndef LOCAL_COOP_GROUP_ROOM_H
#define LOCAL_COOP_GROUP_ROOM_H

#include <SDL.h>

#include <array>
#include <cstdio>
#include <cstring>

#include "color.h"
#include "input.h"
#include "local_coop.h"
#include "local_coop_mobile.h"
#include "mouse.h"
#include "svga.h"
#include "text_font.h"
#include "window_manager.h"

namespace fallout {

// COOP_TILELESS_GROUP_ROOM_V1
// COOP_TILELESS_GROUP_ROOM_V2
// Pure UI scene shown before the real Fallout opening. It deliberately creates
// no map, no tiles, no scripts, no critters and no world transitions.
//
// On first launch this room doubles as the co-op onboarding screen. The READY
// vote remains locked until the guide has been read. The guide is saved in an
// SDL per-user preferences directory, so each Windows user sees it once per
// install/profile without requiring writes beside the game executable.

inline bool localCoopTutorialAlreadySeen()
{
    char* prefPath = SDL_GetPrefPath("PhoBoi", "FalloutUnifiedCoop");
    if (prefPath == nullptr) return false;

    char marker[1024];
    std::snprintf(marker, sizeof(marker), "%scoop_onboarding_v2.seen", prefPath);
    SDL_free(prefPath);

    FILE* stream = std::fopen(marker, "rb");
    if (stream == nullptr) return false;
    std::fclose(stream);
    return true;
}

inline void localCoopMarkTutorialSeen()
{
    char* prefPath = SDL_GetPrefPath("PhoBoi", "FalloutUnifiedCoop");
    if (prefPath == nullptr) return;

    char marker[1024];
    std::snprintf(marker, sizeof(marker), "%scoop_onboarding_v2.seen", prefPath);
    SDL_free(prefPath);

    FILE* stream = std::fopen(marker, "wb");
    if (stream == nullptr) return;
    const char* text = "PhoBoi co-op onboarding complete\n";
    std::fwrite(text, 1, std::strlen(text), stream);
    std::fclose(stream);
}

inline bool localCoopRunTilelessGroupRoom()
{
    localCoopInit();

    std::array<bool, kLocalCoopMaxPlayers> joined {};
    std::array<bool, kLocalCoopMaxPlayers> ready {};
    std::array<bool, kLocalCoopMaxPlayers> startWasDown {};
    std::array<bool, kLocalCoopMaxPlayers> aWasDown {};
    std::array<bool, kLocalCoopMaxPlayers> leftWasDown {};
    std::array<bool, kLocalCoopMaxPlayers> rightWasDown {};

    joined[0] = true;
    // COOP_READY_ROOM_PREJOIN_TRANSFER_V1
    gLocalCoopPrejoinedSlots.fill(false);
    gLocalCoopPrejoinedSlots[0] = true;

    static const char* kTutorialPages[][6] = {
        {
            "WELCOME TO FALLOUT UNIFIED CO-OP",
            "This build supports up to four local players in the same live world.",
            "Player 1 owns global menus and campaign decisions.",
            "Other players keep their own character, HUD, camera and combat input.",
            "Phones can join through the PhoBoi controller page shown by the game.",
            ""
        },
        {
            "PARTY / SESSION",
            "Press START to join a player slot.",
            "After the guide, press START again to toggle READY.",
            "The game begins only when every joined player is READY.",
            "Disconnected player slots can return without replacing another player.",
            ""
        },
        {
            "COMBAT AND PERSONAL CONTROLS",
            "Each human player controls their own movement, aiming and attacks.",
            "Controller RT = primary attack, RB = secondary attack.",
            "X = reload, Y = swap active hand/weapon.",
            "D-pad Right = quick self medical; Right Stick Click = Skilldex.",
            ""
        },
        {
            "MENUS / INVENTORY",
            "Controller Back/Select opens Inventory.",
            "D-pad Left opens the Pip-Boy / personal device flow.",
            "START opens the system menu during normal gameplay.",
            "Player 1 remains the owner of shared/global Fallout interfaces.",
            ""
        },
        {
            "ISOMETRIC <-> FIRST PERSON",
            "The first-person mode uses the same live Fallout map and simulation.",
            "Walls are raycast and critters/objects are rendered as billboards.",
            "Keyboard: F9 toggles camera mode.",
            "Controller: L3 / Left Stick Click. Phone: tap FPS / ISO.",
            ""
        },
        {
            "READY TO PLAY",
            "The guide is now complete for this Windows user.",
            "You can still use the normal game menus and controller labels as reminders.",
            "Press A on a controller/phone or ENTER on keyboard to unlock READY voting.",
            "Then each joined player presses START to vote READY.",
            ""
        },
    };
    constexpr int kTutorialPageCount = static_cast<int>(sizeof(kTutorialPages) / sizeof(kTutorialPages[0]));

    bool tutorialComplete = localCoopTutorialAlreadySeen();
    int tutorialPage = 0;

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
        return true;
    }

    bool oldCursorHidden = cursorIsHidden();
    mouseShowCursor();

    auto draw = [&]() {
        windowFill(win, 0, 0, width, height, _colorTable[0]);
        windowDrawBorder(win);
        windowDrawText(win, "PHOBOI CO-OP GROUP ROOM", width - 48, 24, 20, _colorTable[992]);

        if (!tutorialComplete) {
            char pageLabel[80];
            std::snprintf(pageLabel, sizeof(pageLabel), "FIRST-LAUNCH GUIDE  %d / %d", tutorialPage + 1, kTutorialPageCount);
            windowDrawText(win, pageLabel, width - 48, 24, 48, _colorTable[32747]);
            windowDrawText(win, kTutorialPages[tutorialPage][0], width - 48, 24, 86, _colorTable[992]);

            for (int line = 1; line < 6; ++line) {
                if (kTutorialPages[tutorialPage][line][0] == '\0') continue;
                windowDrawText(win, kTutorialPages[tutorialPage][line], width - 72, 38,
                    122 + (line - 1) * 40, _colorTable[992]);
            }

            windowDrawText(win,
                "LEFT/RIGHT OR D-PAD = PAGE    A/ENTER = NEXT    START = JOIN",
                width - 48,
                24,
                height - 58,
                _colorTable[32747]);
            windowDrawText(win, "READY VOTING UNLOCKS AFTER THIS GUIDE", width - 48, 24, height - 32, _colorTable[992]);
        } else {
            windowDrawText(win, "FORM PARTY BEFORE THE VAULT INTRO", width - 48, 24, 50, _colorTable[992]);
            windowDrawText(win, "PRESS START TO JOIN. RELEASE. PRESS START AGAIN TO VOTE READY.", width - 48, 24, 80, _colorTable[32747]);

            for (int slot = 0; slot < kLocalCoopMaxPlayers; ++slot) {
                const LocalCoopPlayer& player = gLocalCoopPlayers[slot];
                char line[180];
                const char* state = ready[slot]
                    ? "READY"
                    : joined[slot]
                        ? "JOINED - PRESS START TO READY"
                        : player.connected
                            ? "PRESS START TO JOIN"
                            : "WAITING FOR CONTROLLER / PHONE";
                if (slot == 0 && player.controller == nullptr && joined[slot] && !ready[slot]) {
                    state = "JOINED - ENTER OR START TO READY";
                }
                std::snprintf(line, sizeof(line), "PLAYER %d   %s", slot + 1, state);
                windowDrawText(win, line, width - 72, 38, 126 + slot * 54,
                    ready[slot] ? _colorTable[32747] : _colorTable[992]);
            }

            windowDrawText(win, "ALL JOINED PLAYERS MUST VOTE READY", width - 48, 24, height - 54, _colorTable[992]);
            windowDrawText(win, "ESC = CANCEL", width - 48, 24, height - 30, _colorTable[992]);
        }

        windowRefresh(win);
    };

    draw();
    bool accepted = false;
    bool dirty = false;
    bool enterWasDown = false;

    while (_game_user_wants_to_quit == 0) {
        sharedFpsLimiter.mark();
        inputGetInput();
        // COOP_READY_ROOM_MOBILE_TICK_V1
        // Keep phone claims materialized as SDL virtual controllers while the
        // pre-game room is open; otherwise a phone can claim a slot on the web
        // page without the ready room ever seeing the controller.
        localCoopMobileTick();
        localCoopRefreshControllers();

        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        bool enterDown = keys != nullptr && keys[SDL_SCANCODE_RETURN] != 0;
        bool escapeDown = keys != nullptr && keys[SDL_SCANCODE_ESCAPE] != 0;
        bool keyLeft = keys != nullptr && keys[SDL_SCANCODE_LEFT] != 0;
        bool keyRight = keys != nullptr && keys[SDL_SCANCODE_RIGHT] != 0;
        static bool keyLeftWasDown = false;
        static bool keyRightWasDown = false;

        if (escapeDown) {
            break;
        }

        bool tutorialAdvance = enterDown && !enterWasDown;
        bool tutorialPrevious = keyLeft && !keyLeftWasDown;
        bool tutorialNext = keyRight && !keyRightWasDown;

        for (int slot = 0; slot < kLocalCoopMaxPlayers; ++slot) {
            LocalCoopPlayer& player = gLocalCoopPlayers[slot];
            bool hasController = player.controller != nullptr;
            bool startDown = hasController
                && SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_START) != 0;
            bool aDown = hasController
                && SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_A) != 0;
            bool leftDown = hasController
                && SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT) != 0;
            bool rightDown = hasController
                && SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) != 0;

            bool startEdge = startDown && !startWasDown[slot];
            bool aEdge = aDown && !aWasDown[slot];
            bool leftEdge = leftDown && !leftWasDown[slot];
            bool rightEdge = rightDown && !rightWasDown[slot];

            if (!tutorialComplete) {
                if (startEdge && !joined[slot]) {
                    joined[slot] = true;
                    ready[slot] = false;
                    gLocalCoopPrejoinedSlots[slot] = true;
                    dirty = true;
                    debugPrint("[COOP GROUP] slot=%d joined during guide\n", slot);
                }
                tutorialAdvance = tutorialAdvance || aEdge;
                tutorialPrevious = tutorialPrevious || leftEdge;
                tutorialNext = tutorialNext || rightEdge;
            } else if (startEdge || (slot == 0 && enterDown && !enterWasDown)) {
                if (!joined[slot]) {
                    joined[slot] = true;
                    ready[slot] = false;
                    gLocalCoopPrejoinedSlots[slot] = true;
                    dirty = true;
                    debugPrint("[COOP GROUP] slot=%d joined\n", slot);
                } else {
                    ready[slot] = !ready[slot];
                    dirty = true;
                    debugPrint("[COOP GROUP] slot=%d ready=%d\n", slot, ready[slot] ? 1 : 0);
                }
            }

            startWasDown[slot] = startDown;
            aWasDown[slot] = aDown;
            leftWasDown[slot] = leftDown;
            rightWasDown[slot] = rightDown;
        }

        if (!tutorialComplete) {
            if (tutorialPrevious && tutorialPage > 0) {
                --tutorialPage;
                dirty = true;
            }
            if (tutorialNext && tutorialPage < kTutorialPageCount - 1) {
                ++tutorialPage;
                dirty = true;
            }
            if (tutorialAdvance) {
                if (tutorialPage < kTutorialPageCount - 1) {
                    ++tutorialPage;
                } else {
                    tutorialComplete = true;
                    localCoopMarkTutorialSeen();
                    debugPrint("[COOP GROUP] first-launch guide complete; ready vote unlocked\n");
                }
                dirty = true;
            }
        }

        enterWasDown = enterDown;
        keyLeftWasDown = keyLeft;
        keyRightWasDown = keyRight;

        if (tutorialComplete) {
            int joinedCount = 0;
            int readyCount = 0;
            for (int slot = 0; slot < kLocalCoopMaxPlayers; ++slot) {
                if (joined[slot]) {
                    ++joinedCount;
                    if (ready[slot]) ++readyCount;
                }
            }

            if (joinedCount > 0 && readyCount == joinedCount) {
                for (int slot = 0; slot < kLocalCoopMaxPlayers; ++slot) {
                    gLocalCoopPrejoinedSlots[slot] = joined[slot];
                }
                debugPrint("[COOP PREJOIN] ready-room transfer p1=%d p2=%d p3=%d p4=%d\n",
                    joined[0] ? 1 : 0, joined[1] ? 1 : 0, joined[2] ? 1 : 0, joined[3] ? 1 : 0);
                accepted = true;
                break;
            }
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
