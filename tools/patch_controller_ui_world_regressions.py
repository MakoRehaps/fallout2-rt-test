#!/usr/bin/env python3
from pathlib import Path

MARKER = 'COOP_CONTROLLER_UI_WORLD_REGRESSIONS_V1'


def patch(path, transform):
    p = Path(path)
    s = p.read_text(encoding='utf-8')
    ns = transform(s)
    if ns != s:
        p.write_text(ns, encoding='utf-8')
        print(f'Patched {path}')
    else:
        print(f'No change needed for {path}')


# ---------------------------------------------------------------------------
# Shared global state: an explicit request to leave the running game and close
# the application.  This is intentionally separate from Fallout's ordinary
# _game_user_wants_to_quit values, which normally return to the main menu.
# ---------------------------------------------------------------------------
def patch_local_coop(s):
    if 'COOP_EXIT_APPLICATION_REQUEST_V1' in s:
        return s
    old = 'inline bool gLocalCoopSystemMenuActive = false;\n'
    if old not in s:
        raise SystemExit('local_coop system-menu state anchor missing')
    new = old + '''\n// COOP_EXIT_APPLICATION_REQUEST_V1\n// Set by the P1-only PhoBoi system menu. main.cc consumes it after the live\n// gameplay loop has unwound so shutdown still goes through Fallout's cleanup.\ninline bool gLocalCoopExitApplicationRequested = false;\n'''
    return s.replace(old, new, 1)


patch('src/local_coop.h', patch_local_coop)


# ---------------------------------------------------------------------------
# P1 global system menu: direct controller-native backends.  Do not synthesize
# I/P/S/C keypresses because controller-only mainLoop intentionally discards
# legacy keyboard input.  Inventory opens the existing SHARED BAG UI; Pip-Boy
# remains strictly P1-owned.
# ---------------------------------------------------------------------------
def patch_system_menu(s):
    if MARKER in s:
        return s

    old = '#include "local_coop.h"\n'
    new = '''#include "local_coop.h"\n#include "local_coop_personal_ui.h"\n#include "pipboy.h"\n#include "skilldex.h"\n#include "character_editor.h"\n'''
    if old not in s:
        raise SystemExit('system-menu include anchor missing')
    s = s.replace(old, new, 1)

    old = '''    Save,\n    Load,\n    Options,\n    Count,\n};'''
    new = '''    Save,\n    Load,\n    Options,\n    ExitGame,\n    Count,\n};'''
    if old not in s:
        raise SystemExit('system-menu enum anchor missing')
    s = s.replace(old, new, 1)

    old = '''        "SAVE GAME",\n        "LOAD GAME",\n        "OPTIONS",\n    };'''
    new = '''        "SAVE GAME",\n        "LOAD GAME",\n        "OPTIONS",\n        "EXIT GAME",\n    };'''
    if old not in s:
        raise SystemExit('system-menu label anchor missing')
    s = s.replace(old, new, 1)

    # Ten rows need a little more vertical room; 464 still fits the 480p base UI.
    s = s.replace('constexpr int height = 430;', 'constexpr int height = 464;')

    old = '''    case LocalCoopSystemMenuAction::Inventory:\n        enqueueInputEvent(KEY_LOWERCASE_I);\n        break;\n    case LocalCoopSystemMenuAction::PipBoy:\n        enqueueInputEvent(KEY_LOWERCASE_P);\n        break;\n    case LocalCoopSystemMenuAction::Skilldex:\n        gLocalCoopSkilldexInvokerSlot = 0;\n        enqueueInputEvent(KEY_LOWERCASE_S);\n        break;\n    case LocalCoopSystemMenuAction::Character:\n        enqueueInputEvent(KEY_LOWERCASE_C);\n        break;'''
    new = '''    case LocalCoopSystemMenuAction::Inventory:\n        // COOP_P1_SHARED_BAG_MENU_V1\n        // P1's global Inventory entry opens the same shared party bag used by\n        // all four personal HUDs; it never falls back to a separate stock bag.\n        localCoopPersonalUiOpenInventory(0);\n        break;\n    case LocalCoopSystemMenuAction::PipBoy:\n        // COOP_P1_DIRECT_PIPBOY_V1\n        // Pip-Boy is a P1-only global device. Call it directly because the\n        // controller-owned main loop intentionally discards keyboard letters.\n        gLocalCoopModalControllerSlot = 0;\n        phoboiOpen(PIPBOY_OPEN_INTENT_WORLD_MAP);\n        break;\n    case LocalCoopSystemMenuAction::Skilldex:\n        gLocalCoopSkilldexInvokerSlot = 0;\n        gLocalCoopModalControllerSlot = 0;\n        skilldexOpen();\n        break;\n    case LocalCoopSystemMenuAction::Character:\n        gLocalCoopModalControllerSlot = 0;\n        characterEditorShow(false);\n        break;'''
    if old not in s:
        raise SystemExit('system-menu legacy-key action block missing')
    s = s.replace(old, new, 1)

    old = '''    case LocalCoopSystemMenuAction::Options:\n        doPreferences(false);\n        break;\n    default:'''
    new = '''    case LocalCoopSystemMenuAction::Options:\n        doPreferences(false);\n        break;\n    case LocalCoopSystemMenuAction::ExitGame:\n        // COOP_CONTROLLER_EXIT_GAME_V1\n        // Unwind the game normally, then main.cc consumes the application-exit\n        // request and skips recreating the main menu.\n        gLocalCoopExitApplicationRequested = true;\n        _game_user_wants_to_quit = 2;\n        break;\n    default:'''
    if old not in s:
        raise SystemExit('system-menu options action anchor missing')
    s = s.replace(old, new, 1)

    marker_anchor = '// COOP_CONTROLLER_ONLY_SYSTEM_MENU_V1\n'
    if marker_anchor not in s:
        raise SystemExit('system-menu controller marker missing')
    s = s.replace(marker_anchor, marker_anchor + f'// {MARKER}\n', 1)
    return s


patch('src/local_coop_system_menu.h', patch_system_menu)


# ---------------------------------------------------------------------------
# P1 D-pad-left Pip-Boy shortcut: open the actual PhoBoi backend directly.
# P2-P4 never enter this branch. B inside the Pip-Boy remains handled by the
# dedicated modal-controller ticker.
# ---------------------------------------------------------------------------
def patch_runtime(s):
    if 'COOP_P1_DIRECT_PIPBOY_HOTKEY_V1' in s:
        return s

    old = '''        if (p1PipboyEdge && pipboyModalActive) {\n            gLocalCoopModalControllerSlot = 0;\n            enqueueInputEvent(KEY_ESCAPE);\n            debugPrint("[PHOBOI INPUT] slot=0 global-ui=pipboy action=close\\n");\n        } else if (canOwnGlobalUi && p1PipboyEdge) {\n            gLocalCoopModalControllerSlot = 0;\n            enqueueInputEvent(KEY_LOWERCASE_P);\n            modalActive = true;\n            debugPrint("[PHOBOI INPUT] slot=0 global-ui=pipboy action=open\\n");\n        } else if (canOpen && skilldexDown && !runtime.skilldexWasDown) {'''
    new = '''        // COOP_P1_DIRECT_PIPBOY_HOTKEY_V1\n        // Only slot 0 owns Pip-Boy.  Open the real backend directly instead of\n        // queueing P, which is intentionally discarded by controller-only P1.\n        if (canOwnGlobalUi && p1PipboyEdge) {\n            gLocalCoopModalControllerSlot = 0;\n            phoboiOpen(PIPBOY_OPEN_INTENT_WORLD_MAP);\n            modalActive = true;\n            debugPrint("[PHOBOI INPUT] slot=0 global-ui=pipboy action=open-direct\\n");\n        } else if (canOpen && skilldexDown && !runtime.skilldexWasDown) {'''
    if old not in s:
        raise SystemExit('runtime Pip-Boy edge block missing')
    s = s.replace(old, new, 1)
    return s


patch('src/local_coop_runtime.h', patch_runtime)


# ---------------------------------------------------------------------------
# Gameplay cursor: no mouse arrow/reticle in controller-owned live play.  The
# VPN/setup page is a separate UI and is not touched by this source patch.
# ---------------------------------------------------------------------------
def patch_main(s):
    if 'COOP_NO_GAMEPLAY_MOUSE_RETICLE_V1' not in s:
        old = '''static void mainLoop()\n{\n    bool cursorWasHidden = cursorIsHidden();\n    if (cursorWasHidden) {\n        mouseShowCursor();\n    }\n\n    _main_game_paused = 0;'''
        new = '''static void mainLoop()\n{\n    // COOP_NO_GAMEPLAY_MOUSE_RETICLE_V1\n    // P1 live gameplay is controller/phone-owned. Keep both the Fallout game\n    // cursor and the platform cursor hidden for the entire live-world loop.\n    gameMouseSetCursor(MOUSE_CURSOR_NONE);\n    mouseHideCursor();\n\n    _main_game_paused = 0;'''
        if old not in s:
            raise SystemExit('mainLoop cursor entry anchor missing')
        s = s.replace(old, new, 1)

        old = '''        localCoopUpdateSharedCamera();\n\n        // COOP_FPS_LATE_RENDER_HOOK_V1'''
        new = '''        localCoopUpdateSharedCamera();\n\n        // Stock modal/map code can restore a cursor while unwinding. Reassert\n        // controller-only presentation before every live gameplay frame.\n        gameMouseSetCursor(MOUSE_CURSOR_NONE);\n        mouseHideCursor();\n\n        // COOP_FPS_LATE_RENDER_HOOK_V1'''
        if old not in s:
            raise SystemExit('mainLoop late cursor anchor missing')
        s = s.replace(old, new, 1)

        old = '''    scriptsDisable();\n\n    if (cursorWasHidden) {\n        mouseHideCursor();\n    }\n}'''
        new = '''    scriptsDisable();\n    gameMouseSetCursor(MOUSE_CURSOR_NONE);\n    mouseHideCursor();\n}'''
        if old not in s:
            raise SystemExit('mainLoop cursor exit anchor missing')
        s = s.replace(old, new, 1)

        # These two setup paths currently show the cursor immediately after
        # selecting MOUSE_CURSOR_NONE. Keep it hidden instead.
        s = s.replace('''    gameMouseSetCursor(MOUSE_CURSOR_NONE);\n    mouseShowCursor();\n    mapLoadByName(mapFileName);''', '''    gameMouseSetCursor(MOUSE_CURSOR_NONE);\n    mouseHideCursor();\n    mapLoadByName(mapFileName);''', 1)
        s = s.replace('''    gameMouseSetCursor(MOUSE_CURSOR_NONE);\n    mouseShowCursor();\n\n    return 0;''', '''    gameMouseSetCursor(MOUSE_CURSOR_NONE);\n    mouseHideCursor();\n\n    return 0;''', 1)

    if 'COOP_EXIT_APPLICATION_CONSUMER_V1' not in s:
        # Consume the explicit Exit Game request after whichever gameplay branch
        # returned.  This runs inside the outer main-menu loop and therefore can
        # shut down cleanly without calling exit() from a modal window.
        old = '''            case MAIN_MENU_SELFRUN:\n                _main_selfrun_record();\n                break;\n            }\n        }\n    }'''
        new = '''            case MAIN_MENU_SELFRUN:\n                _main_selfrun_record();\n                break;\n            }\n\n            // COOP_EXIT_APPLICATION_CONSUMER_V1\n            if (gLocalCoopExitApplicationRequested) {\n                gLocalCoopExitApplicationRequested = false;\n                done = true;\n                mainMenuWindowHide(false);\n                mainMenuWindowFree();\n                backgroundSoundDelete();\n            }\n        }\n    }'''
        if old not in s:
            raise SystemExit('falloutMain switch tail anchor missing')
        s = s.replace(old, new, 1)

    return s


patch('src/main.cc', patch_main)


# ---------------------------------------------------------------------------
# F1 wilderness/ravine cleanup.  The previous safety rule only blocked 49/50,
# but live logs show the ordinary road pool selecting 56/58/62/63.  Until each
# F1 encounter layout is visually audited for four open sides, use the known
# open desert templates 0/1/2 only.  Normalize old saves/active chains as well,
# so an already-persisted ravine cannot keep returning.
# ---------------------------------------------------------------------------
def patch_world(s):
    if 'COOP_F1_NO_RAVINE_POOL_V1' in s:
        return s

    old = '''inline constexpr int kUnifiedWorldSystemFallout1OrdinaryMaps[] = {\n    56, 57, 58, 59, 61, 62, 63, 64,\n};'''
    new = '''// COOP_F1_NO_RAVINE_POOL_V1\n// The former 56-64 F1 road set produced the narrow ravine/corridor layouts seen\n// in live co-op logs. Restrict physical through-travel to known-open desert\n// templates until additional F1 layouts are individually certified four-side.\ninline constexpr int kUnifiedWorldSystemFallout1OrdinaryMaps[] = {\n    0, 1, 2,\n};'''
    if old not in s:
        raise SystemExit('F1 ordinary wilderness pool anchor missing')
    s = s.replace(old, new, 1)

    old = '''    if (game == UnifiedGameId::Fallout1) {\n        if (mapIdx == 49) return 0;\n        if (mapIdx == 50) return 1;\n    } else {'''
    new = '''    if (game == UnifiedGameId::Fallout1) {\n        if (mapIdx == 49) return 0;\n        if (mapIdx == 50) return 1;\n        // COOP_F1_RAVINE_HARD_REMAP_V1\n        // Old saves and already-built encounter chains can still contain the\n        // former road-pool maps. Remap all of them before any load occurs.\n        if (mapIdx >= 56 && mapIdx <= 64) {\n            return (mapIdx - 56) % 3;\n        }\n    } else {'''
    if old not in s:
        raise SystemExit('F1 safe-template anchor missing')
    s = s.replace(old, new, 1)
    return s


patch('src/unified_world_system.h', patch_world)

print('Controller UI/world regression patch complete')
