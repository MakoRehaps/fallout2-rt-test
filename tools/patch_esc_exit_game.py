#!/usr/bin/env python3
from pathlib import Path

p = Path('src/main.cc')
s = p.read_text(encoding='utf-8')
marker = '// COOP_ESCAPE_EXIT_GAME_V1'
if marker in s:
    print('Escape exit-game patch already applied')
    raise SystemExit(0)

old = '''        int legacyKeyCode = inputGetInput();\n        int keyCode = -1;\n        (void)legacyKeyCode;\n'''
new = '''        int legacyKeyCode = inputGetInput();\n        int keyCode = -1;\n\n        // COOP_ESCAPE_EXIT_GAME_V1\n        // Escape is the one live-game keyboard exception. It exits the running\n        // co-op game/application cleanly; every other keyboard/mouse gameplay\n        // command remains discarded. VPN/co-op setup keyboard input is separate.\n        if (legacyKeyCode == KEY_ESCAPE) {\n            gLocalCoopExitApplicationRequested = true;\n            _game_user_wants_to_quit = 2;\n        }\n'''
if old not in s:
    raise SystemExit('controller-only legacy input anchor missing')

s = s.replace(old, new, 1)
p.write_text(s, encoding='utf-8')
print('Mapped Escape to clean co-op application exit')
