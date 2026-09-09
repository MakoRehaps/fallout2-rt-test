#!/usr/bin/env python3
from pathlib import Path

p = Path('src/main.cc')
s = p.read_text(encoding='utf-8')
marker = '// COOP_PHOBOI_DIRECT_F11_V1'

if marker in s:
    print('Direct PhoBoi F11 polling already applied')
    raise SystemExit(0)

if '#include <SDL.h>' not in s:
    include_anchor = '#include "main.h"\n\n'
    if include_anchor not in s:
        raise SystemExit('main include anchor missing')
    s = s.replace(include_anchor, '#include "main.h"\n\n#include <SDL.h>\n', 1)

old = '''        bool phoboiSetupKey = localCoopMobileHandleKey(legacyKeyCode);\n        if (!phoboiSetupKey && legacyKeyCode == KEY_ESCAPE) {\n            localCoopSystemMenuToggle();\n        }\n'''
new = '''        // COOP_PHOBOI_DIRECT_F11_V1\n        // Fallout's legacy keyboard queue is intentionally suppressed during\n        // controller-owned gameplay, and on some Windows builds that also drops\n        // F11 before the PhoBoi setup handler sees it. Poll SDL's physical key\n        // state directly, edge-trigger it, and feed only F11 into the host/setup\n        // handler. This does not restore keyboard gameplay.\n        static bool phoboiF11WasDown = false;\n        const Uint8* phoboiKeyboard = SDL_GetKeyboardState(nullptr);\n        bool phoboiF11Down = phoboiKeyboard != nullptr\n            && phoboiKeyboard[SDL_SCANCODE_F11] != 0;\n        bool phoboiDirectF11Handled = false;\n        if (phoboiF11Down && !phoboiF11WasDown) {\n            phoboiDirectF11Handled = localCoopMobileHandleKey(KEY_F11);\n        }\n        phoboiF11WasDown = phoboiF11Down;\n\n        // Keep the old setup-only translation for C/T/2/3/4/Escape while the\n        // host window is open, but do not let a simultaneously queued F11 toggle\n        // the window a second time.\n        bool phoboiSetupKey = phoboiDirectF11Handled\n            || (legacyKeyCode != KEY_F11 && localCoopMobileHandleKey(legacyKeyCode));\n        if (!phoboiSetupKey && legacyKeyCode == KEY_ESCAPE) {\n            localCoopSystemMenuToggle();\n        }\n'''

if old not in s:
    raise SystemExit('PhoBoi setup-key routing anchor missing; run patch_esc_exit_game.py first')

s = s.replace(old, new, 1)
p.write_text(s, encoding='utf-8')
print('Applied direct SDL F11 PhoBoi host toggle')
