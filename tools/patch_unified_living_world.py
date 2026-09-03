from pathlib import Path

runtime = Path('src/local_coop_runtime.h')
worldmap = Path('src/worldmap.cc')

rs = runtime.read_text(encoding='utf-8')
if 'COOP_UNIFIED_LIVING_WORLD_RUNTIME_V1' not in rs:
    include_anchor = '#include "unified_campaign_transition.h"\n'
    if include_anchor not in rs:
        raise SystemExit('runtime include anchor missing')
    rs = rs.replace(include_anchor, include_anchor + '#include "unified_living_world.h"\n', 1)

    tick_anchor = '''    if (localCoopSimulationPaused()) {\n        gLocalCoopRuntimeInsideTick = false;\n        return;\n    }\n\n'''
    if tick_anchor not in rs:
        raise SystemExit('runtime pause anchor missing')
    rs = rs.replace(tick_anchor, tick_anchor + '''    // COOP_UNIFIED_LIVING_WORLD_RUNTIME_V1\n    // Advance the offline faction/economy/territory simulation only while the\n    // gameplay world itself is running. Explicit co-op modal pauses freeze it.\n    unifiedLivingRuntimeTick();\n\n''', 1)
    runtime.write_text(rs, encoding='utf-8')
else:
    print('runtime already patched')

ws = worldmap.read_text(encoding='utf-8')
if 'COOP_UNIFIED_LIVING_WORLD_ENCOUNTERS_V1' not in ws:
    include_anchor = '#include "unified_world_system.h"\n'
    if include_anchor not in ws:
        raise SystemExit('worldmap include anchor missing')
    ws = ws.replace(include_anchor, include_anchor + '#include "unified_living_world.h"\n', 1)

    objective_anchor = '    int objectiveRoll = randomBetween(0, 6);\n'
    if objective_anchor not in ws:
        raise SystemExit('self-play objective anchor missing')
    ws = ws.replace(objective_anchor, '''    // COOP_UNIFIED_LIVING_WORLD_ENCOUNTERS_V1\n    // The same hidden encounter game still decides the tactical setup, but the\n    // current cell's persistent event/war/economy state biases its objective.\n    int objectiveRoll = unifiedLivingEncounterObjectiveBias(randomBetween(0, 6));\n''', 1)

    lead_anchor = '    int lead = (sideA + progressA) - (sideB + progressB);\n'
    if lead_anchor not in ws:
        raise SystemExit('self-play lead anchor missing')
    ws = ws.replace(lead_anchor, lead_anchor + '''\n    // Feed the hidden setup result back into the persistent cell so repeated\n    // trouble raises conflict/danger and can create later revenge events.\n    unifiedLivingRecordEncounterSetup(lead);\n''', 1)
    worldmap.write_text(ws, encoding='utf-8')
else:
    print('worldmap already patched')

print('wired unified living world into runtime and encounters')
