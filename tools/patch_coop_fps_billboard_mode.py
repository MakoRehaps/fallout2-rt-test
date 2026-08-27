from pathlib import Path

marker = 'COOP_NATIVE_BILLBOARD_FPS_RUNTIME_V1'

runtime = Path('src/local_coop_runtime.h')
s = runtime.read_text(encoding='utf-8')
if marker not in s:
    inc = '#include "local_coop_focus.h"\n'
    if inc not in s:
        raise SystemExit('runtime include anchor missing')
    s = s.replace(inc, inc + '#include "local_coop_fps.h"\n', 1)

    old = '    localCoopUpdateSharedCamera();\n    localCoopPersonalUiTick();\n'
    if old not in s:
        raise SystemExit('runtime camera/personal-ui anchor missing')
    s = s.replace(old,
        '    // COOP_NATIVE_BILLBOARD_FPS_RUNTIME_V1\n'
        '    localCoopUpdateSharedCamera();\n'
        '    localCoopFpsTick();\n'
        '    // Personal HUDs draw after FPS so all four remain readable.\n'
        '    localCoopPersonalUiTick();\n', 1)
    runtime.write_text(s, encoding='utf-8')

menu = Path('src/local_coop_system_menu.h')
s = menu.read_text(encoding='utf-8')
if 'COOP_NATIVE_BILLBOARD_FPS_MENU_V1' not in s:
    inc = '#include "local_coop_accessibility.h"\n'
    if inc not in s:
        raise SystemExit('menu include anchor missing')
    s = s.replace(inc, inc + '#include "local_coop_fps.h"\n', 1)

    s = s.replace(
        '    Accessibility,\n    Save,',
        '    Accessibility,\n    CameraMode,\n    Save,', 1)
    s = s.replace(
        '        "ACCESSIBILITY HIGHLIGHTS",\n        "SAVE GAME",',
        '        "ACCESSIBILITY HIGHLIGHTS",\n        "CAMERA MODE",\n        "SAVE GAME",', 1)

    label_anchor = '    // COOP_ACCESSIBILITY_MENU_V1\n'
    if label_anchor not in s:
        raise SystemExit('menu label anchor missing')
    addition = (
        '    // COOP_NATIVE_BILLBOARD_FPS_MENU_V1\n'
        '    if (index == static_cast<int>(LocalCoopSystemMenuAction::CameraMode)) {\n'
        '        return localCoopFpsActive() ? "CAMERA: FIRST PERSON" : "CAMERA: ISOMETRIC";\n'
        '    }\n'
    )
    s = s.replace(label_anchor, addition + label_anchor, 1)

    action_anchor = (
        '    case LocalCoopSystemMenuAction::Accessibility:\n'
        '        localCoopAccessibilityToggle();\n'
        '        debugPrint("[COOP ACCESSIBILITY] highlights=%s\\n", localCoopAccessibilityStatusLabel());\n'
        '        break;\n'
    )
    if action_anchor not in s:
        raise SystemExit('menu action anchor missing')
    s = s.replace(action_anchor, action_anchor +
        '    case LocalCoopSystemMenuAction::CameraMode:\n'
        '        localCoopFpsToggle();\n'
        '        break;\n', 1)

    menu.write_text(s, encoding='utf-8')

print('wired native billboard FPS camera mode')
