#!/usr/bin/env python3
from pathlib import Path

MARKER = 'COOP_PIPBOY_ECOLOGY_V1'

# 1) Deterministic population/ecology band on the authoritative unified grid.
p = Path('src/unified_world_system.h')
s = p.read_text(encoding='utf-8')
if MARKER not in s:
    anchor = '''inline int unifiedWorldSystemCellIndex(int cellX, int cellY)'''
    if anchor not in s:
        raise SystemExit('world-system ecology insertion anchor not found')
    ecology = r'''
// COOP_PIPBOY_ECOLOGY_V1
// Population belongs to the physical world cell, not to a one-off random roll.
// This makes the Pip-Boy map predictive: most land is quiet, wildlife country
// is common, and sustained human activity is deliberately uncommon.
enum class UnifiedWorldSystemPopulation : uint8_t {
    Quiet = 0,
    Wildlife = 1,
    HumanActivity = 2,
    MixedDanger = 3,
};

inline UnifiedWorldSystemPopulation unifiedWorldSystemPopulationForSeed(uint32_t seed)
{
    uint32_t roll = unifiedWorldSystemMixSeed(seed ^ 0x45434F4C) % 100; // "ECOL"
    if (roll < 50) return UnifiedWorldSystemPopulation::Quiet;
    if (roll < 88) return UnifiedWorldSystemPopulation::Wildlife;
    if (roll < 96) return UnifiedWorldSystemPopulation::HumanActivity;
    return UnifiedWorldSystemPopulation::MixedDanger;
}

inline const char* unifiedWorldSystemPopulationName(UnifiedWorldSystemPopulation population)
{
    switch (population) {
    case UnifiedWorldSystemPopulation::Quiet: return "QUIET / EMPTY";
    case UnifiedWorldSystemPopulation::Wildlife: return "WILDLIFE";
    case UnifiedWorldSystemPopulation::HumanActivity: return "HUMAN ACTIVITY";
    case UnifiedWorldSystemPopulation::MixedDanger: return "MIXED DANGER";
    }
    return "UNKNOWN";
}

'''
    s = s.replace(anchor, ecology + anchor, 1)

    # Materialize the derived ecology byte when a world is initialized or a
    # cleared cell regenerates. Readers still derive from seed for old saves.
    old = '''            cell.templateMapIdx = -1;
            cell.temporaryDungeonMapIdx = -1;'''
    new = '''            cell.terrain = static_cast<uint8_t>(unifiedWorldSystemPopulationForSeed(cell.seed));
            cell.templateMapIdx = -1;
            cell.temporaryDungeonMapIdx = -1;'''
    if old not in s:
        raise SystemExit('world reset cell anchor not found')
    s = s.replace(old, new, 1)

    old = '''    cell.templateMapIdx = static_cast<int16_t>(
        unifiedWorldSystemPoolMap(game, cell.seed));'''
    new = '''    cell.terrain = static_cast<uint8_t>(unifiedWorldSystemPopulationForSeed(cell.seed));
    cell.templateMapIdx = static_cast<int16_t>(
        unifiedWorldSystemPoolMap(game, cell.seed));'''
    if old not in s:
        raise SystemExit('world regen cell anchor not found')
    s = s.replace(old, new, 1)
    p.write_text(s, encoding='utf-8')
    print('Patched unified-world ecology bands')
else:
    print('Unified-world ecology already patched')

# 2) Gate vanilla encounter results through the cell ecology. Rare SPECIAL
# encounters remain untouched. This does not invent NPCs or creatures; it uses
# the original encounter tables but stops human groups from occupying wildlife
# and quiet cells.
p = Path('src/worldmap.cc')
s = p.read_text(encoding='utf-8')
if '// COOP_WORLD_ENCOUNTER_ECOLOGY_GATE_V1' not in s:
    old = '''    if (wmRndEncounterPick() == -1) {
        wmGenData.encounterMapId = -1;
        wmGenData.encounterTableId = -1;
        wmGenData.encounterEntryId = -1;
        return;
    }'''
    new = r'''    // COOP_WORLD_ENCOUNTER_ECOLOGY_GATE_V1
    const UnifiedWorldSystemCellState* ecologyCell = unifiedWorldSystemGetCell(
        unifiedCampaignGetActiveGame(),
        travel.currentCellX[gameIndex],
        travel.currentCellY[gameIndex]);
    UnifiedWorldSystemPopulation ecology = ecologyCell != nullptr
        ? unifiedWorldSystemPopulationForSeed(ecologyCell->seed)
        : UnifiedWorldSystemPopulation::MixedDanger;

    auto clearEncounter = []() {
        wmGenData.encounterMapId = -1;
        wmGenData.encounterTableId = -1;
        wmGenData.encounterEntryId = -1;
    };

    // Half of the physical grid is genuinely quiet wilderness. A map can still
    // contain scenery, salvage, vehicles, weather, etc.; it simply does not
    // manufacture a group of people every time the party crosses a cell.
    if (ecology == UnifiedWorldSystemPopulation::Quiet) {
        clearEncounter();
        debugPrint("[WORLD ECOLOGY] quiet cell x=%d y=%d - no random critter group\n",
            travel.currentCellX[gameIndex], travel.currentCellY[gameIndex]);
        return;
    }

    if (wmRndEncounterPick() == -1) {
        clearEncounter();
        return;
    }

    // Classify the encounter picked by Fallout's own table using kill types.
    // Creature kill types count as wildlife; people, ghouls, super mutants,
    // robots and unknown scripted groups count as human/activity. Mixed groups
    // are reserved for the small MIXED DANGER portion of the grid.
    auto pickedPopulation = []() -> int {
        if (wmGenData.encounterTableId < 0
            || wmGenData.encounterTableId >= wmMaxEncounterInfoTables) return 3;
        EncounterTable& table = wmEncounterTableList[wmGenData.encounterTableId];
        if (wmGenData.encounterEntryId < 0
            || wmGenData.encounterEntryId >= table.entriesLength) return 3;
        EncounterTableEntry& tableEntry = table.entries[wmGenData.encounterEntryId];
        if ((tableEntry.flags & ENCOUNTER_ENTRY_SPECIAL) != 0) return 4;

        int wildlife = 0;
        int activity = 0;
        for (int sub = 0; sub < tableEntry.subEntiesLength; sub++) {
            int encounterIndex = tableEntry.subEntries[sub].encounterIndex;
            if (encounterIndex < 0 || encounterIndex >= wmMaxEncBaseTypes) continue;
            Encounter& encounter = wmEncBaseTypeList[encounterIndex];
            for (int entryIndex = 0; entryIndex < encounter.entriesLength; entryIndex++) {
                int pid = encounter.entries[entryIndex].pid;
                if (pid < 0 || PID_TYPE(pid) != OBJ_TYPE_CRITTER) continue;
                Proto* proto = nullptr;
                if (protoGetProto(pid, &proto) != 0 || proto == nullptr) continue;
                int killType = proto->critter.data.killType;
                switch (killType) {
                case KILL_TYPE_BRAHMIN:
                case KILL_TYPE_RADSCORPION:
                case KILL_TYPE_RAT:
                case KILL_TYPE_FLOATER:
                case KILL_TYPE_CENTAUR:
                case KILL_TYPE_DOG:
                case KILL_TYPE_MANTIS:
                case KILL_TYPE_DEATH_CLAW:
                case KILL_TYPE_PLANT:
                case KILL_TYPE_GECKO:
                case KILL_TYPE_ALIEN:
                case KILL_TYPE_GIANT_ANT:
                    wildlife++;
                    break;
                default:
                    activity++;
                    break;
                }
            }
        }
        if (wildlife > 0 && activity == 0) return 1;
        if (activity > 0 && wildlife == 0) return 2;
        return 3;
    };

    int picked = pickedPopulation();
    bool allowed = picked == 4
        || (ecology == UnifiedWorldSystemPopulation::Wildlife && picked == 1)
        || (ecology == UnifiedWorldSystemPopulation::HumanActivity && picked == 2)
        || ecology == UnifiedWorldSystemPopulation::MixedDanger;
    if (!allowed) {
        debugPrint("[WORLD ECOLOGY] filtered encounter class=%d cellClass=%d x=%d y=%d\n",
            picked, static_cast<int>(ecology),
            travel.currentCellX[gameIndex], travel.currentCellY[gameIndex]);
        clearEncounter();
        return;
    }
'''
    if old not in s:
        raise SystemExit('worldmap encounter-pick anchor not found')
    s = s.replace(old, new, 1)
    p.write_text(s, encoding='utf-8')
    print('Patched worldmap encounter ecology gate')
else:
    print('Worldmap ecology gate already patched')

# 3) Show the same persistent ecology class on the selected Pip-Boy grid cell.
p = Path('src/pipboy.cc')
s = p.read_text(encoding='utf-8')
if '// COOP_PIPBOY_ECOLOGY_DISPLAY_V1' not in s:
    old = '''    windowDrawText(gPipboyWindow, line, mapWidth, left, 374, _colorTable[992]);
    windowDrawText(gPipboyWindow,
        "D-PAD: GUIDE   A: TARGET   LB/RB: MENUS   B: CLOSE",
        mapWidth, left, 390, _colorTable[992]);'''
    new = '''    // COOP_PIPBOY_ECOLOGY_DISPLAY_V1
    char ecologyLine[96];
    UnifiedWorldSystemPopulation population =
        unifiedWorldSystemPopulationForSeed(selectedCell.seed);
    snprintf(ecologyLine, sizeof(ecologyLine), "AREA: %s", unifiedWorldSystemPopulationName(population));
    windowDrawText(gPipboyWindow, ecologyLine, mapWidth, left, 374, _colorTable[992]);
    windowDrawText(gPipboyWindow, line, mapWidth, left, 390, _colorTable[992]);
    windowDrawText(gPipboyWindow,
        "D-PAD: GUIDE   A: TARGET   LB/RB: MENUS   B: CLOSE",
        mapWidth, left, 406, _colorTable[992]);'''
    if old not in s:
        raise SystemExit('Pip-Boy target-status anchor not found')
    s = s.replace(old, new, 1)
    p.write_text(s, encoding='utf-8')
    print('Patched Pip-Boy ecology display')
else:
    print('Pip-Boy ecology display already patched')
