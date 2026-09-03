from pathlib import Path

menu = Path('src/local_coop_system_menu.h')
runtime = Path('src/local_coop_runtime.h')
menu_text = menu.read_text(encoding='utf-8')
runtime_text = runtime.read_text(encoding='utf-8')
marker = '// COOP_ACCESSIBILITY_MENU_V1'

if marker in menu_text and 'localCoopAccessibilityTick();' in runtime_text:
    print('Accessibility highlights already wired')
    raise SystemExit(0)

include_anchor = '#include "local_coop.h"\n'
if '#include "local_coop_accessibility.h"\n' not in menu_text:
    if include_anchor not in menu_text:
        raise SystemExit('accessibility include anchor not found')
    menu_text = menu_text.replace(include_anchor, include_anchor + '#include "local_coop_accessibility.h"\n', 1)

enum_old = '''    Character,\n    Save,\n    Load,\n    Options,\n    Count,'''
enum_new = '''    Character,\n    Accessibility,\n    Save,\n    Load,\n    Options,\n    Count,'''
if enum_old in menu_text:
    menu_text = menu_text.replace(enum_old, enum_new, 1)
elif '    Accessibility,\n' not in menu_text:
    raise SystemExit('accessibility enum anchor not found')

labels_old = '''        "CHARACTER / PERKS",\n        "SAVE GAME",\n        "LOAD GAME",\n        "OPTIONS",'''
labels_new = '''        "CHARACTER / PERKS",\n        "ACCESSIBILITY HIGHLIGHTS",\n        "SAVE GAME",\n        "LOAD GAME",\n        "OPTIONS",'''
if labels_old in menu_text:
    menu_text = menu_text.replace(labels_old, labels_new, 1)
elif '        "ACCESSIBILITY HIGHLIGHTS",\n' not in menu_text:
    raise SystemExit('accessibility label anchor not found')

return_old = '''    return index >= 0 && index < static_cast<int>(LocalCoopSystemMenuAction::Count)\n        ? labels[index]\n        : "";'''
return_new = '''    // COOP_ACCESSIBILITY_MENU_V1\n    if (index == static_cast<int>(LocalCoopSystemMenuAction::Accessibility)) {\n        return gLocalCoopAccessibilityHighlightsEnabled\n            ? "ACCESSIBILITY HIGHLIGHTS: ON"\n            : "ACCESSIBILITY HIGHLIGHTS: OFF";\n    }\n    return index >= 0 && index < static_cast<int>(LocalCoopSystemMenuAction::Count)\n        ? labels[index]\n        : "";'''
if return_old in menu_text:
    menu_text = menu_text.replace(return_old, return_new, 1)
elif marker not in menu_text:
    raise SystemExit('accessibility dynamic label anchor not found')

# Nine rows need a slightly taller shell.
menu_text = menu_text.replace('constexpr int height = 390;', 'constexpr int height = 430;')

switch_old = '''    case LocalCoopSystemMenuAction::Character:\n        enqueueInputEvent(KEY_LOWERCASE_C);\n        break;\n    case LocalCoopSystemMenuAction::Save:'''
switch_new = '''    case LocalCoopSystemMenuAction::Character:\n        enqueueInputEvent(KEY_LOWERCASE_C);\n        break;\n    case LocalCoopSystemMenuAction::Accessibility:\n        localCoopAccessibilityToggle();\n        debugPrint("[COOP ACCESSIBILITY] highlights=%s\\n", localCoopAccessibilityStatusLabel());\n        break;\n    case LocalCoopSystemMenuAction::Save:'''
if switch_old in menu_text:
    menu_text = menu_text.replace(switch_old, switch_new, 1)
elif 'case LocalCoopSystemMenuAction::Accessibility:' not in menu_text:
    raise SystemExit('accessibility activation anchor not found')

# Keep nearby highlights refreshed as actors/objects move.
tick_anchor = '''    localCoopSystemMenuTick();\n    localCoopRestoreCharactersFromSave();'''
tick_new = '''    localCoopSystemMenuTick();\n    localCoopAccessibilityTick();\n    localCoopRestoreCharactersFromSave();'''
if tick_anchor in runtime_text:
    runtime_text = runtime_text.replace(tick_anchor, tick_new, 1)
elif 'localCoopAccessibilityTick();' not in runtime_text:
    raise SystemExit('accessibility runtime tick anchor not found')

menu.write_text(menu_text, encoding='utf-8')
runtime.write_text(runtime_text, encoding='utf-8')
print('Wired accessibility highlights into PhoBoi menu and runtime')
