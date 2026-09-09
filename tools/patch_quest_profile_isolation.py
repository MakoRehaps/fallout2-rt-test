from pathlib import Path

MARKER = '// UNIFIED_QUEST_PROFILE_ISOLATION_V1'

p = Path('src/scripts.cc')
s = p.read_text(encoding='utf-8')

if MARKER in s:
    print('Quest profile isolation already patched')
    raise SystemExit(0)

include_anchor = '#include "worldmap.h"\n'
if '#include "unified_campaign.h"\n' not in s:
    if include_anchor not in s:
        raise SystemExit('scripts.cc include anchor not found')
    s = s.replace(include_anchor, include_anchor + '#include "unified_campaign.h"\n', 1)

anchor = '''int _scriptsCheckGameEvents(int* moviePtr, int window)\n{\n    int movie = -1;\n'''
replacement = '''int _scriptsCheckGameEvents(int* moviePtr, int window)\n{\n    // UNIFIED_QUEST_PROFILE_ISOLATION_V1\n    // This function is Fallout 2's Arroyo/timed-quest checker. Its GVAR enum\n    // values are not meaningful in Fallout 1 and can alias unrelated F1 quest\n    // globals. F1 owns its timed world events through the dedicated F1 event\n    // adapter, so never evaluate Fallout 2 quest state while F1 is active.\n    if (unifiedCampaignGetActiveGame() == UnifiedGameId::Fallout1) {\n        if (moviePtr != nullptr) {\n            *moviePtr = -1;\n        }\n        return 0;\n    }\n\n    int movie = -1;\n'''
if anchor not in s:
    raise SystemExit('scripts.cc _scriptsCheckGameEvents anchor not found')

s = s.replace(anchor, replacement, 1)
p.write_text(s, encoding='utf-8')
print('Patched Fallout 2 timed quest checker to stay out of Fallout 1')
