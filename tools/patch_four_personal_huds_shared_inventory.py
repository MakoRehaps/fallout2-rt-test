from pathlib import Path

p = Path('src/local_coop_runtime.h')
s = p.read_text(encoding='utf-8')
marker = 'COOP_FOUR_PERSONAL_HUD_SHARED_BAG_RUNTIME_V1'
if marker in s:
    print('already patched')
    raise SystemExit(0)

anchor = '#include "local_coop_focus.h"\n'
if anchor not in s:
    raise SystemExit('include anchor missing')
s = s.replace(anchor, anchor + '#include "local_coop_personal_ui.h"\n', 1)

anchor = 'inline void localCoopProcessModalMenuInput()\n{\n'
if anchor not in s:
    raise SystemExit('modal anchor missing')
s = s.replace(anchor, anchor + '    // COOP_FOUR_PERSONAL_HUD_SHARED_BAG_RUNTIME_V1\n    localCoopPersonalUiTick();\n', 1)

old = '        bool inventoryDown = backDown;'
if old not in s:
    raise SystemExit('inventory binding anchor missing')
s = s.replace(old, '        // Personal co-op bag overlays consume Back per player; stock Inventory is global.\n        bool inventoryDown = false;', 1)

old = '    localCoopUpdateSharedCamera();\n'
if old not in s:
    raise SystemExit('camera anchor missing')
s = s.replace(old, '    localCoopUpdateSharedCamera();\n    localCoopPersonalUiTick();\n', 1)

p.write_text(s, encoding='utf-8')
print('wired four personal HUDs and shared bag overlays')
