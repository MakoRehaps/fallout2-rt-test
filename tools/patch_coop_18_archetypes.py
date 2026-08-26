from pathlib import Path

p = Path('src/local_coop.h')
s = p.read_text(encoding='utf-8')
marker = '// COOP_18_ARCHETYPES_V1'
if marker in s:
    print('18 archetypes already applied')
    raise SystemExit(0)

old = '''inline constexpr int kLocalCoopArchetypeCount = 4;
inline constexpr const char* kLocalCoopArchetypeNames[kLocalCoopArchetypeCount] = {
    "WASTELAND FIGHTER",
    "SCOUT",
    "MEDIC",
    "TECH SPECIALIST",
};

inline constexpr int kLocalCoopArchetypeStats[kLocalCoopArchetypeCount][PRIMARY_STAT_COUNT] = {
    { 7, 5, 7, 4, 5, 7, 5 },
    { 5, 8, 5, 4, 6, 8, 4 },
    { 4, 6, 5, 6, 8, 6, 5 },
    { 4, 6, 4, 5, 9, 6, 6 },
};
'''
new = '''// COOP_18_ARCHETYPES_V1
inline constexpr int kLocalCoopArchetypeCount = 18;
inline constexpr const char* kLocalCoopArchetypeNames[kLocalCoopArchetypeCount] = {
    "BRUISER",
    "GUNSLINGER",
    "SNIPER",
    "COMMANDO",
    "HEAVY GUNNER",
    "MELEE RAIDER",
    "UNARMED BRAWLER",
    "SCOUT",
    "SURVIVALIST",
    "MEDIC",
    "SCIENTIST",
    "MECHANIC",
    "DIPLOMAT",
    "THIEF",
    "DEMOLITIONIST",
    "RANGER",
    "LUCKY DRIFTER",
    "ENERGY SPECIALIST",
};

inline constexpr const char* kLocalCoopArchetypeRoles[kLocalCoopArchetypeCount] = {
    "TOUGH / CLOSE COMBAT",
    "PISTOLS / CRITICALS",
    "RIFLES / LONG RANGE",
    "AUTOMATICS / MOBILITY",
    "BIG GUNS / ENDURANCE",
    "MELEE / ARMOR",
    "UNARMED / SPEED",
    "SCOUTING / SMALL GUNS",
    "OUTDOORS / SELF RELIANCE",
    "FIRST AID / DOCTOR",
    "SCIENCE / ENERGY",
    "REPAIR / UTILITY",
    "SPEECH / BARTER",
    "LOCKPICK / STEAL",
    "TRAPS / EXPLOSIVES",
    "RIFLES / SURVIVAL",
    "LUCK / GENERALIST",
    "ENERGY WEAPONS / SCIENCE",
};

inline constexpr int kLocalCoopArchetypeStats[kLocalCoopArchetypeCount][PRIMARY_STAT_COUNT] = {
    { 8, 5, 8, 3, 4, 7, 5 },
    { 4, 8, 5, 5, 6, 8, 4 },
    { 4, 9, 4, 4, 7, 7, 5 },
    { 6, 7, 6, 3, 5, 8, 5 },
    { 8, 5, 8, 3, 4, 6, 6 },
    { 8, 5, 8, 4, 4, 7, 4 },
    { 7, 6, 7, 4, 5, 8, 3 },
    { 5, 8, 5, 4, 6, 8, 4 },
    { 6, 7, 7, 4, 6, 6, 4 },
    { 4, 6, 5, 6, 8, 6, 5 },
    { 3, 6, 4, 5, 10, 6, 6 },
    { 5, 6, 5, 4, 9, 6, 5 },
    { 3, 6, 4, 10, 7, 5, 5 },
    { 4, 7, 4, 5, 6, 9, 5 },
    { 5, 7, 5, 3, 7, 7, 6 },
    { 6, 8, 6, 4, 6, 6, 4 },
    { 4, 6, 5, 6, 5, 6, 8 },
    { 4, 7, 4, 4, 9, 7, 5 },
};
'''
if old not in s:
    raise SystemExit('archetype block not found')
s = s.replace(old, new, 1)

old_draw = '''    windowDrawText(player.joinWindow, choice, 380, 20, 92, _colorTable[992]);

    const int* stats = kLocalCoopArchetypeStats[player.archetype];
'''
new_draw = '''    windowDrawText(player.joinWindow, choice, 380, 20, 82, _colorTable[992]);
    windowDrawText(player.joinWindow, kLocalCoopArchetypeRoles[player.archetype], 380, 20, 106, _colorTable[992]);

    const int* stats = kLocalCoopArchetypeStats[player.archetype];
'''
if old_draw not in s:
    raise SystemExit('join menu role anchor not found')
s = s.replace(old_draw, new_draw, 1)
s = s.replace('''        126,\n        _colorTable[992]);''', '''        136,\n        _colorTable[992]);''', 1)
s = s.replace('''        154,\n        _colorTable[992]);''', '''        164,\n        _colorTable[992]);''', 1)
s = s.replace('''        194,\n        _colorTable[992]);''', '''        204,\n        _colorTable[992]);''', 1)

p.write_text(s, encoding='utf-8')
print('Expanded local co-op archetypes to 18 lore-friendly builds')
