#!/usr/bin/env python3
from pathlib import Path
import atexit
import runpy

# Some older PhoBoi materializers intentionally stop this wrapper early once
# their target is already present. Register the modern join/control repair as
# an exit hook so it ALWAYS runs after those legacy passes, even on SystemExit.
# This is the authoritative last word for browser CONNECT and MSVC transport
# helper visibility.
def _final_phoboi_repair():
    runpy.run_path('tools/patch_phoboi_final_no_refresh_controls.py', run_name='__main__')

atexit.register(_final_phoboi_repair)

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
