from pathlib import Path


def patch(path, old, new, label):
    p = Path(path)
    s = p.read_text(encoding='utf-8')
    if new in s:
        print(label + ' already applied')
        return
    if old not in s:
        raise SystemExit(label + ' anchor missing')
    p.write_text(s.replace(old, new, 1), encoding='utf-8')
    print('Applied ' + label)

# Regular co-op is isometric-only for now. Keep legacy FPS source/markers so old
# validation and future experimentation remain possible, but remove every live
# gameplay entry point (F9 and the late L3/render tick).
patch(
    'src/main.cc',
    '''        // COOP_FPS_KEYCODE_HARD_HOOK_V1\n        if (keyCode == KEY_F9) {\n            debugPrint("[COOP CAMERA] mainLoop KEY_F9 hard hook\\n");\n            localCoopFpsToggle();\n            // Prevent the physical-state path from toggling a second time this frame.\n            gLocalCoopFpsToggleWasDown = true;\n        }\n''',
    '''        // COOP_REGULAR_ISOMETRIC_ONLY_V1\n        // FPS is parked while regular co-op is stabilized. F9 is intentionally\n        // ignored here; legacy FPS code remains compiled only for compatibility.\n        if (keyCode == KEY_F9) {\n            keyCode = -1;\n        }\n''',
    'regular isometric F9 disable')

patch(
    'src/main.cc',
    '''        // COOP_FPS_LATE_RENDER_HOOK_V1\n        // COOP_FPS_SINGLE_LATE_TICK_V1\n        // Run once, unconditionally: this processes L3/FPS input even while\n        // isometric, then draws FPS last so the stock world cannot cover it.\n        localCoopFpsTick();\n''',
    '''        // COOP_FPS_LATE_RENDER_HOOK_V1\n        // COOP_FPS_SINGLE_LATE_TICK_V1\n        // COOP_REGULAR_ISOMETRIC_ONLY_V1: deliberately no localCoopFpsTick().\n        // This disables L3/phone/FPS activation and rendering in regular co-op.\n''',
    'regular isometric late FPS disable')

# Pip-Boy: use a normal P1 D-pad-left edge. The previous 140 ms re-arm state was
# fragile and could leave the button permanently disarmed after modal transitions.
p = Path('src/local_coop_runtime.h')
s = p.read_text(encoding='utf-8')
marker = '// COOP_PIPBOY_EDGE_TOGGLE_V2'
if marker not in s:
    start = s.index('        // COOP_P1_GLOBAL_UI_OWNER_V1\n        // COOP_P1_GLOBAL_UI_TOGGLE_V1')
    end = s.index('        runtime.pipboyWasDown = pipboyDown;', start)
    replacement = '''        // COOP_P1_GLOBAL_UI_OWNER_V1\n        // COOP_P1_GLOBAL_UI_TOGGLE_V1\n        // COOP_PIPBOY_EDGE_TOGGLE_V2\n        // P1 owns the stock global Pip-Boy. Use the physical D-pad-left edge\n        // directly; no delayed re-arm timer survives across modal transitions.\n        bool canOwnGlobalUi = canOpen && slot == 0;\n        bool p1PipboyEdge = slot == 0 && pipboyDown && !runtime.pipboyWasDown;\n\n        if (p1PipboyEdge && pipboyModalActive) {\n            gLocalCoopModalControllerSlot = 0;\n            enqueueInputEvent(KEY_ESCAPE);\n            debugPrint("[PHOBOI INPUT] slot=0 global-ui=pipboy action=close\\n");\n        } else if (canOwnGlobalUi && p1PipboyEdge) {\n            gLocalCoopModalControllerSlot = 0;\n            enqueueInputEvent(KEY_LOWERCASE_P);\n            modalActive = true;\n            debugPrint("[PHOBOI INPUT] slot=0 global-ui=pipboy action=open\\n");\n        } else if (canOpen && skilldexDown && !runtime.skilldexWasDown) {\n            gLocalCoopModalControllerSlot = slot;\n            gLocalCoopSkilldexInvokerSlot = slot;\n            enqueueInputEvent(KEY_LOWERCASE_S);\n            modalActive = true;\n            debugPrint("[COOP SKILLDEX] slot=%d source=controller button=right-stick\\n", slot);\n        } else if (canOpen && slot == 0 && startDown && !runtime.startWasDown) {\n            gLocalCoopModalControllerSlot = 0;\n            localCoopSystemMenuOpen();\n            modalActive = true;\n            debugPrint("[COOP MENU] slot=0 source=controller button=start action=open-phoboi\\n");\n        }\n\n'''
    s = s[:start] + replacement + s[end:]
    p.write_text(s, encoding='utf-8')
    print('Applied Pip-Boy edge toggle')
else:
    print('Pip-Boy edge toggle already applied')

# Encounter chains must never advance to the same map that is currently loaded.
# Build-time generation already avoids adjacent duplicates, but old saves and
# remapped unsafe templates can still collapse two stages onto one map.
p = Path('src/unified_world_system.h')
s = p.read_text(encoding='utf-8')
marker = '// COOP_CHAIN_NO_REPEAT_V1'
if marker not in s:
    old = '''        if (nextDepth >= 0 && nextDepth < active.length) {\n            active.depth = static_cast<uint8_t>(nextDepth);\n            active.currentMapIdx = active.maps[nextDepth];\n'''
    new = '''        if (nextDepth >= 0 && nextDepth < active.length) {\n            // COOP_CHAIN_NO_REPEAT_V1\n            // Skip duplicate/remapped stages instead of reloading the same map.\n            int step = delta > 0 ? 1 : -1;\n            while (nextDepth >= 0 && nextDepth < active.length\n                && unifiedWorldSystemSafeTemplateMap(game, active.maps[nextDepth]) == currentMapIdx) {\n                nextDepth += step;\n            }\n            if (nextDepth < 0 || nextDepth >= active.length) {\n                active.valid = 0;\n            } else {\n                active.depth = static_cast<uint8_t>(nextDepth);\n                active.currentMapIdx = unifiedWorldSystemSafeTemplateMap(game, active.maps[nextDepth]);\n'''
    if old not in s:
        raise SystemExit('chain advance anchor missing')
    s = s.replace(old, new, 1)
    old_tail = '''            return true;\n        }\n    }\n\n    int nextX = travel.currentCellX[gameIndex];'''
    new_tail = '''                return true;\n            }\n        }\n    }\n\n    int nextX = travel.currentCellX[gameIndex];'''
    if old_tail not in s:
        raise SystemExit('chain advance tail anchor missing')
    s = s.replace(old_tail, new_tail, 1)
    p.write_text(s, encoding='utf-8')
    print('Applied encounter no-repeat advance')
else:
    print('Encounter no-repeat already applied')

# Mountain authored layouts are already absent from the ordinary pool and the
# map-load safety gate remaps direct/scripted requests. This marker makes the
# cleanup explicit and lets CI verify the intended regular-coop rule.
p = Path('src/unified_world_system.h')
s = p.read_text(encoding='utf-8')
if '// COOP_REGULAR_NO_MOUNTAIN_ENCOUNTERS_V1' not in s:
    anchor = '// COOP_FOUR_SIDE_WILDERNESS_POOL_V1\n'
    if anchor not in s:
        raise SystemExit('ordinary wilderness pool anchor missing')
    s = s.replace(anchor, '// COOP_REGULAR_NO_MOUNTAIN_ENCOUNTERS_V1\n' + anchor, 1)
    p.write_text(s, encoding='utf-8')
    print('Marked mountain encounters excluded')
