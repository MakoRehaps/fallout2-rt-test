#ifndef UNIFIED_FALLOUT1_ENCOUNTER_PROFILE_H
#define UNIFIED_FALLOUT1_ENCOUNTER_PROFILE_H

#include "random.h"
#include "unified_fallout1_worldmap_state.h"

namespace fallout {

inline constexpr int kUnifiedFallout1EncounterColumns = 28;
inline constexpr int kUnifiedFallout1EncounterRows = 30;
inline constexpr int kUnifiedFallout1EncounterCellSize = 50;

enum class UnifiedFallout1Terrain : unsigned char {
    Desert = 0,
    Mountain = 1,
    City = 2,
    Coast = 3,
};

// Exact Fallout 1 WorldTerraTable. Values select the original random-map pool:
// desert, mountain, city or coast.
inline constexpr unsigned char kUnifiedFallout1TerrainTable[kUnifiedFallout1EncounterRows][kUnifiedFallout1EncounterColumns] = {
    { 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0 },
    { 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0 },
    { 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0 },
    { 3, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 2, 2 },
    { 3, 3, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 2, 0 },
    { 3, 3, 3, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0 },
    { 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 3, 3, 3, 3, 3, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0 },
    { 3, 3, 3, 3, 3, 3, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0 },
    { 3, 3, 3, 3, 3, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0 },
    { 3, 3, 3, 3, 3, 3, 2, 2, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0 },
    { 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 2, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 0, 2, 2, 2, 2, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0 },
    { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0 },
    { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 3, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0 },
    { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0 },
    { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0 },
    { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 1, 1, 1, 1, 1, 0, 0, 0 },
    { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 1, 1, 1, 1, 1, 0, 0, 0 },
    { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 0, 0, 0 },
    { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 0, 0 },
    { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 1, 0, 0, 0, 0, 0, 0 },
    { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 1, 0, 0, 0 },
    { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0 },
};

// Exact Fallout 1 WorldEcountChanceTable. A travel-day encounter roll uses
// 3d6 and thresholds <6, <7, <9 or <10 for classes 0..3 respectively.
inline constexpr unsigned char kUnifiedFallout1EncounterChanceTable[kUnifiedFallout1EncounterRows][kUnifiedFallout1EncounterColumns] = {
    { 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 3 },
    { 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3 },
    { 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 3, 3 },
    { 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 3, 3 },
    { 3, 3, 3, 3, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2 },
    { 2, 2, 2, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 2, 2, 2 },
    { 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 2, 2, 2 },
    { 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 2, 2, 2 },
    { 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 2, 2 },
    { 2, 2, 2, 2, 2, 1, 1, 2, 2, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2 },
    { 2, 2, 2, 2, 2, 1, 1, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2 },
    { 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2 },
    { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 2, 2, 2, 2, 1, 2, 1, 1, 1, 1, 2, 2 },
    { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 2 },
    { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2 },
    { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 2 },
    { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1 },
    { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 2 },
    { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 2, 2, 2 },
    { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 2, 2, 2 },
    { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 2, 2, 2 },
    { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 2, 2 },
    { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1 },
    { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 3, 3, 3 },
    { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3 },
    { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3 },
    { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3 },
    { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3 },
    { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3 },
    { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3 },
};

inline constexpr int kUnifiedFallout1RandomMapIds[4][3] = {
    { 0, 1, 2 }, // DESERT1, DESERT2, DESERT3
    { 49, 50, -1 }, // MOUNTN1, MOUNTN2
    { 19, -1, -1 }, // CITY1
    { 20, 21, -1 }, // COAST1, COAST2
};

inline constexpr int kUnifiedFallout1SpecialMapIds[6] = {
    51, // FOOT
    53, // TALKCOW
    54, // USEDCAR
    52, // TARDIS
    23, // FSAUSER
    22, // COLATRUK
};

struct UnifiedFallout1SpecialRange {
    int start;
    int end;
};

inline constexpr UnifiedFallout1SpecialRange kUnifiedFallout1SpecialRanges[6] = {
    { 1, 30 },
    { 31, 50 },
    { 51, 70 },
    { 71, 80 },
    { 81, 90 },
    { 91, 100 },
};

inline bool unifiedFallout1EncounterCellIsValid(int worldX, int worldY)
{
    return worldX >= 0
        && worldY >= 0
        && worldX / kUnifiedFallout1EncounterCellSize < kUnifiedFallout1EncounterColumns
        && worldY / kUnifiedFallout1EncounterCellSize < kUnifiedFallout1EncounterRows;
}

inline int unifiedFallout1TerrainAt(int worldX, int worldY)
{
    if (!unifiedFallout1EncounterCellIsValid(worldX, worldY)) {
        return -1;
    }

    return kUnifiedFallout1TerrainTable[worldY / kUnifiedFallout1EncounterCellSize]
                                      [worldX / kUnifiedFallout1EncounterCellSize];
}

inline int unifiedFallout1EncounterChanceClassAt(int worldX, int worldY)
{
    if (!unifiedFallout1EncounterCellIsValid(worldX, worldY)) {
        return -1;
    }

    return kUnifiedFallout1EncounterChanceTable[worldY / kUnifiedFallout1EncounterCellSize]
                                              [worldX / kUnifiedFallout1EncounterCellSize];
}

inline bool unifiedFallout1EncounterRollTriggers(int chanceClass, int threeD6)
{
    switch (chanceClass) {
    case 0:
        return threeD6 < 6;
    case 1:
        return threeD6 < 7;
    case 2:
        return threeD6 < 9;
    case 3:
        return threeD6 < 10;
    default:
        return false;
    }
}

inline bool unifiedFallout1RollRandomEncounterAt(int worldX, int worldY)
{
    int chanceClass = unifiedFallout1EncounterChanceClassAt(worldX, worldY);
    if (chanceClass == -1) {
        return false;
    }

    int threeD6 = randomBetween(1, 6) + randomBetween(1, 6) + randomBetween(1, 6);
    return unifiedFallout1EncounterRollTriggers(chanceClass, threeD6);
}

inline int unifiedFallout1RollRandomTerrainMap(int worldX, int worldY)
{
    int terrain = unifiedFallout1TerrainAt(worldX, worldY);
    if (terrain < 0 || terrain > 3) {
        return -1;
    }

    int count = 0;
    while (count < 3 && kUnifiedFallout1RandomMapIds[terrain][count] != -1) {
        count++;
    }
    if (count == 0) {
        return -1;
    }

    return kUnifiedFallout1RandomMapIds[terrain][randomBetween(0, count - 1)];
}

inline int unifiedFallout1RollSpecialEncounter(int luck, int explorerLevel)
{
    UnifiedFallout1WorldMapState& state = unifiedFallout1WorldMapGetState();
    if ((state.specialEncounterFlags & 0x3F) == 0x3F) {
        return -1;
    }

    while (true) {
        int specialCheck = randomBetween(1, 6)
            + randomBetween(1, 6)
            + randomBetween(1, 6)
            - 5
            + luck
            + 2 * explorerLevel;

        if (specialCheck < 18) {
            return -1;
        }

        int selection = randomBetween(1, 100);
        for (int index = 0; index < 6; index++) {
            const UnifiedFallout1SpecialRange& range = kUnifiedFallout1SpecialRanges[index];
            if (selection < range.start || selection > range.end) {
                continue;
            }

            if ((state.specialEncounterFlags & (1 << index)) != 0) {
                // Original F1 restarts the entire special check if the selected
                // one-shot encounter was already consumed.
                break;
            }

            state.specialEncounterFlags |= 1 << index;
            return kUnifiedFallout1SpecialMapIds[index];
        }
    }
}

} // namespace fallout

#endif /* UNIFIED_FALLOUT1_ENCOUNTER_PROFILE_H */
