from pathlib import Path

path = Path('src/local_coop_runtime.h')
text = path.read_text(encoding='utf-8')
marker = '// COOP_P1_HYBRID_INPUT_TRIGGER_FIX_V2'
if marker in text:
    print('P1 hybrid trigger repair already applied')
    raise SystemExit(0)

old = '''    bool controllerActivity = false;\n    if (p1.controller != nullptr) {\n        for (int axis = 0; axis < SDL_CONTROLLER_AXIS_MAX; axis++) {\n            int value = SDL_GameControllerGetAxis(p1.controller, static_cast<SDL_GameControllerAxis>(axis));\n            if (std::abs(value) > 9000) { controllerActivity = true; break; }\n        }\n        if (!controllerActivity) {\n            for (int button = 0; button < SDL_CONTROLLER_BUTTON_MAX; button++) {\n                if (SDL_GameControllerGetButton(p1.controller, static_cast<SDL_GameControllerButton>(button))) {\n                    controllerActivity = true; break;\n                }\n            }\n        }\n    }'''
new = '''    // COOP_P1_HYBRID_INPUT_TRIGGER_FIX_V2\n    bool controllerActivity = false;\n    if (p1.controller != nullptr) {\n        const SDL_GameControllerAxis stickAxes[] = {\n            SDL_CONTROLLER_AXIS_LEFTX,\n            SDL_CONTROLLER_AXIS_LEFTY,\n            SDL_CONTROLLER_AXIS_RIGHTX,\n            SDL_CONTROLLER_AXIS_RIGHTY,\n        };\n        for (SDL_GameControllerAxis axis : stickAxes) {\n            int value = SDL_GameControllerGetAxis(p1.controller, axis);\n            if (std::abs(value) > 9000) { controllerActivity = true; break; }\n        }\n        if (!controllerActivity) {\n            int leftTrigger = SDL_GameControllerGetAxis(p1.controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT);\n            int rightTrigger = SDL_GameControllerGetAxis(p1.controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);\n            // Trigger rest can be either 0 or SDL_JOYSTICK_AXIS_MIN depending on\n            // backend. Only positive pull values count as controller activity.\n            controllerActivity = leftTrigger > 12000 || rightTrigger > 12000;\n        }\n        if (!controllerActivity) {\n            for (int button = 0; button < SDL_CONTROLLER_BUTTON_MAX; button++) {\n                if (SDL_GameControllerGetButton(p1.controller, static_cast<SDL_GameControllerButton>(button))) {\n                    controllerActivity = true; break;\n                }\n            }\n        }\n    }'''
if old not in text:
    raise SystemExit('old hybrid controller activity block not found')
text = text.replace(old, new, 1)
path.write_text(text, encoding='utf-8')
print('Repaired P1 hybrid trigger idle detection')
