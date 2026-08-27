from pathlib import Path

path = Path('src/local_coop_runtime.h')
text = path.read_text(encoding='utf-8')
marker = '// COOP_P1_HYBRID_INPUT_START_TOGGLE_V1'
if marker in text:
    print('P1 hybrid input/start toggle already applied')
    raise SystemExit(0)

if '#include "mouse.h"\n' not in text:
    anchor = '#include "local_coop_focus.h"\n'
    if anchor not in text:
        raise SystemExit('mouse include anchor not found')
    text = text.replace(anchor, anchor + '#include "mouse.h"\n', 1)

old_fields = '''    bool startWasDown = false;\n    bool skilldexWasDown = false;'''
new_fields = '''    bool startWasDown = false;\n    bool startToggleArmed = true;\n    Uint32 startReleaseStartedTick = 0;\n    bool skilldexWasDown = false;'''
if old_fields not in text:
    raise SystemExit('start field anchor not found; run P1 global UI toggle first')
text = text.replace(old_fields, new_fields, 1)

state_anchor = '''inline int gLocalCoopCameraTargetTile = -1;\n'''
state_new = '''inline int gLocalCoopCameraTargetTile = -1;\n\n// COOP_P1_HYBRID_INPUT_START_TOGGLE_V1\ninline bool gLocalCoopP1ControllerActive = false;\ninline int gLocalCoopP1LastMouseX = -1;\ninline int gLocalCoopP1LastMouseY = -1;\ninline int gLocalCoopP1LastMouseButtons = 0;\n'''
if state_anchor not in text:
    raise SystemExit('hybrid state anchor not found')
text = text.replace(state_anchor, state_new, 1)

# Requires tools/patch_p1_global_ui_toggle.py to have run first.
mode_anchor = '''    bool pipboyModalActive = (currentGameMode & (GameMode::kPipboy | GameMode::kAutomap)) != 0;\n    Uint32 now = SDL_GetTicks();'''
mode_new = '''    bool pipboyModalActive = (currentGameMode & (GameMode::kPipboy | GameMode::kAutomap)) != 0;\n    bool startMenuModalActive = (currentGameMode & (GameMode::kOptions\n        | GameMode::kPreferences\n        | GameMode::kSaveGame\n        | GameMode::kLoadGame\n        | GameMode::kHelp)) != 0;\n    Uint32 now = SDL_GetTicks();'''
if mode_anchor not in text:
    raise SystemExit('single-press modal state anchor not found')
text = text.replace(mode_anchor, mode_new, 1)

owner_release_anchor = '''            if (!inventoryDown) {\n                if (runtime.inventoryReleaseStartedTick == 0) runtime.inventoryReleaseStartedTick = now;\n                if (!runtime.inventoryToggleArmed\n                    && static_cast<Sint32>(now - runtime.inventoryReleaseStartedTick) >= 140) {\n                    runtime.inventoryToggleArmed = true;\n                }\n            } else {\n                runtime.inventoryReleaseStartedTick = 0;\n            }\n        }'''
owner_release_new = '''            if (!inventoryDown) {\n                if (runtime.inventoryReleaseStartedTick == 0) runtime.inventoryReleaseStartedTick = now;\n                if (!runtime.inventoryToggleArmed\n                    && static_cast<Sint32>(now - runtime.inventoryReleaseStartedTick) >= 140) {\n                    runtime.inventoryToggleArmed = true;\n                }\n            } else {\n                runtime.inventoryReleaseStartedTick = 0;\n            }\n\n            if (!startDown) {\n                if (runtime.startReleaseStartedTick == 0) runtime.startReleaseStartedTick = now;\n                if (!runtime.startToggleArmed\n                    && static_cast<Sint32>(now - runtime.startReleaseStartedTick) >= 140) {\n                    runtime.startToggleArmed = true;\n                }\n            } else {\n                runtime.startReleaseStartedTick = 0;\n            }\n        }'''
if owner_release_anchor not in text:
    raise SystemExit('P1 release latch anchor not found')
text = text.replace(owner_release_anchor, owner_release_new, 1)

old_start = '''        } else if (canOpen && startDown && !runtime.startWasDown) {\n            gLocalCoopModalControllerSlot = slot;\n            enqueueInputEvent(KEY_ESCAPE);\n            modalActive = true;\n            debugPrint("[COOP MENU] slot=%d source=controller button=start\\n", slot);\n        }'''
new_start = '''        } else if (slot == 0 && startDown && runtime.startToggleArmed && startMenuModalActive) {\n            runtime.startToggleArmed = false;\n            gLocalCoopModalControllerSlot = 0;\n            enqueueInputEvent(KEY_ESCAPE);\n            debugPrint("[COOP MENU] slot=0 source=controller button=start action=close\\n");\n        } else if (canOpen && slot == 0 && startDown && runtime.startToggleArmed) {\n            runtime.startToggleArmed = false;\n            gLocalCoopModalControllerSlot = 0;\n            enqueueInputEvent(KEY_ESCAPE);\n            modalActive = true;\n            debugPrint("[COOP MENU] slot=0 source=controller button=start action=open\\n");\n        }'''
if old_start not in text:
    raise SystemExit('start menu block anchor not found')
text = text.replace(old_start, new_start, 1)

camera_anchor = '''inline void localCoopUpdateSharedCamera()\n{'''
hybrid_fn = '''inline void localCoopUpdateP1InputSource()\n{\n    LocalCoopPlayer& p1 = gLocalCoopPlayers[0];\n\n    int mouseX = 0;\n    int mouseY = 0;\n    mouseGetPosition(&mouseX, &mouseY);\n    int mouseButtons = mouse_get_last_buttons();\n    bool mouseActivity = gLocalCoopP1LastMouseX != -1\n        && (mouseX != gLocalCoopP1LastMouseX\n            || mouseY != gLocalCoopP1LastMouseY\n            || mouseButtons != gLocalCoopP1LastMouseButtons);\n    gLocalCoopP1LastMouseX = mouseX;\n    gLocalCoopP1LastMouseY = mouseY;\n    gLocalCoopP1LastMouseButtons = mouseButtons;\n\n    bool keyboardActivity = false;\n    int keyboardCount = 0;\n    const Uint8* keyboard = SDL_GetKeyboardState(&keyboardCount);\n    if (keyboard != nullptr) {\n        for (int i = 0; i < keyboardCount; i++) {\n            if (keyboard[i]) { keyboardActivity = true; break; }\n        }\n    }\n\n    bool controllerActivity = false;\n    if (p1.controller != nullptr) {\n        const SDL_GameControllerAxis stickAxes[] = {\n            SDL_CONTROLLER_AXIS_LEFTX,\n            SDL_CONTROLLER_AXIS_LEFTY,\n            SDL_CONTROLLER_AXIS_RIGHTX,\n            SDL_CONTROLLER_AXIS_RIGHTY,\n        };\n        for (SDL_GameControllerAxis axis : stickAxes) {\n            int value = SDL_GameControllerGetAxis(p1.controller, axis);\n            if (std::abs(value) > 9000) { controllerActivity = true; break; }\n        }\n        if (!controllerActivity) {\n            int leftTrigger = SDL_GameControllerGetAxis(p1.controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT);\n            int rightTrigger = SDL_GameControllerGetAxis(p1.controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);\n            controllerActivity = leftTrigger > 12000 || rightTrigger > 12000;\n        }\n        if (!controllerActivity) {\n            for (int button = 0; button < SDL_CONTROLLER_BUTTON_MAX; button++) {\n                if (SDL_GameControllerGetButton(p1.controller, static_cast<SDL_GameControllerButton>(button))) {\n                    controllerActivity = true; break;\n                }\n            }\n        }\n    }\n\n    // Latest active device wins. Mouse/keyboard can immediately take the cursor\n    // back; controller activity hides it again. Neither device is disabled.\n    if (mouseActivity || keyboardActivity) {\n        gLocalCoopP1ControllerActive = false;\n        if (cursorIsHidden()) mouseShowCursor();\n    } else if (controllerActivity) {\n        gLocalCoopP1ControllerActive = true;\n        if (!cursorIsHidden()) mouseHideCursor();\n    }\n}\n\ninline void localCoopUpdateSharedCamera()\n{'''
if camera_anchor not in text:
    raise SystemExit('camera function anchor not found')
text = text.replace(camera_anchor, hybrid_fn, 1)

poll_anchor = '''    localCoopPollControllers();\n    localCoopProcessJoinMenus();'''
poll_new = '''    localCoopPollControllers();\n    localCoopUpdateP1InputSource();\n    localCoopProcessJoinMenus();'''
if poll_anchor not in text:
    raise SystemExit('runtime poll anchor not found')
text = text.replace(poll_anchor, poll_new, 1)

path.write_text(text, encoding='utf-8')
print('Applied P1 hybrid controller/KBM handoff and Start toggle')
