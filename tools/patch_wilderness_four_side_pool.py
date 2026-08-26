from pathlib import Path

p = Path('src/unified_world_system.h')
s = p.read_text(encoding='utf-8')
marker = '// COOP_FOUR_SIDE_WILDERNESS_POOL_V1'
if marker in s:
    print('four-side wilderness pool already applied')
    raise SystemExit(0)

old = '''inline constexpr int kUnifiedWorldSystemFallout1OrdinaryMaps[] = {\n    // Mountain templates 49/50 reuse desert templates. Keep duplicate\n    // desert entries so mountain weighting remains represented.\n    0, 1, 2, 19, 20, 21, 0, 1,\n    56, 57, 58, 59, 61, 62, 63, 64,\n};\n\ninline constexpr int kUnifiedWorldSystemFallout2OrdinaryMaps[] = {\n    0, 1, 2,\n    // Mountain templates are replaced with open desert templates. Keep the\n    // duplicate entries so terrain weighting stays stable without loading the\n    // narrow authored mountain corridors.\n    68, 69, 70, 71, 72, 73, 81, 82, 76, 77,\n    80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91,\n    94, 94,\n    110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120,\n    113, 114, 115, 116, 125,\n    141, 142, 143, 144, 145, 146,\n};\n'''
new = '''// COOP_FOUR_SIDE_WILDERNESS_POOL_V1\n// Physical world travel only chooses generic wilderness/encounter layouts here.\n// Authored destinations (towns, caves, vaults, quest maps, forts/holds) are not\n// members of this pool and keep their intentional entrance/exit layout. Coast\n// locations are also handled outside this ordinary four-side wilderness pool.\n//\n// Keep this list conservative: a generic road/battle tile must be able to serve\n// as a through-map when the party enters from any world direction. Low-number\n// authored-location IDs and the later dungeon/special ranges are intentionally\n// excluded instead of being treated as interchangeable wilderness.\ninline constexpr int kUnifiedWorldSystemFallout1OrdinaryMaps[] = {\n    56, 57, 58, 59, 61, 62, 63, 64,\n};\n\ninline constexpr int kUnifiedWorldSystemFallout2OrdinaryMaps[] = {\n    // Core open random-encounter wilderness set. Mountain corridor layouts\n    // 74/75 and 95 remain excluded; the no-mountain safety gate still protects\n    // direct/scripted loads separately.\n    68, 69, 70, 71, 72, 73,\n    76, 77,\n    80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91,\n    94,\n};\n'''
if old not in s:
    raise SystemExit('ordinary wilderness pool block not found')

s = s.replace(old, new, 1)
p.write_text(s, encoding='utf-8')
print('Restricted procedural world travel to the conservative four-side wilderness pool')
