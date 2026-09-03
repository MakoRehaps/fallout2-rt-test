from pathlib import Path

# Kept as a standalone idempotent patch so every installer rebuild can enforce
# that gameplay-only co-op presentation never owns the stock Fallout main menu.
path = Path('src/local_coop_runtime.h')
text = path.read_text(encoding='utf-8')
marker = 'COOP_MAIN_MENU_RUNTIME_GATE_V1'
if marker in text:
    print('main-menu co-op runtime gate already applied')
    raise SystemExit(0)

include_anchor = '#include "local_coop_system_menu.h"\n'
if include_anchor not in text:
    raise SystemExit('runtime include anchor missing')
text = text.replace(include_anchor, include_anchor + '#include "mainmenu.h"\n', 1)

anchor = '''    gLocalCoopRuntimeInsideTick = true;\n\n    if (!gLocalCoopInitialized) {\n        localCoopInit();\n    }\n'''
replacement = '''    gLocalCoopRuntimeInsideTick = true;\n\n    // COOP_MAIN_MENU_RUNTIME_GATE_V1\n    // The stock Fallout main menu must own its window and input completely.\n    // Do not initialize players, create personal HUDs, hide the stock interface,\n    // process gameplay controller binds, or advance the living world here.\n    // If gameplay returned to the menu, tear down any presentation windows first.\n    if (_main_menu_is_enabled()) {\n        localCoopFpsDestroyWindow();\n        localCoopPersonalUiShutdown();\n        localCoopDestroyHud();\n        if (cursorIsHidden()) mouseShowCursor();\n        gLocalCoopRuntimeInsideTick = false;\n        return;\n    }\n\n    if (!gLocalCoopInitialized) {\n        localCoopInit();\n    }\n'''
if anchor not in text:
    raise SystemExit('runtime tick initialization anchor missing')
text = text.replace(anchor, replacement, 1)

path.write_text(text, encoding='utf-8')
print('gated co-op runtime and HUDs off the stock main menu')
