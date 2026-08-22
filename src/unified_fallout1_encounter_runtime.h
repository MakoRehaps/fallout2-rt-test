#ifndef UNIFIED_FALLOUT1_ENCOUNTER_RUNTIME_H
#define UNIFIED_FALLOUT1_ENCOUNTER_RUNTIME_H

#include "unified_fallout1_encounter_profile.h"
#include "unified_fallout1_worldmap_globals.h"
#include "unified_worldmap_state_profile.h"

namespace fallout {

inline constexpr int kUnifiedFallout1WorldTerrainGvar = 65;

// Exact Fallout 1 WorldEcounTable. These region ids are written to
// GVAR_WORLD_TERRAIN before loading random/special encounter maps so their
// original scripts know which encounter population/region generated them.
inline constexpr unsigned char kUnifiedFallout1EncounterRegionTable[kUnifiedFallout1EncounterRows][kUnifiedFallout1EncounterColumns] = {
    { 11, 11, 11, 11, 11, 11, 11, 11, 11, 2, 2, 2, 2, 2, 2, 4, 4, 4, 2, 2, 5, 5, 5, 0, 0, 0, 0, 0 },
    { 11, 11, 11, 11, 11, 11, 11, 11, 11, 2, 2, 2, 2, 2, 2, 4, 4, 4, 4, 5, 5, 5, 5, 5, 0, 0, 0, 0 },
    { 11, 11, 11, 11, 11, 11, 11, 11, 11, 0, 2, 2, 2, 2, 2, 4, 4, 4, 4, 2, 5, 5, 5, 5, 2, 0, 0, 0 },
    { 11, 11, 11, 11, 11, 11, 11, 11, 11, 0, 0, 2, 2, 2, 2, 2, 4, 4, 4, 2, 0, 0, 6, 6, 2, 0, 0, 0 },
    { 11, 11, 11, 11, 11, 11, 11, 11, 11, 0, 0, 0, 2, 2, 2, 2, 4, 4, 2, 2, 0, 0, 6, 6, 2, 0, 0, 0 },
    { 11, 11, 11, 11, 11, 11, 11, 11, 11, 0, 0, 0, 2, 2, 2, 2, 0, 0, 0, 2, 2, 0, 2, 0, 0, 0, 0, 0 },
    { 15, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0 },
    { 15, 15, 2, 2, 2, 0, 0, 0, 0, 0, 10, 10, 10, 2, 2, 2, 2, 0, 0, 0, 2, 0, 2, 0, 0, 0, 0, 0 },
    { 15, 15, 15, 2, 0, 0, 0, 0, 0, 10, 10, 10, 10, 2, 0, 2, 2, 0, 0, 0, 2, 2, 2, 0, 0, 0, 0, 0 },
    { 15, 15, 15, 0, 0, 0, 0, 0, 0, 10, 10, 10, 10, 2, 2, 2, 7, 7, 7, 7, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 15, 15, 15, 15, 0, 0, 0, 0, 0, 10, 10, 10, 10, 2, 2, 2, 7, 7, 7, 7, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 15, 15, 15, 15, 15, 0, 0, 0, 0, 0, 10, 10, 10, 0, 2, 2, 7, 7, 7, 7, 7, 9, 9, 9, 9, 0, 0, 0 },
    { 15, 15, 15, 15, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 8, 8, 0, 9, 9, 9, 9, 9, 0, 0, 0 },
    { 15, 15, 15, 15, 15, 15, 0, 2, 0, 0, 0, 0, 0, 0, 2, 14, 8, 8, 8, 0, 9, 9, 9, 9, 9, 0, 0, 0 },
    { 15, 15, 15, 15, 15, 15, 0, 2, 0, 0, 0, 0, 0, 0, 14, 14, 8, 8, 8, 0, 9, 9, 9, 9, 9, 0, 0, 0 },
    { 15, 15, 15, 15, 15, 15, 1, 1, 1, 3, 1, 1, 1, 1, 1, 14, 8, 8, 8, 8, 8, 9, 9, 9, 9, 10, 1, 1 },
    { 15, 15, 15, 15, 15, 15, 15, 15, 1, 1, 3, 3, 3, 13, 13, 13, 13, 13, 13, 13, 13, 1, 9, 9, 9, 1, 1, 1 },
    { 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 1, 1, 13, 13, 13, 13, 13, 13, 13, 13, 1, 1, 1, 1, 1, 1, 1 },
    { 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 3, 1, 1, 1, 1, 1, 1 },
    { 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 13, 13, 13, 13, 13, 13, 13, 3, 1, 1, 1, 1, 1, 1 },
    { 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 13, 13, 15, 13, 13, 13, 3, 3, 3, 1, 1, 1, 1 },
    { 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 1, 1, 3, 3, 3, 3, 3, 1, 1, 1 },
    { 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 1, 1, 3, 3, 3, 3, 3, 1, 1, 1 },
    { 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 1, 3, 3, 3, 3, 3, 1, 1, 1 },
    { 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 1, 3, 3, 3, 12, 12, 12, 12, 1 },
    { 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 3, 3, 3, 12, 12, 12, 12, 1 },
    { 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 3, 3, 12, 12, 12, 12, 1 },
    { 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 3, 12, 12, 12, 12, 12, 1 },
    { 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 1, 12, 12, 3, 1, 1, 1 },
    { 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 1, 1, 1, 1, 1, 1, 1 },
};

struct UnifiedFallout1EncounterSelection {
    bool triggered;
    bool special;
    int mapIdx;
    int regionId;
};

inline int unifiedFallout1EncounterRegionAt(int worldX, int worldY)
{
    if (!unifiedFallout1EncounterCellIsValid(worldX, worldY)) {
        return -1;
    }

    return kUnifiedFallout1EncounterRegionTable[worldY / kUnifiedFallout1EncounterCellSize]
                                              [worldX / kUnifiedFallout1EncounterCellSize];
}

inline void unifiedFallout1SetEncounterRegionGlobal(int regionId)
{
    if (gGameGlobalVars == nullptr
        || kUnifiedFallout1WorldTerrainGvar < 0
        || kUnifiedFallout1WorldTerrainGvar >= gGameGlobalVarsLength) {
        return;
    }

    gGameGlobalVars[kUnifiedFallout1WorldTerrainGvar] = regionId;
}

inline bool unifiedFallout1RollTravelEncounterAt(int worldX, int worldY)
{
    // Original F1 checks InCity before applying WorldEcountChanceTable.
    if (unifiedFallout1TownAtWorldPos(worldX, worldY) != -1) {
        return false;
    }

    return unifiedFallout1RollRandomEncounterAt(worldX, worldY);
}

inline UnifiedFallout1EncounterSelection unifiedFallout1SelectTravelEncounter(
    int worldX,
    int worldY,
    int luck,
    int explorerLevel)
{
    UnifiedFallout1EncounterSelection result {};
    result.triggered = false;
    result.special = false;
    result.mapIdx = -1;
    result.regionId = unifiedFallout1EncounterRegionAt(worldX, worldY);

    if (result.regionId == -1 || !unifiedFallout1RollTravelEncounterAt(worldX, worldY)) {
        return result;
    }

    result.triggered = true;

    int specialMap = unifiedFallout1RollSpecialEncounter(luck, explorerLevel);
    if (specialMap != -1) {
        result.special = true;
        result.mapIdx = specialMap;
    } else {
        result.mapIdx = unifiedFallout1RollRandomTerrainMap(worldX, worldY);
    }

    if (result.mapIdx == -1) {
        result.triggered = false;
        return result;
    }

    unifiedFallout1SetEncounterRegionGlobal(result.regionId);
    return result;
}

inline int unifiedFallout1SelectTerrainDropMap(int worldX, int worldY)
{
    int regionId = unifiedFallout1EncounterRegionAt(worldX, worldY);
    int mapIdx = unifiedFallout1RollRandomTerrainMap(worldX, worldY);
    if (regionId == -1 || mapIdx == -1) {
        return -1;
    }

    unifiedFallout1SetEncounterRegionGlobal(regionId);
    return mapIdx;
}

} // namespace fallout

#endif /* UNIFIED_FALLOUT1_ENCOUNTER_RUNTIME_H */
