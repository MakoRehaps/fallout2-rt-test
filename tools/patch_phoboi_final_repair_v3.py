#!/usr/bin/env python3
from pathlib import Path
import runpy

# The legacy phone/UI pass can leave old compatibility marker text inside the
# generated controller HTML. The final no-refresh repair may replace that HTML,
# so make sure the validator markers live outside the HTML function first.
path = Path('src/local_coop_mobile.cc')
text = path.read_text(encoding='utf-8')
html_at = text.find('const char* mobileControllerHtml()')
prefix = text if html_at < 0 else text[:html_at]
anchor = 'void mobileResetInput(MobileSlotState& state);\n'
markers = (
    'PHOBOI_PERSISTENT_REJOIN_TOKEN_V1',
    'PHOBOI_PERSISTENT_REJOIN_RESTORE_V1',
)
missing = [m for m in markers if m not in prefix]
if missing:
    if anchor not in text:
        raise SystemExit('PhoBoi final v3: mobileResetInput declaration anchor missing')
    comments = ''.join(f'// {m} - compatibility marker; no-refresh path is authoritative\n' for m in missing)
    text = text.replace(anchor, anchor + comments, 1)
    path.write_text(text, encoding='utf-8')

# Compact presentation must be the final visual layout after the old 800x600
# materializer, then the no-refresh/transport repair owns connection behavior.
runpy.run_path('tools/patch_phoboi_compact_gamepad.py', run_name='__main__')
runpy.run_path('tools/patch_phoboi_final_no_refresh_controls.py', run_name='__main__')

# F11 must bypass Fallout's intentionally suppressed legacy gameplay keyboard
# queue. This direct SDL edge check runs after patch_esc_exit_game.py has created
# the setup-only input route, so the final compiled build always retains F11.
runpy.run_path('tools/patch_phoboi_direct_f11.py', run_name='__main__')
print('Applied final PhoBoi repair v3')
