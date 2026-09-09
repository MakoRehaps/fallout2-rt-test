from pathlib import Path


def read(path):
    return Path(path).read_text(encoding='utf-8')


def write(path, text):
    Path(path).write_text(text, encoding='utf-8')


# -----------------------------------------------------------------------------
# Main Fallout menu: controller owned. VPN/remote-coop setup is intentionally
# outside this file and keeps keyboard support.
# -----------------------------------------------------------------------------
path = 'src/mainmenu.cc'
s = read(path)
marker = '// COOP_CONTROLLER_OWNED_MAIN_MENU_V1'
if marker not in s:
    if '#include <SDL.h>\n' not in s:
        s = s.replace('#include <ctype.h>\n', '#include <ctype.h>\n\n#include <SDL.h>\n', 1)

    state_anchor = 'static bool gMainMenuWindowHidden;\n'
    state = r'''

// COOP_CONTROLLER_OWNED_MAIN_MENU_V1
// The first game controller owns the Fallout main menu. This deliberately does
// not affect the separate VPN/remote-coop setup UI, where keyboard entry remains
// useful for addresses, session codes, and connection setup.
static SDL_GameController* gCoopMainMenuController = nullptr;
static int gCoopMainMenuSelection = 1; // NEW GAME
static bool gCoopMainMenuUpWasDown = false;
static bool gCoopMainMenuDownWasDown = false;
static bool gCoopMainMenuConfirmWasDown = false;
static bool gCoopMainMenuCancelWasDown = false;

static void coopMainMenuAcquireController()
{
    if (gCoopMainMenuController != nullptr
        && SDL_GameControllerGetAttached(gCoopMainMenuController)) {
        return;
    }

    if (gCoopMainMenuController != nullptr) {
        SDL_GameControllerClose(gCoopMainMenuController);
        gCoopMainMenuController = nullptr;
    }

    int count = SDL_NumJoysticks();
    for (int i = 0; i < count; ++i) {
        if (!SDL_IsGameController(i)) continue;
        gCoopMainMenuController = SDL_GameControllerOpen(i);
        if (gCoopMainMenuController != nullptr) break;
    }
}

static void coopMainMenuReleaseController()
{
    if (gCoopMainMenuController != nullptr) {
        SDL_GameControllerClose(gCoopMainMenuController);
        gCoopMainMenuController = nullptr;
    }
}

static void coopMainMenuDrawSelection()
{
    if (gMainMenuWindow == -1) return;

    constexpr int markerX = 13;
    constexpr int markerY = 23;
    constexpr int markerW = 15;
    constexpr int markerH = 250;
    windowFill(gMainMenuWindow, markerX, markerY, markerW, markerH, 0);
    windowDrawText(gMainMenuWindow,
        ">",
        markerW,
        markerX,
        markerY + gCoopMainMenuSelection * 41,
        _colorTable[32747]);
    windowRefresh(gMainMenuWindow);
}
'''
    if state_anchor not in s:
        raise SystemExit('main menu state anchor missing')
    s = s.replace(state_anchor, state_anchor + state, 1)

    fn_start = s.find('int mainMenuWindowHandleEvents()\n{')
    fn_end = s.find('\n// NOTE: Inlined.', fn_start)
    if fn_start < 0 or fn_end < 0:
        raise SystemExit('main menu event function boundaries missing')

    controller_fn = r'''int mainMenuWindowHandleEvents()
{
    _in_main_menu = true;

    // No gameplay-style mouse/keyboard ownership here. We still pump the input
    // system so SDL/controller hotplug stays alive.
    mouseHideCursor();
    coopMainMenuAcquireController();

    unsigned int tick = getTicks();
    int rc = -1;

    while (rc == -1) {
        sharedFpsLimiter.mark();
        inputGetInput();
        coopMainMenuAcquireController();

        bool up = gCoopMainMenuController != nullptr
            && (SDL_GameControllerGetButton(gCoopMainMenuController, SDL_CONTROLLER_BUTTON_DPAD_UP) != 0
                || SDL_GameControllerGetAxis(gCoopMainMenuController, SDL_CONTROLLER_AXIS_LEFTY) < -16000);
        bool down = gCoopMainMenuController != nullptr
            && (SDL_GameControllerGetButton(gCoopMainMenuController, SDL_CONTROLLER_BUTTON_DPAD_DOWN) != 0
                || SDL_GameControllerGetAxis(gCoopMainMenuController, SDL_CONTROLLER_AXIS_LEFTY) > 16000);
        bool confirm = gCoopMainMenuController != nullptr
            && SDL_GameControllerGetButton(gCoopMainMenuController, SDL_CONTROLLER_BUTTON_A) != 0;
        bool cancel = gCoopMainMenuController != nullptr
            && SDL_GameControllerGetButton(gCoopMainMenuController, SDL_CONTROLLER_BUTTON_B) != 0;

        if (up && !gCoopMainMenuUpWasDown) {
            gCoopMainMenuSelection =
                (gCoopMainMenuSelection + MAIN_MENU_BUTTON_COUNT - 1) % MAIN_MENU_BUTTON_COUNT;
            main_menu_play_sound("nmselec0");
            tick = getTicks();
        }
        if (down && !gCoopMainMenuDownWasDown) {
            gCoopMainMenuSelection =
                (gCoopMainMenuSelection + 1) % MAIN_MENU_BUTTON_COUNT;
            main_menu_play_sound("nmselec0");
            tick = getTicks();
        }
        if (confirm && !gCoopMainMenuConfirmWasDown) {
            main_menu_play_sound("nmselec1");
            rc = _return_values[gCoopMainMenuSelection];
        } else if (cancel && !gCoopMainMenuCancelWasDown) {
            main_menu_play_sound("nmselec1");
            rc = MAIN_MENU_EXIT;
        }

        gCoopMainMenuUpWasDown = up;
        gCoopMainMenuDownWasDown = down;
        gCoopMainMenuConfirmWasDown = confirm;
        gCoopMainMenuCancelWasDown = cancel;

        if (_game_user_wants_to_quit == 3) {
            rc = MAIN_MENU_EXIT;
        } else if (_game_user_wants_to_quit == 2) {
            _game_user_wants_to_quit = 0;
        } else if (getTicksSince(tick) >= gMainMenuScreensaverDelay) {
            rc = MAIN_MENU_TIMEOUT;
        }

        coopMainMenuDrawSelection();
        renderPresent();
        sharedFpsLimiter.throttle();
    }

    coopMainMenuReleaseController();
    gCoopMainMenuUpWasDown = false;
    gCoopMainMenuDownWasDown = false;
    gCoopMainMenuConfirmWasDown = false;
    gCoopMainMenuCancelWasDown = false;
    mouseHideCursor();
    _in_main_menu = false;
    return rc;
}
'''
    s = s[:fn_start] + controller_fn + s[fn_end:]
    write(path, s)
else:
    print('Controller-owned main menu already applied')


# -----------------------------------------------------------------------------
# In-game PhoBoi system menu: controller only. Stock Fallout backends remain
# callable behind our shell, but keyboard/mouse navigation is removed here.
# FPS/camera switching is removed from regular co-op entirely.
# -----------------------------------------------------------------------------
path = 'src/local_coop_system_menu.h'
s = read(path)
marker = '// COOP_CONTROLLER_ONLY_SYSTEM_MENU_V1'
if marker not in s:
    s = s.replace(
        '// COOP_SYSTEM_MENU_V1\n',
        '// COOP_SYSTEM_MENU_V1\n// COOP_CONTROLLER_ONLY_SYSTEM_MENU_V1\n// COOP_NATIVE_BILLBOARD_FPS_MENU_V1 compatibility marker; FPS action removed.\n',
        1)

    s = s.replace('    CameraMode,\n', '', 1)
    s = s.replace('        "CAMERA MODE",\n', '', 1)
    s = s.replace('inline bool gLocalCoopSystemMenuMouseWasDown = false;\n', '', 1)
    s = s.replace('    gLocalCoopSystemMenuMouseWasDown = false;\n', '', 1)

    camera_label = '''    // COOP_NATIVE_BILLBOARD_FPS_MENU_V1\n    if (index == static_cast<int>(LocalCoopSystemMenuAction::CameraMode)) {\n        return localCoopFpsActive() ? "CAMERA: FIRST PERSON" : "CAMERA: ISOMETRIC";\n    }\n'''
    s = s.replace(camera_label, '', 1)

    camera_case = '''    case LocalCoopSystemMenuAction::CameraMode:\n        localCoopFpsToggle();\n        break;\n'''
    s = s.replace(camera_case, '', 1)

    s = s.replace(
        '        "D-PAD/ARROWS OR MOUSE  |  A/ENTER SELECT  |  B/START/ESC CLOSE",\n',
        '        "D-PAD / LEFT STICK  |  A SELECT  |  B / START CLOSE",\n',
        1)

    keyboard = '''    const Uint8* keys = SDL_GetKeyboardState(nullptr);\n    if (keys != nullptr) {\n        up = up || keys[SDL_SCANCODE_UP] != 0 || keys[SDL_SCANCODE_W] != 0;\n        down = down || keys[SDL_SCANCODE_DOWN] != 0 || keys[SDL_SCANCODE_S] != 0;\n        confirm = confirm || keys[SDL_SCANCODE_RETURN] != 0 || keys[SDL_SCANCODE_SPACE] != 0;\n        cancel = cancel || keys[SDL_SCANCODE_ESCAPE] != 0;\n    }\n\n'''
    if keyboard not in s:
        raise SystemExit('system menu keyboard block missing')
    s = s.replace(keyboard, '''    if (p1.controller != nullptr) {\n        int stickY = SDL_GameControllerGetAxis(p1.controller, SDL_CONTROLLER_AXIS_LEFTY);\n        up = up || stickY < -16000;\n        down = down || stickY > 16000;\n    }\n\n''', 1)

    mouse_start = s.find('    int mx = 0;\n', s.find('inline void localCoopSystemMenuTick()'))
    mouse_end_anchor = '    if ((cancel && !gLocalCoopSystemMenuCancelWasDown)'
    mouse_end = s.find(mouse_end_anchor, mouse_start)
    if mouse_start < 0 or mouse_end < 0:
        raise SystemExit('system menu mouse block boundaries missing')
    s = s[:mouse_start] + s[mouse_end:]
    s = s.replace('    gLocalCoopSystemMenuMouseWasDown = mouseDown;\n', '', 1)

    # Controller-only menu should not expose a cursor left over from a stock UI.
    s = s.replace(
        '    gLocalCoopSystemMenuActive = true;\n    gLocalCoopSystemMenuSelection = 0;\n',
        '    gLocalCoopSystemMenuActive = true;\n    gLocalCoopSystemMenuSelection = 0;\n    mouseHideCursor();\n',
        1)

    write(path, s)
else:
    print('Controller-only system menu already applied')


# -----------------------------------------------------------------------------
# Live gameplay: P1 controller/phone owns play. Input is still pumped for SDL,
# but the returned keyboard/mouse command is not sent to Fallout's game handler.
# This does NOT touch VPN/remote-coop setup input.
# -----------------------------------------------------------------------------
path = 'src/main.cc'
s = read(path)
marker = '// COOP_P1_CONTROLLER_ONLY_GAMEPLAY_V1'
if marker not in s:
    regular = '''        int keyCode = inputGetInput();\n\n        // COOP_REGULAR_ISOMETRIC_ONLY_V1'''
    replacement = '''        // COOP_P1_CONTROLLER_ONLY_GAMEPLAY_V1\n        // Pump platform events so controllers/phones/hotplug remain live, but\n        // discard legacy keyboard/mouse gameplay commands for P1.\n        int legacyKeyCode = inputGetInput();\n        int keyCode = -1;\n        (void)legacyKeyCode;\n\n        // COOP_REGULAR_ISOMETRIC_ONLY_V1'''
    if regular in s:
        s = s.replace(regular, replacement, 1)
    else:
        legacy = '''        int keyCode = inputGetInput();\n\n        // COOP_FPS_KEYCODE_HARD_HOOK_V1\n        if (keyCode == KEY_F9) {\n            debugPrint("[COOP CAMERA] mainLoop KEY_F9 hard hook\\n");\n            localCoopFpsToggle();\n            // Prevent the physical-state path from toggling a second time this frame.\n            gLocalCoopFpsToggleWasDown = true;\n        }\n'''
        legacy_replacement = '''        // COOP_P1_CONTROLLER_ONLY_GAMEPLAY_V1\n        int legacyKeyCode = inputGetInput();\n        int keyCode = -1;\n        (void)legacyKeyCode;\n\n        // COOP_REGULAR_ISOMETRIC_ONLY_V1\n        // Legacy F9/FPS entry is disabled in regular co-op.\n'''
        if legacy not in s:
            raise SystemExit('main gameplay input anchor missing')
        s = s.replace(legacy, legacy_replacement, 1)
    write(path, s)
else:
    print('P1 controller-only gameplay already applied')

print('Applied controller-owned Fallout menus and P1 controller-only gameplay; VPN setup keyboard preserved')
