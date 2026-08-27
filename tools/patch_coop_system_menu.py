from pathlib import Path

coop = Path('src/local_coop.h')
runtime = Path('src/local_coop_runtime.h')
coop_text = coop.read_text(encoding='utf-8')
runtime_text = runtime.read_text(encoding='utf-8')
marker = '// COOP_SYSTEM_MENU_RUNTIME_V1'
if marker in coop_text and marker in runtime_text:
    print('Co-op system menu already wired')
    raise SystemExit(0)

state_anchor = '''inline int gLocalCoopSkilldexInvokerSlot = -1;\n'''
state_new = '''inline int gLocalCoopSkilldexInvokerSlot = -1;\n\n// COOP_SYSTEM_MENU_RUNTIME_V1\ninline bool gLocalCoopSystemMenuActive = false;\n'''
if marker not in coop_text:
    if state_anchor not in coop_text:
        raise SystemExit('system menu state anchor not found')
    coop_text = coop_text.replace(state_anchor, state_new, 1)

pause_old = '''inline bool localCoopSimulationPaused()\n{\n    return gLocalCoopLevelChoiceActive || localCoopJoinChoiceActive();\n}'''
pause_new = '''inline bool localCoopSimulationPaused()\n{\n    return gLocalCoopLevelChoiceActive\n        || localCoopJoinChoiceActive()\n        || gLocalCoopSystemMenuActive;\n}'''
if pause_old in coop_text:
    coop_text = coop_text.replace(pause_old, pause_new, 1)
elif '|| gLocalCoopSystemMenuActive' not in coop_text:
    raise SystemExit('simulation pause anchor not found')

if '#include "local_coop_system_menu.h"\n' not in runtime_text:
    anchor = '#include "local_coop_focus.h"\n'
    if anchor not in runtime_text:
        raise SystemExit('runtime system menu include anchor not found')
    runtime_text = runtime_text.replace(anchor, anchor + '#include "local_coop_system_menu.h"\n', 1)

# This patch is intentionally applied after hybrid/start-toggle. Replace the
# stock Escape open path with our menu shell. Closing is handled by the shell's
# own Start/B/Escape latch before the simulation-pause early return.
old_start = '''        } else if (slot == 0 && startDown && runtime.startToggleArmed && startMenuModalActive) {\n            runtime.startToggleArmed = false;\n            gLocalCoopModalControllerSlot = 0;\n            enqueueInputEvent(KEY_ESCAPE);\n            debugPrint("[COOP MENU] slot=0 source=controller button=start action=close\\n");\n        } else if (canOpen && slot == 0 && startDown && runtime.startToggleArmed) {\n            runtime.startToggleArmed = false;\n            gLocalCoopModalControllerSlot = 0;\n            enqueueInputEvent(KEY_ESCAPE);\n            modalActive = true;\n            debugPrint("[COOP MENU] slot=0 source=controller button=start action=open\\n");\n        }'''
new_start = '''        } else if (canOpen && slot == 0 && startDown && runtime.startToggleArmed) {\n            // COOP_SYSTEM_MENU_RUNTIME_V1\n            runtime.startToggleArmed = false;\n            gLocalCoopModalControllerSlot = 0;\n            localCoopSystemMenuOpen();\n            modalActive = true;\n            debugPrint("[COOP MENU] slot=0 source=controller button=start action=open-phoboi\\n");\n        }'''
if marker not in runtime_text:
    if old_start not in runtime_text:
        raise SystemExit('hybrid start-toggle block not found')
    runtime_text = runtime_text.replace(old_start, new_start, 1)

# Tick the custom menu before explicit simulation pause. This keeps UI/controller
# input live while the world itself is frozen.
old_tick = '''    localCoopPollControllers();\n    localCoopUpdateP1InputSource();\n    localCoopProcessJoinMenus();\n    localCoopRestoreCharactersFromSave();'''
new_tick = '''    localCoopPollControllers();\n    localCoopUpdateP1InputSource();\n    localCoopProcessJoinMenus();\n    localCoopSystemMenuTick();\n    localCoopRestoreCharactersFromSave();'''
if old_tick in runtime_text:
    runtime_text = runtime_text.replace(old_tick, new_tick, 1)
elif 'localCoopSystemMenuTick();' not in runtime_text:
    raise SystemExit('runtime menu tick anchor not found')

coop.write_text(coop_text, encoding='utf-8')
runtime.write_text(runtime_text, encoding='utf-8')
print('Wired PhoBoi co-op system menu')
