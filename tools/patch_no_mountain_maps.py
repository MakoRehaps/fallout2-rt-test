from pathlib import Path

p = Path('src/map.cc')
s = p.read_text(encoding='utf-8')
marker = '// COOP_HARD_BLOCK_MOUNTAIN_MAPS_V1'
if marker in s:
    print('Mountain map hard-block already applied')
    raise SystemExit(0)

old = '''int mapLoadById(int map)\n{\n    scriptSetFixedParam(gMapSid, map);\n'''
new = '''int mapLoadById(int map)\n{\n    // COOP_HARD_BLOCK_MOUNTAIN_MAPS_V1\n    // Final safety gate: every map-loading path comes through here, including\n    // physical-road travel, random encounters, scripts, encounter chains and\n    // temporary dungeons. Keep mountain terrain on the world map, but never\n    // load the cramped authored MOUNTN layouts.\n    UnifiedGameId activeGame = unifiedCampaignGetActiveGame();\n    int requestedMap = map;\n    map = unifiedWorldSystemSafeTemplateMap(activeGame, map);\n    if (map != requestedMap) {\n        debugPrint(\"[WILDERNESS MAP REMAP] game=%d blockedMountainMap=%d replacement=%d\\n\",\n            static_cast<int>(static_cast<uint32_t>(activeGame)),\n            requestedMap,\n            map);\n    }\n\n    scriptSetFixedParam(gMapSid, map);\n'''

if old not in s:
    raise SystemExit('Expected mapLoadById entry point not found')

s = s.replace(old, new, 1)
p.write_text(s, encoding='utf-8')
print('Applied hard block for authored mountain encounter maps')
