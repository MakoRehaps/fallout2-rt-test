#!/usr/bin/env python3
from pathlib import Path
import runpy

p = Path('src/main.cc')
s = p.read_text(encoding='utf-8')
marker = '// COOP_ESCAPE_EXIT_GAME_V1'

if marker not in s:
    old = '''        int legacyKeyCode = inputGetInput();\n        int keyCode = -1;\n        (void)legacyKeyCode;\n'''
    new = '''        int legacyKeyCode = inputGetInput();\n        int keyCode = -1;\n\n        // COOP_ESCAPE_EXIT_GAME_V1\n        // Escape is the one live-game keyboard exception. It exits the running\n        // co-op game/application cleanly; every other keyboard/mouse gameplay\n        // command remains discarded. VPN/co-op setup keyboard input is separate.\n        if (legacyKeyCode == KEY_ESCAPE) {\n            gLocalCoopExitApplicationRequested = true;\n            _game_user_wants_to_quit = 2;\n        }\n'''
    if old not in s:
        raise SystemExit('controller-only legacy input anchor missing')

    s = s.replace(old, new, 1)
    p.write_text(s, encoding='utf-8')
    print('Mapped Escape to clean co-op application exit')
else:
    print('Escape exit-game patch already applied')

# This is deliberately the LAST gameplay/UI correction in the final workflow.
# The earlier regression pass changed previously-working Pip-Boy behavior and
# later PhoBoi patches layered transport experiments over the working Cloudflare
# Quick Tunnel. Restore those known-working paths here, then add the new shared
# party Skilldex without touching either one.
runpy.run_path('tools/patch_restore_working_paths_shared_skilldex.py', run_name='__main__')

# Preserve Character editor modal safety independently. It remains deferred
# until after the runtime ticker returns, while Pip-Boy uses its restored direct
# backend consumer and Skilldex uses the new non-stock shared party overlay.
runpy.run_path('tools/patch_coop_character_ui_deferred.py', run_name='__main__')
