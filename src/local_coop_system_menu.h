#ifndef LOCAL_COOP_SYSTEM_MENU_H
#define LOCAL_COOP_SYSTEM_MENU_H

#include <SDL.h>

#include <algorithm>

#include "color.h"
#include "input.h"
#include "kb.h"
#include "loadsave.h"
#include "local_coop.h"
#include "local_coop_accessibility.h"
#include "local_coop_fps.h"
#include "mouse.h"
#include "preferences.h"
#include "svga.h"
#include "window_manager.h"

namespace fallout {

// COOP_SYSTEM_MENU_V1
// New P1-owned co-op menu shell. It replaces the stock Escape/start-menu entry
// point while preserving the game's proven inventory/Pip-Boy/character/save
// backends underneath, so quests and script-driven menus stay compatible.
enum class LocalCoopSystemMenuAction {
    Resume,
    Inventory,
    PipBoy,
    Skilldex,
    Character,
    Accessibility,
    CameraMode,
    Save,
    Load,
    Options,
    Count,
};

inline int gLocalCoopSystemMenuWindow = -1;
inline int gLocalCoopSystemMenuSelection = 0;
inline bool gLocalCoopSystemMenuUpWasDown = false;
inline bool gLocalCoopSystemMenuDownWasDown = false;
inline bool gLocalCoopSystemMenuConfirmWasDown = false;
inline bool gLocalCoopSystemMenuCancelWasDown = false;
inline bool gLocalCoopSystemMenuStartWasDown = false;
inline bool gLocalCoopSystemMenuMouseWasDown = false;

inline const char* localCoopSystemMenuLabel(int index)
{
    static const char* labels[] = {
        "RESUME",
        "INVENTORY",
        "PIP-BOY / MAP",
        "SKILLDEX",
        "CHARACTER / PERKS",
        "ACCESSIBILITY HIGHLIGHTS",
        "CAMERA MODE",
        "SAVE GAME",
        "LOAD GAME",
        "OPTIONS",
    };
    // COOP_NATIVE_BILLBOARD_FPS_MENU_V1
    if (index == static_cast<int>(LocalCoopSystemMenuAction::CameraMode)) {
        return localCoopFpsActive() ? "CAMERA: FIRST PERSON" : "CAMERA: ISOMETRIC";
    }
    // COOP_ACCESSIBILITY_MENU_V1
    if (index == static_cast<int>(LocalCoopSystemMenuAction::Accessibility)) {
        return gLocalCoopAccessibilityHighlightsEnabled
            ? "ACCESSIBILITY HIGHLIGHTS: ON"
            : "ACCESSIBILITY HIGHLIGHTS: OFF";
    }
    return index >= 0 && index < static_cast<int>(LocalCoopSystemMenuAction::Count)
        ? labels[index]
        : "";
}

inline void localCoopSystemMenuDraw()
{
    if (gLocalCoopSystemMenuWindow == -1) return;

    constexpr int width = 500;
    constexpr int height = 430;
    windowFill(gLocalCoopSystemMenuWindow, 0, 0, width, height, _colorTable[0]);
    windowDrawBorder(gLocalCoopSystemMenuWindow);
    windowDrawText(gLocalCoopSystemMenuWindow,
        "PHOBOI CO-OP SYSTEM",
        width - 48,
        24,
        20,
        _colorTable[992]);
    windowDrawText(gLocalCoopSystemMenuWindow,
        "PLAYER 1 GLOBAL MENU",
        width - 48,
        24,
        46,
        _colorTable[992]);

    constexpr int firstY = 86;
    constexpr int rowHeight = 34;
    for (int i = 0; i < static_cast<int>(LocalCoopSystemMenuAction::Count); ++i) {
        char line[128];
        snprintf(line, sizeof(line), "%s %s",
            i == gLocalCoopSystemMenuSelection ? ">" : " ",
            localCoopSystemMenuLabel(i));
        windowDrawText(gLocalCoopSystemMenuWindow,
            line,
            width - 64,
            34,
            firstY + i * rowHeight,
            _colorTable[i == gLocalCoopSystemMenuSelection ? 32747 : 992]);
    }

    windowDrawText(gLocalCoopSystemMenuWindow,
        "D-PAD/ARROWS OR MOUSE  |  A/ENTER SELECT  |  B/START/ESC CLOSE",
        width - 48,
        24,
        height - 28,
        _colorTable[992]);
    windowRefresh(gLocalCoopSystemMenuWindow);
}

inline void localCoopSystemMenuClose()
{
    if (gLocalCoopSystemMenuWindow != -1) {
        windowDestroy(gLocalCoopSystemMenuWindow);
        gLocalCoopSystemMenuWindow = -1;
    }
    gLocalCoopSystemMenuActive = false;
    gLocalCoopSystemMenuUpWasDown = false;
    gLocalCoopSystemMenuDownWasDown = false;
    gLocalCoopSystemMenuConfirmWasDown = false;
    gLocalCoopSystemMenuCancelWasDown = false;
    gLocalCoopSystemMenuStartWasDown = false;
    gLocalCoopSystemMenuMouseWasDown = false;
}

inline bool localCoopSystemMenuOpen()
{
    if (gLocalCoopSystemMenuActive) return true;

    constexpr int width = 500;
    constexpr int height = 430;
    gLocalCoopSystemMenuWindow = windowCreate(
        (screenGetWidth() - width) / 2,
        (screenGetVisibleHeight() - height) / 2,
        width,
        height,
        _colorTable[0],
        WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    if (gLocalCoopSystemMenuWindow == -1) return false;

    gLocalCoopSystemMenuActive = true;
    gLocalCoopSystemMenuSelection = 0;

    // COOP_SYSTEM_MENU_OPEN_LATCH_V1
    // Start is normally still held during the tick after it opens this window.
    // Seed the latch from the live device so that same physical press cannot be
    // interpreted again as an immediate close. A release is required first.
    LocalCoopPlayer& p1 = gLocalCoopPlayers[0];
    gLocalCoopSystemMenuStartWasDown = p1.controller != nullptr
        && SDL_GameControllerGetButton(p1.controller, SDL_CONTROLLER_BUTTON_START) != 0;

    localCoopSystemMenuDraw();
    return true;
}

inline void localCoopSystemMenuToggle()
{
    if (gLocalCoopSystemMenuActive) {
        localCoopSystemMenuClose();
    } else {
        localCoopSystemMenuOpen();
    }
}

inline void localCoopSystemMenuActivate(int index)
{
    LocalCoopSystemMenuAction action = static_cast<LocalCoopSystemMenuAction>(index);
    localCoopSystemMenuClose();
    gLocalCoopModalControllerSlot = 0;

    switch (action) {
    case LocalCoopSystemMenuAction::Resume:
        break;
    case LocalCoopSystemMenuAction::Inventory:
        enqueueInputEvent(KEY_LOWERCASE_I);
        break;
    case LocalCoopSystemMenuAction::PipBoy:
        enqueueInputEvent(KEY_LOWERCASE_P);
        break;
    case LocalCoopSystemMenuAction::Skilldex:
        gLocalCoopSkilldexInvokerSlot = 0;
        enqueueInputEvent(KEY_LOWERCASE_S);
        break;
    case LocalCoopSystemMenuAction::Character:
        enqueueInputEvent(KEY_LOWERCASE_C);
        break;
    case LocalCoopSystemMenuAction::Accessibility:
        localCoopAccessibilityToggle();
        debugPrint("[COOP ACCESSIBILITY] highlights=%s\n", localCoopAccessibilityStatusLabel());
        break;
    case LocalCoopSystemMenuAction::CameraMode:
        localCoopFpsToggle();
        break;
    case LocalCoopSystemMenuAction::Save:
        lsgSaveGame(LOAD_SAVE_MODE_NORMAL);
        break;
    case LocalCoopSystemMenuAction::Load:
        lsgLoadGame(LOAD_SAVE_MODE_NORMAL);
        break;
    case LocalCoopSystemMenuAction::Options:
        doPreferences(false);
        break;
    default:
        break;
    }
}

inline void localCoopSystemMenuTick()
{
    if (!gLocalCoopSystemMenuActive) return;

    LocalCoopPlayer& p1 = gLocalCoopPlayers[0];
    bool up = false;
    bool down = false;
    bool confirm = false;
    bool cancel = false;
    bool start = false;
    if (p1.controller != nullptr) {
        up = SDL_GameControllerGetButton(p1.controller, SDL_CONTROLLER_BUTTON_DPAD_UP) != 0;
        down = SDL_GameControllerGetButton(p1.controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN) != 0;
        confirm = SDL_GameControllerGetButton(p1.controller, SDL_CONTROLLER_BUTTON_A) != 0;
        cancel = SDL_GameControllerGetButton(p1.controller, SDL_CONTROLLER_BUTTON_B) != 0;
        start = SDL_GameControllerGetButton(p1.controller, SDL_CONTROLLER_BUTTON_START) != 0;
    }

    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    if (keys != nullptr) {
        up = up || keys[SDL_SCANCODE_UP] != 0 || keys[SDL_SCANCODE_W] != 0;
        down = down || keys[SDL_SCANCODE_DOWN] != 0 || keys[SDL_SCANCODE_S] != 0;
        confirm = confirm || keys[SDL_SCANCODE_RETURN] != 0 || keys[SDL_SCANCODE_SPACE] != 0;
        cancel = cancel || keys[SDL_SCANCODE_ESCAPE] != 0;
    }

    bool redraw = false;
    int count = static_cast<int>(LocalCoopSystemMenuAction::Count);
    if (up && !gLocalCoopSystemMenuUpWasDown) {
        gLocalCoopSystemMenuSelection = (gLocalCoopSystemMenuSelection + count - 1) % count;
        redraw = true;
    }
    if (down && !gLocalCoopSystemMenuDownWasDown) {
        gLocalCoopSystemMenuSelection = (gLocalCoopSystemMenuSelection + 1) % count;
        redraw = true;
    }

    int mx = 0;
    int my = 0;
    mouseGetPositionInWindow(gLocalCoopSystemMenuWindow, &mx, &my);
    bool mouseDown = (mouse_get_last_buttons() & MOUSE_STATE_LEFT_BUTTON_DOWN) != 0;
    constexpr int firstY = 86;
    constexpr int rowHeight = 34;
    if (mx >= 24 && mx < 476 && my >= firstY && my < firstY + count * rowHeight) {
        int hover = (my - firstY) / rowHeight;
        if (hover >= 0 && hover < count && hover != gLocalCoopSystemMenuSelection) {
            gLocalCoopSystemMenuSelection = hover;
            redraw = true;
        }
        if (mouseDown && !gLocalCoopSystemMenuMouseWasDown) {
            localCoopSystemMenuActivate(gLocalCoopSystemMenuSelection);
            return;
        }
    }

    if ((cancel && !gLocalCoopSystemMenuCancelWasDown)
        || (start && !gLocalCoopSystemMenuStartWasDown)) {
        localCoopSystemMenuClose();
        return;
    }
    if (confirm && !gLocalCoopSystemMenuConfirmWasDown) {
        localCoopSystemMenuActivate(gLocalCoopSystemMenuSelection);
        return;
    }

    if (redraw) localCoopSystemMenuDraw();
    gLocalCoopSystemMenuUpWasDown = up;
    gLocalCoopSystemMenuDownWasDown = down;
    gLocalCoopSystemMenuConfirmWasDown = confirm;
    gLocalCoopSystemMenuCancelWasDown = cancel;
    gLocalCoopSystemMenuStartWasDown = start;
    gLocalCoopSystemMenuMouseWasDown = mouseDown;
}

} // namespace fallout

#endif
