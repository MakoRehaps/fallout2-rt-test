#!/usr/bin/env python3
from pathlib import Path

h = Path('src/local_coop.h')
r = Path('src/local_coop_runtime.h')
hs = h.read_text(encoding='utf-8')
rs = r.read_text(encoding='utf-8')

MARKER = '// COOP_EXPLICIT_SIMULATION_PAUSE_V1'
if MARKER not in hs:
    anchor = 'inline int gLocalCoopSkilldexInvokerSlot = -1;\n'
    if anchor not in hs:
        raise SystemExit('local_coop pause-state anchor not found')
    hs = hs.replace(anchor, anchor + '''\n''' + MARKER + '''\n// Window focus/Alt+Tab must never pause realtime co-op. Only explicit gameplay\n// flows such as joining a player or choosing a level-up perk freeze simulation.\ninline bool gLocalCoopLevelChoiceActive = false;\n\ninline bool localCoopJoinChoiceActive()\n{\n    for (const LocalCoopPlayer& player : gLocalCoopPlayers) {\n        if (player.joinMenuActive) {\n            return true;\n        }\n    }\n    return false;\n}\n\ninline bool localCoopSimulationPaused()\n{\n    return gLocalCoopLevelChoiceActive || localCoopJoinChoiceActive();\n}\n\ninline void localCoopSetLevelChoiceActive(bool active)\n{\n    gLocalCoopLevelChoiceActive = active;\n}\n''')

RUNTIME_MARKER = '// COOP_EXPLICIT_SIMULATION_PAUSE_RUNTIME_V1'
if RUNTIME_MARKER not in rs:
    old = '''    localCoopPollControllers();\n    localCoopProcessJoinMenus();\n    localCoopRestoreCharactersFromSave();\n    localCoopKeepReservedActorsWithParty();\n\n    // This should never become the player's normal state anymore.'''
    new = '''    localCoopPollControllers();\n    localCoopProcessJoinMenus();\n    localCoopRestoreCharactersFromSave();\n    localCoopKeepReservedActorsWithParty();\n\n    ''' + RUNTIME_MARKER + '''\n    // Keep polling controllers and join UI while paused, but freeze the world.\n    // Alt+Tab/focus loss is intentionally NOT part of this condition.\n    if (localCoopSimulationPaused()) {\n        gLocalCoopRuntimeInsideTick = false;\n        return;\n    }\n\n    // This should never become the player's normal state anymore.'''
    if old not in rs:
        raise SystemExit('runtime pause anchor not found')
    rs = rs.replace(old, new, 1)

h.write_text(hs, encoding='utf-8')
r.write_text(rs, encoding='utf-8')
print('Patched explicit simulation pause for join menus and level-choice UI')
