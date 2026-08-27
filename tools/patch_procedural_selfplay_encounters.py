from pathlib import Path

p = Path('src/worldmap.cc')
s = p.read_text(encoding='utf-8')
marker = 'COOP_PROCEDURAL_SELFPLAY_ENCOUNTERS_V1'
if marker in s:
    print('already patched')
    raise SystemExit(0)

anchor = '''typedef struct Encounter {
    char name[40];
    int position;
    int spacing;
    int distance;
    int entriesLength;
    EncounterEntry entries[10];
} Encounter;
'''
if anchor not in s:
    raise SystemExit('Encounter struct anchor missing')

code = r'''

// COOP_PROCEDURAL_SELFPLAY_ENCOUNTERS_V1
// Before a random encounter is placed, two abstract sides play a short hidden
// tactical game against each other. The result changes formation, spacing and
// approach distance so random maps start as ambushes, pursuits, holds, scavenges,
// breakouts, hunts and messy crossfires instead of always two firing lines.
enum class CoopEncounterObjective {
    Ambush,
    Hold,
    Scavenge,
    Rescue,
    Hunt,
    Escape,
    Crossfire,
};

static const char* coopEncounterObjectiveName(CoopEncounterObjective objective)
{
    switch (objective) {
    case CoopEncounterObjective::Ambush: return "ambush";
    case CoopEncounterObjective::Hold: return "hold";
    case CoopEncounterObjective::Scavenge: return "scavenge";
    case CoopEncounterObjective::Rescue: return "rescue";
    case CoopEncounterObjective::Hunt: return "hunt";
    case CoopEncounterObjective::Escape: return "escape";
    case CoopEncounterObjective::Crossfire: return "crossfire";
    }
    return "unknown";
}

static void coopDirectEncounterBySelfPlay(Encounter* encounter)
{
    if (encounter == nullptr || encounter->entriesLength <= 0) return;

    int objectiveRoll = randomBetween(0, 6);
    CoopEncounterObjective objective = static_cast<CoopEncounterObjective>(objectiveRoll);

    // Two simple agents evaluate pressure, cover, morale and objective progress.
    // This is intentionally cheap: it runs before spawning and produces a state,
    // not an off-screen combat animation.
    int sideA = 45 + randomBetween(0, 35) + encounter->entriesLength * 2;
    int sideB = 45 + randomBetween(0, 35) + encounter->entriesLength * 2;
    int progressA = 0;
    int progressB = 0;
    int pressure = randomBetween(-10, 10);

    for (int turn = 0; turn < 10; turn++) {
        int coverA = randomBetween(0, 12);
        int coverB = randomBetween(0, 12);
        int aggressionA = randomBetween(4, 16) + std::max(0, pressure);
        int aggressionB = randomBetween(4, 16) + std::max(0, -pressure);
        int objectiveA = randomBetween(2, 12);
        int objectiveB = randomBetween(2, 12);

        sideA -= std::max(0, aggressionB - coverA) / 3;
        sideB -= std::max(0, aggressionA - coverB) / 3;
        progressA += objectiveA + (sideA > sideB ? 2 : 0);
        progressB += objectiveB + (sideB > sideA ? 2 : 0);
        pressure += (aggressionA - aggressionB) / 5;
        pressure = std::clamp(pressure, -14, 14);
    }

    int lead = (sideA + progressA) - (sideB + progressB);

    switch (objective) {
    case CoopEncounterObjective::Ambush:
        encounter->position = ENCOUNTER_FORMATION_TYPE_WEDGE;
        encounter->spacing = randomBetween(2, 4);
        break;
    case CoopEncounterObjective::Hold:
        encounter->position = ENCOUNTER_FORMATION_TYPE_DOUBLE_LINE;
        encounter->spacing = randomBetween(2, 5);
        break;
    case CoopEncounterObjective::Scavenge:
        encounter->position = ENCOUNTER_FORMATION_TYPE_HUDDLE;
        encounter->spacing = randomBetween(1, 3);
        break;
    case CoopEncounterObjective::Rescue:
        encounter->position = lead >= 0 ? ENCOUNTER_FORMATION_TYPE_CONE : ENCOUNTER_FORMATION_TYPE_HUDDLE;
        encounter->spacing = randomBetween(1, 3);
        break;
    case CoopEncounterObjective::Hunt:
        encounter->position = ENCOUNTER_FORMATION_TYPE_SURROUNDING;
        encounter->spacing = randomBetween(2, 4);
        break;
    case CoopEncounterObjective::Escape:
        encounter->position = ENCOUNTER_FORMATION_TYPE_STRAIGHT_LINE;
        encounter->spacing = randomBetween(3, 6);
        break;
    case CoopEncounterObjective::Crossfire:
        encounter->position = ENCOUNTER_FORMATION_TYPE_CONE;
        encounter->spacing = randomBetween(3, 5);
        break;
    }

    // A side that won the hidden setup game starts with the positional edge,
    // while the losing side is pushed farther away. Existing teams, scripts,
    // inventories and encounter identities remain untouched.
    for (int index = 0; index < encounter->entriesLength; index++) {
        EncounterEntry& entry = encounter->entries[index];
        int base = entry.distance != 0 ? entry.distance : 5;
        int sideBias = (index & 1) == 0 ? lead : -lead;
        int delta = sideBias > 15 ? -2 : (sideBias < -15 ? 3 : randomBetween(-1, 2));
        entry.distance = std::clamp(base + delta, 2, 14);
    }

    debugPrint("[COOP ENCOUNTER DIRECTOR] objective=%s lead=%d formation=%d spacing=%d entries=%d\n",
        coopEncounterObjectiveName(objective), lead, encounter->position, encounter->spacing, encounter->entriesLength);
}
'''
s = s.replace(anchor, anchor + code, 1)

hook = '    if (wmSetupRndNextTileNumInit(encounter) == -1) {'
if hook not in s:
    raise SystemExit('random encounter placement hook missing')
s = s.replace(hook,
    '    coopDirectEncounterBySelfPlay(encounter);\n\n' + hook, 1)

p.write_text(s, encoding='utf-8')
print('added procedural self-play encounter director')
