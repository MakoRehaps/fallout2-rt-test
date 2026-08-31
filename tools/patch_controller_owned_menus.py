from pathlib import Path


def read(path):
    return Path(path).read_text(encoding='utf-8')


def write(path, text):
    Path(path).write_text(text, encoding='utf-8')


# -----------------------------------------------------------------------------
# Main menu: P1 controller owns navigation. Remove legacy keyboard/mouse menu
# activation and draw a clear controller selection marker over the stock art.
# -----------------------------------------------------------------------------
path = 'src/mainmenu.cc'
s = read(path)
marker = '// COOP_CONTROLLER_OWNED_MAIN_MENU_V1'
if marker not in s:
    s = s.replace('#include <ctype.h>\n', '#include <ctype.h>\n\n#include <SDL.h>\n', 1)

    anchor = 'static void main_menu_play_sound(const char* fileName);\n'
    insert = r'''static void main_menu_play_sound(const char* fileName);

// COOP_CONTROLLER_OWNED_MAIN_MENU_V1
// P1's first game controller owns every interactive main-menu action.  The
// legacy Fallout keyboard accelerator keys and mouse buttons are intentionally
// not used in the unified co-op shell.
static SDL_GameController* gCoopMainMenuController = nullptr;
static int gCoopMainMenuSelection = 1; // NEW GAME
static bool gCoopMainMenuUpWasDown = false;
static bool gCoopMainMenuDownWasDown = false;
static bool gCoopMainMenuConfirmWasDown = false;
static bool gCoopMainMenuCancelWasDown = false;

static void coopMainMenuAcquireController()
{
    if (gCoopMainMenuController != nullptr && SDL_GameControllerGetAttached(gCoopMainMenuController)) {
        return;
    }
    if (gCoopMainMenuController != nullptr) {
        SDL_GameControllerClose(gCoopMainMenuController);
        gCoopMainMenuController = nullptr;
    }
    int count = SDL_NumJoysticks();
    for (int i = 0; i < count; ++i) {
        if (SDL_IsGameController(i)) {
            gCoopMainMenuController = SDL_GameControllerOpen(i);
            if (gCoopMainMenuController != nullptr) break;
        }
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
    if (gMainMenuWindow == -1 || gMainMenuWindowHidden) return;

    // Redraw the original background/buttons by refreshing the window, then put
    // a controller-owned selector beside the current option.  This keeps all
    // original Fallout artwork while making the active co-op menu state obvious.
    constexpr int markerX = 13;
    constexpr int markerY = 23;
    constexpr int markerW = 15;
    constexpr int markerH = 250;
    windowFill(gMainMenuWindow, markerX, markerY, markerW, markerH, 0);
    char selector[] = ">";
    windowDrawText(gMainMenuWindow,
        selector,
        markerW,
        markerX,
        markerY + gCoopMainMenuSelection * 41,
        _colorTable[32747]);
    windowRefresh(gMainMenuWindow);
}
'''
    if anchor not in s:
        raise SystemExit('mainmenu prototype anchor missing')
    s = s.replace(anchor, insert, 1)

    s = s.replace('    windowShow(gMainMenuWindow);\n', '    windowShow(gMainMenuWindow);\n    coopMainMenuDrawSelection();\n', 1)

    start = '''int mainMenuWindowHandleEvents()\n{\n    _in_main_menu = true;\n\n    bool oldCursorIsHidden = cursorIsHidden();\n    if (oldCursorIsHidden) {\n        mouseShowCursor();\n    }\n\n    unsigned int tick = getTicks();\n\n    int rc = -1;\n    while (rc == -1) {\n        sharedFpsLimiter.mark();\n\n        int keyCode = inputGetInput();\n'''
    replacement = '''int mainMenuWindowHandleEvents()\n{\n    _in_main_menu = true;\n\n    // Controller-owned shell: never expose or use a mouse cursor here.\n    mouseHideCursor();\n    coopMainMenuAcquireController();\n    coopMainMenuDrawSelection();\n\n    unsigned int tick = getTicks();\n\n    int rc = -1;\n    while (rc == -1) {\n        sharedFpsLimiter.mark();\n\n        // Pump SDL/Fallout events, but discard legacy keyboard/mouse activation.\n        inputGetInput();\n        coopMainMenuAcquireController();\n\n        bool up = gCoopMainMenuController != nullptr\n            && (SDL_GameControllerGetButton(gCoopMainMenuController, SDL_CONTROLLER_BUTTON_DPAD_UP) != 0\n                || SDL_GameControllerGetAxis(gCoopMainMenuController, SDL_CONTROLLER_AXIS_LEFTY) < -16000);\n        bool down = gCoopMainMenuController != nullptr\n            && (SDL_GameControllerGetButton(gCoopMainMenuController, SDL_CONTROLLER_BUTTON_DPAD_DOWN) != 0\n                || SDL_GameControllerGetAxis(gCoopMainMenuController, SDL_CONTROLLER_AXIS_LEFTY) > 16000);\n        bool confirm = gCoopMainMenuController != nullptr\n            && SDL_GameControllerGetButton(gCoopMainMenuController, SDL_CONTROLLER_BUTTON_A) != 0;\n        bool cancel = gCoopMainMenuController != nullptr\n            && SDL_GameControllerGetButton(gCoopMainMenuController, SDL_CONTROLLER_BUTTON_B) != 0;\n\n        if (up && !gCoopMainMenuUpWasDown) {\n            gCoopMainMenuSelection = (gCoopMainMenuSelection + MAIN_MENU_BUTTON_COUNT - 1) % MAIN_MENU_BUTTON_COUNT;\n            main_menu_play_sound("nmselec0");\n            coopMainMenuDrawSelection();\n            tick = getTicks();\n        }\n        if (down && !gCoopMainMenuDownWasDown) {\n            gCoopMainMenuSelection = (gCoopMainMenuSelection + 1) % MAIN_MENU_BUTTON_COUNT;\n            main_menu_play_sound("nmselec0");\n            coopMainMenuDrawSelection();\n            tick = getTicks();\n        }\n        if (confirm && !gCoopMainMenuConfirmWasDown) {\n            main_menu_play_sound("nmselec1");\n            rc = _return_values[gCoopMainMenuSelection];\n        } else if (cancel && !gCoopMainMenuCancelWasDown) {\n            main_menu_play_sound("nmselec1");\n            rc = MAIN_MENU_EXIT;\n        }\n\n        gCoopMainMenuUpWasDown = up;\n        gCoopMainMenuDownWasDown = down;\n        gCoopMainMenuConfirmWasDown = confirm;\n        gCoopMainMenuCancelWasDown = cancel;\n\n        int keyCode = -1;\n'''
    if start not in s:
        raise SystemExit('main menu event-loop anchor missing')
    s = s.replace(start, replacement, 1)

    # Remove the entire old keyboard/mouse dispatch block by making it unreachable.
    old = '''        for (int buttonIndex = 0; buttonIndex < MAIN_MENU_BUTTON_COUNT; buttonIndex++) {\n            if (keyCode == gMainMenuButtonKeyBindings[buttonIndex] || keyCode == toupper(gMainMenuButtonKeyBindings[buttonIndex])) {'''
    s = s.replace(old, '''        // Legacy keyboard/mouse main-menu dispatch is disabled for unified co-op.\n        if (false) for (int buttonIndex = 0; buttonIndex < MAIN_MENU_BUTTON_COUNT; buttonIndex++) {\n            if (keyCode == gMainMenuButtonKeyBindings[buttonIndex] || keyCode == toupper(gMainMenuButtonKeyBindings[buttonIndex])) {''', 1)

    end_old = '''    if (oldCursorIsHidden) {\n        mouseHideCursor();\n    }\n\n    _in_main_menu = false;\n\n    return rc;\n}'''
    end_new = '''    mouseHideCursor();\n    coopMainMenuReleaseController();\n    gCoopMainMenuUpWasDown = false;\n    gCoopMainMenuDownWasDown = false;\n    gCoopMainMenuConfirmWasDown = false;\n    gCoopMainMenuCancelWasDown = false;\n\n    _in_main_menu = false;\n\n    return rc;\n}'''
    if end_old not in s:
        raise SystemExit('main menu event-loop tail anchor missing')
    s = s.replace(end_old, end_new, 1)
    write(path, s)


# -----------------------------------------------------------------------------
# In-game PhoBoi system menu: controller-only.  Remove keyboard/mouse navigation.
# -----------------------------------------------------------------------------
path = 'src/local_coop_system_menu.h'
s = read(path)
marker = '// COOP_CONTROLLER_ONLY_SYSTEM_MENU_V1'
if marker not in s:
    s = s.replace('// COOP_SYSTEM_MENU_V1\n', '// COOP_SYSTEM_MENU_V1\n// COOP_CONTROLLER_ONLY_SYSTEM_MENU_V1\n', 1)
    s = s.replace('inline bool gLocalCoopSystemMenuMouseWasDown = false;\n', '', 1)
    s = s.replace('    gLocalCoopSystemMenuMouseWasDown = false;\n', '', 1)
    s = s.replace('        "D-PAD/ARROWS OR MOUSE  |  A/ENTER SELECT  |  B/START/ESC CLOSE",\n',
                  '        "D-PAD / LEFT STICK  |  A SELECT  |  B / START CLOSE",\n', 1)

    keyboard = '''    const Uint8* keys = SDL_GetKeyboardState(nullptr);\n    if (keys != nullptr) {\n        up = up || keys[SDL_SCANCODE_UP] != 0 || keys[SDL_SCANCODE_W] != 0;\n        down = down || keys[SDL_SCANCODE_DOWN] != 0 || keys[SDL_SCANCODE_S] != 0;\n        confirm = confirm || keys[SDL_SCANCODE_RETURN] != 0 || keys[SDL_SCANCODE_SPACE] != 0;\n        cancel = cancel || keys[SDL_SCANCODE_ESCAPE] != 0;\n    }\n\n'''
    if keyboard not in s:
        raise SystemExit('system menu keyboard block missing')
    s = s.replace(keyboard, '''    // P1 gameplay/menu ownership is controller-only.\n    if (p1.controller != nullptr) {\n        int stickY = SDL_GameControllerGetAxis(p1.controller, SDL_CONTROLLER_AXIS_LEFTY);\n        up = up || stickY < -16000;\n        down = down || stickY > 16000;\n    }\n\n''', 1)

    mouse_block = '''    int mx = 0;\n    int my = 0;\n    mouseGetPositionInWindow(gLocalCoopSystemMenuWindow, &mx, &my);\n    bool mouseDown = (mouse_get_last_buttons() & MOUSE_STATE_LEFT_BUTTON_DOWN) != 0;\n    constexpr int firstY = 86;\n    constexpr int rowHeight = 34;\n    if (mx >= 24 && mx < 476 && my >= firstY && my < firstY + count * rowHeight) {\n        int hover = (my - firstY) / rowHeight;\n        if (hover >= 0 && hover < count && hover != gLocalCoopSystemMenuSelection) {\n            gLocalCoopSystemMenuSelection = hover;\n            redraw = true;\n        }\n        if (mouseDown && !gLocalCoopSystemMenuMouseWasDown) {\n            localCoopSystemMenuActivate(gLocalCoopSystemMenuSelection);\n            return;\n        }\n    }\n\n'''
    if mouse_block not in s:
        raise SystemExit('system menu mouse block missing')
    s = s.replace(mouse_block, '', 1)
    s = s.replace('    gLocalCoopSystemMenuMouseWasDown = mouseDown;\n', '', 1)
    write(path, s)


# -----------------------------------------------------------------------------
# Live gameplay: P1 no longer drives Fallout through legacy keyboard/mouse input.
# Controller, phone, co-op menu, and explicit co-op camera paths remain active.
# -----------------------------------------------------------------------------
path = 'src/main.cc'
s = read(path)
marker = '// COOP_P1_CONTROLLER_ONLY_GAMEPLAY_V1'
if marker not in s:
    old = '''        int keyCode = inputGetInput();\n\n        // COOP_FPS_KEYCODE_HARD_HOOK_V1\n        if (keyCode == KEY_F9) {\n            debugPrint("[COOP CAMERA] mainLoop KEY_F9 hard hook\\n");\n            localCoopFpsToggle();\n            // Prevent the physical-state path from toggling a second time this frame.\n            gLocalCoopFpsToggleWasDown = true;\n        }\n'''
    new = '''        // COOP_P1_CONTROLLER_ONLY_GAMEPLAY_V1\n        // Pump platform events so controller/phone input remains live, but do not\n        // feed normal keyboard/mouse commands into Fallout gameplay. P1 now owns\n        // the game through the co-op controller bindings and PhoBoi menus.\n        int legacyKeyCode = inputGetInput();\n        int keyCode = -1;\n        (void)legacyKeyCode;\n'''
    if old not in s:
        raise SystemExit('main gameplay key hook anchor missing')
    s = s.replace(old, new, 1)
    write(path, s)

print('Applied controller-owned menu shell and P1 controller-only gameplay')
