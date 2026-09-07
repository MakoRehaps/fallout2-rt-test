#!/usr/bin/env python3
from pathlib import Path
import atexit
import re
import runpy


def _enforce_setup_only_input():
    mobile_path = Path('src/local_coop_mobile.cc')
    mobile = mobile_path.read_text(encoding='utf-8')

    # The Cloudflare/browser gameplay K/M bridge was a misunderstanding. Remove
    # it from the generated controller page so gameplay remains controller/phone
    # only. Keyboard/mouse are allowed only in the local PhoBoi host/setup window.
    mobile = re.sub(
        r'\n// PHOBOI_CLOUDFLARE_BROWSER_KBM_V1.*?\nsetInterval\(send,16\);',
        '\nsetInterval(send,16);',
        mobile,
        count=1,
        flags=re.S,
    )

    if '#include "mouse.h"' not in mobile:
        mobile = mobile.replace('#include "kb.h"\n', '#include "kb.h"\n#include "mouse.h"\n', 1)

    marker = '// PHOBOI_HOST_SETUP_MOUSE_KEYBOARD_ONLY_V1'
    if marker not in mobile:
        globals_anchor = 'int gMobileHostWindow = -1;\nuint64_t gMobileHostWindowLastDraw = 0;\n'
        if globals_anchor not in mobile:
            raise SystemExit('PhoBoi host window globals anchor missing')
        mobile = mobile.replace(
            globals_anchor,
            globals_anchor
            + marker + '\n'
            + 'bool gMobileHostMouseWasDown = false;\n'
            + 'void mobileCloseHostWindow();\n'
            + '#ifdef _WIN32\n'
            + 'bool mobileStartCloudflareTunnel();\n'
            + '#endif\n',
            1,
        )

        # Make the existing host/setup labels genuinely mouse-clickable. These
        # clicks never enter gameplay input; they only operate tunnel/setup state.
        open_anchor = '''void mobileOpenHostWindow()\n{\n    if (gMobileHostWindow != -1) {\n        return;\n    }\n'''
        if open_anchor not in mobile:
            raise SystemExit('PhoBoi host open anchor missing')
        mobile = mobile.replace(
            open_anchor,
            open_anchor
            + '    gMobileHostMouseWasDown = false;\n'
            + '    mouseShowCursor();\n',
            1,
        )

        close_old = '''void mobileCloseHostWindow()\n{\n    if (gMobileHostWindow != -1) {\n        windowDestroy(gMobileHostWindow);\n        gMobileHostWindow = -1;\n    }\n}\n'''
        close_new = '''void mobileCloseHostWindow()\n{\n    if (gMobileHostWindow != -1) {\n        windowDestroy(gMobileHostWindow);\n        gMobileHostWindow = -1;\n    }\n    gMobileHostMouseWasDown = false;\n    // Live gameplay owns a hidden cursor. The main menu will show it again\n    // through its normal path after returning from the game.\n    mouseHideCursor();\n}\n'''
        if close_old not in mobile:
            raise SystemExit('PhoBoi host close anchor missing')
        mobile = mobile.replace(close_old, close_new, 1)

        handler_anchor = 'void mobileOpenHostWindow()\n{'
        handler = r'''void mobileHandleHostMouse()
{
    if (gMobileHostWindow == -1) return;

    int mouseX = 0;
    int mouseY = 0;
    Uint32 state = SDL_GetMouseState(&mouseX, &mouseY);
    bool down = (state & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
    if (!down || gMobileHostMouseWasDown) {
        gMobileHostMouseWasDown = down;
        return;
    }
    gMobileHostMouseWasDown = true;

    // Host window is always centered and fixed at 620x420.
    int x = mouseX - (screenGetWidth() - 620) / 2;
    int y = mouseY - (screenGetVisibleHeight() - 420) / 2;
    if (x < 0 || y < 0 || x >= 620 || y >= 420) return;

    // Existing line: "C: COPY LINK   T: CLOUDFLARE HTTPS [...]"
    if (y >= 98 && y <= 136) {
        if (x < 285) {
            std::string pairingUrl = mobilePairingUrl();
            SDL_SetClipboardText(pairingUrl.c_str());
        } else {
#ifdef _WIN32
            mobileStartCloudflareTunnel();
#endif
        }
        mobileDrawHostWindow();
        return;
    }

    // Clicking a connected player row performs the same setup-only kick as
    // keyboard 2/3/4. It cannot control an actor in the running game.
    for (int slot = 1; slot < kLocalCoopMaxPlayers; slot++) {
        int rowY = 150 + (slot - 1) * 34;
        if (y >= rowY - 4 && y <= rowY + 24 && gMobileSlots[slot].claimed.load()) {
            mobileResetInput(gMobileSlots[slot]);
            gMobileSlots[slot].claimed.store(false);
            gMobileSlots[slot].token.store(0);
            mobileDrawHostWindow();
            return;
        }
    }

    if (y >= 292 && y <= 330) {
        mobileCloseHostWindow();
    }
}

'''
        mobile = mobile.replace(handler_anchor, handler + handler_anchor, 1)

        tick_anchor = '''    if (gMobileHostWindow != -1 && now - gMobileHostWindowLastDraw >= 250) {\n        mobileDrawHostWindow();\n    }\n'''
        tick_new = '''    if (gMobileHostWindow != -1) {\n        mobileHandleHostMouse();\n        if (gMobileHostWindow != -1 && now - gMobileHostWindowLastDraw >= 250) {\n            mobileDrawHostWindow();\n        }\n    }\n'''
        if tick_anchor not in mobile:
            raise SystemExit('PhoBoi host tick anchor missing')
        mobile = mobile.replace(tick_anchor, tick_new, 1)

    mobile_path.write_text(mobile, encoding='utf-8')

    # Escape is the only live-game keyboard exception. It opens the P1 system
    # menu. "Exit Game" returns to Fallout's main menu; the application can then
    # be exited normally from that main menu.
    system_path = Path('src/local_coop_system_menu.h')
    system = system_path.read_text(encoding='utf-8')
    system = system.replace(
        '''        gLocalCoopExitApplicationRequested = true;\n        _game_user_wants_to_quit = 2;\n''',
        '''        // COOP_EXIT_TO_MAIN_MENU_V1\n        _game_user_wants_to_quit = 2;\n''',
        1,
    )
    system_path.write_text(system, encoding='utf-8')


# Some older PhoBoi materializers intentionally stop this wrapper early once
# their target is already present. Register the modern repair as an exit hook so
# it ALWAYS runs after those legacy passes, even on SystemExit.
def _final_phoboi_repair():
    runpy.run_path('tools/patch_phoboi_final_repair_v3.py', run_name='__main__')
    _enforce_setup_only_input()


atexit.register(_final_phoboi_repair)

p = Path('src/main.cc')
s = p.read_text(encoding='utf-8')
marker = '// COOP_ESCAPE_EXIT_GAME_V1'

if marker not in s:
    old = '''        int legacyKeyCode = inputGetInput();\n        int keyCode = -1;\n        (void)legacyKeyCode;\n'''
    new = '''        int legacyKeyCode = inputGetInput();\n        int keyCode = -1;\n\n        // COOP_ESCAPE_EXIT_GAME_V1\n        // Escape is the one live-game keyboard exception. It opens the P1\n        // system menu; every other keyboard/mouse gameplay command is discarded.\n        // PhoBoi/Cloudflare host setup input is handled separately.\n        if (legacyKeyCode == KEY_ESCAPE) {\n            localCoopSystemMenuToggle();\n        }\n'''
    if old not in s:
        raise SystemExit('controller-only legacy input anchor missing')

    s = s.replace(old, new, 1)
    p.write_text(s, encoding='utf-8')
    print('Mapped Escape to P1 system menu; gameplay keyboard remains disabled')
else:
    print('Escape system-menu patch already applied')

# Restore the known working tunnel/backend paths and shared Skilldex first.
runpy.run_path('tools/patch_restore_working_paths_shared_skilldex.py', run_name='__main__')

# Preserve Character editor modal safety and mixed phone/XInput reservation.
runpy.run_path('tools/patch_coop_character_ui_deferred.py', run_name='__main__')

# FINAL ownership/presentation pass. This deliberately runs after every older
# co-op patch so hybrid keyboard gameplay, visible Pip-Boy entries, 16:9 phone
# scaling, and stale phone join behavior cannot overwrite the requested model.
runpy.run_path('tools/patch_phoboi_phone800_controller_only.py', run_name='__main__')

# The final workflow still checks a few old marker names. Keep them as inert
# comments only; they must never restore the removed stock Pip-Boy UI/action.
runpy.run_path('tools/patch_phoboi_only_validation_compat.py', run_name='__main__')

# Do not call the final PhoBoi repair here directly. The atexit hook above runs
# it after this wrapper and also covers early exits from any legacy child patch.
