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

# This workflow step runs after the controller UI regression patch. Correct its
# blocking modal calls here so PipBoy/Skilldex/Character are dispatched from the
# main loop only after the co-op ticker has released its re-entry guard.
runpy.run_path('tools/patch_coop_deferred_global_ui.py', run_name='__main__')
