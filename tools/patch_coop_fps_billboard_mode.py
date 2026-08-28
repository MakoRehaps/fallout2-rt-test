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

# Hard-wire the co-op runtime into Fallout's real gameplay loop.  This is kept
# in the main FPS patch itself so every Windows installer build repairs the hook
# before compilation instead of relying on a previous materialization commit.
main = Path('src/main.cc')
ms = main.read_text(encoding='utf-8')
main_changed = False
runtime_include = '#include "local_coop_runtime.h"\n'
if runtime_include not in ms:
    include_anchor = '#include "local_coop_group_room.h"\n'
    if include_anchor not in ms:
        raise SystemExit('main runtime include anchor missing')
    ms = ms.replace(include_anchor, include_anchor + runtime_include, 1)
    main_changed = True

input_anchor = '        int keyCode = inputGetInput();\n'
if 'COOP_RUNTIME_MAINLOOP_HOOK_V1' not in ms:
    if input_anchor not in ms:
        raise SystemExit('main gameplay input anchor missing')
    ms = ms.replace(input_anchor, input_anchor +
        '\n'
        '        // COOP_RUNTIME_MAINLOOP_HOOK_V1\n'
        '        // Guaranteed per-frame entry point for co-op/FPS runtime.\n'
        '        localCoopRuntimeTick();\n', 1)
    main_changed = True

# F9 is also handled from the engine-translated keyCode here.  This bypasses
# any stale SDL physical-key state while still leaving localCoopFpsTick as the
# controller/phone and modal-loop path.
if 'COOP_FPS_KEYCODE_HARD_HOOK_V1' not in ms:
    if input_anchor not in ms:
        raise SystemExit('main F9 keyCode anchor missing')
    ms = ms.replace(input_anchor, input_anchor +
        '\n'
        '        // COOP_FPS_KEYCODE_HARD_HOOK_V1\n'
        '        if (keyCode == KEY_F9) {\n'
        '            debugPrint("[COOP CAMERA] mainLoop KEY_F9 hard hook\\n");\n'
        '            localCoopFpsToggle();\n'
        '            // Prevent the physical-state path from toggling a second time this frame.\n'
        '            gLocalCoopFpsToggleWasDown = true;\n'
        '        }\n', 1)
    main_changed = True

if main_changed:
    main.write_text(ms, encoding='utf-8')

# Force SDL to refresh controller state immediately before the L3 check.  Some
# Fallout input paths do not leave SDL's cached GameController button state fresh.
fps = Path('src/local_coop_fps.h')
fs = fps.read_text(encoding='utf-8')
if 'COOP_FPS_CONTROLLER_UPDATE_HARDEN_V1' not in fs:
    controller_anchor = '    bool controllerToggleDown = false;\n'
    if controller_anchor not in fs:
        raise SystemExit('FPS controller scan anchor missing')
    fs = fs.replace(controller_anchor,
        '    // COOP_FPS_CONTROLLER_UPDATE_HARDEN_V1\n'
        '    SDL_GameControllerUpdate();\n'
        + controller_anchor, 1)
    fps.write_text(fs, encoding='utf-8')

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

print('wired native billboard FPS camera mode with hard gameplay hook')

# The raycaster is intentionally a second-stage patch: the base FPS patch above
# remains idempotent, then this adds collision columns, z-occluded billboards and
# prepares the exact Freedoom 0.13.0 WAD pack for the installer.
raycast_patch = Path('tools/patch_fps_raycast_freedoom.py')
if not raycast_patch.exists():
    raise SystemExit('raycast/Freedoom patch helper missing')
exec(compile(raycast_patch.read_text(encoding='utf-8'), str(raycast_patch), 'exec'))
