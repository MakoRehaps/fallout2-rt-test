#ifndef UNIFIED_FALLOUT1_WORLDMAP_GLOBALS_H
#define UNIFIED_FALLOUT1_WORLDMAP_GLOBALS_H

#include "unified_campaign.h"
#include "unified_fallout1_worldmap_state.h"

namespace fallout {

// Avoid pulling game.h/scripts.h into the low-level world-map profile headers.
// These are the existing Fallout 2 CE globals/functions with identical runtime
// roles when the Fallout 1 content profile is mounted.
extern int* gGameGlobalVars;
extern int gGameGlobalVarsLength;
unsigned int gameTimeGetTime();

inline constexpr int kUnifiedFallout1MasterBlownGvar = 18;
inline constexpr int kUnifiedFallout1VatsBlownGvar = 17;

inline constexpr int kUnifiedFallout1CityKnownGvars[12] = {
    67, // Vault 13
    70, // Vault 15
    68, // Shady Sands
    71, // Junktown
    69, // Raiders
    72, // Necropolis
    73, // The Hub
    74, // Brotherhood
    78, // Military Base
    76, // The Glow
    75, // Boneyard
    77, // Cathedral
};

// Original Fallout 1 ElevXgvar table. A zero terminates the entrance list for
// a town. Values 558-600 are preserved exactly from fallout1-ce.
inline constexpr int kUnifiedFallout1EntranceKnownGvars[12][7] = {
    { 558, 559, 560, 561, 0, 0, 0 },
    { 562, 563, 564, 565, 0, 0, 0 },
    { 566, 567, 568, 0, 0, 0, 0 },
    { 569, 570, 571, 0, 0, 0, 0 },
    { 572, 0, 0, 0, 0, 0, 0 },
    { 573, 574, 575, 0, 0, 0, 0 },
    { 576, 577, 578, 579, 580, 581, 0 },
    { 582, 583, 584, 585, 586, 0, 0 },
    { 587, 588, 589, 590, 591, 0, 0 },
    { 592, 593, 0, 0, 0, 0, 0 },
    { 594, 595, 596, 597, 598, 0, 0 },
    { 599, 600, 0, 0, 0, 0, 0 },
};

inline constexpr int kUnifiedFallout1TownColumns[12] = {
    16, 25, 21, 17, 22, 22, 17, 12, 3, 24, 15, 15,
};

inline constexpr int kUnifiedFallout1TownRows[12] = {
    1, 1, 1, 10, 3, 13, 14, 9, 1, 25, 18, 20,
};

inline int unifiedFallout1ReadGlobal(int index)
{
    if (gGameGlobalVars == nullptr || index < 0 || index >= gGameGlobalVarsLength) {
        return 0;
    }

    return gGameGlobalVars[index];
}

inline void unifiedFallout1WorldMapSyncFromGlobals()
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return;
    }

    UnifiedFallout1WorldMapState& state = unifiedFallout1WorldMapGetState();

    // Fallout 1's UpdateTownStatus intentionally starts at town 1. Vault 13 is
    // handled by its special time-based rule below.
    for (int town = 1; town < 12; town++) {
        for (int entrance = 0; entrance < 7; entrance++) {
            int gvar = kUnifiedFallout1EntranceKnownGvars[town][entrance];
            if (gvar == 0) {
                break;
            }

            state.townSelectionKnowledge[town][entrance] =
                unifiedFallout1ReadGlobal(gvar) != 0 ? 1 : 0;
        }

        if (unifiedFallout1ReadGlobal(kUnifiedFallout1CityKnownGvars[town]) == 1) {
            int row = kUnifiedFallout1TownRows[town];
            int column = kUnifiedFallout1TownColumns[town];
            if (state.worldGrid[row][column] == 0) {
                state.worldGrid[row][column] = 1;
            }

            state.firstVisitFlags |= 1 << town;
            state.townSelectionKnowledge[town][0] = 1;
        }
    }

    // Original F1 behavior: after one in-game day all four Vault 13 town-map
    // selections are available; during day zero only its first entrance is.
    state.townSelectionKnowledge[0][0] = 1;
    bool vault13Expanded = gameTimeGetTime() / 864000 != 0;
    for (int entrance = 1; entrance <= 3; entrance++) {
        state.townSelectionKnowledge[0][entrance] = vault13Expanded ? 1 : 0;
    }

    // Fallout 1 maintains three synthetic town-map rows for destroyed/alternate
    // versions of Cathedral, Military Base and Brotherhood. They are not normal
    // world-map towns but their knowledge flags are part of the saved 15x7 grid.
    if (unifiedFallout1ReadGlobal(kUnifiedFallout1MasterBlownGvar) != 0
        || state.townSelectionKnowledge[11][0] != 0
        || state.townSelectionKnowledge[11][1] != 0) {
        state.townSelectionKnowledge[12][0] = 1;
    }

    if (unifiedFallout1ReadGlobal(kUnifiedFallout1VatsBlownGvar) != 0
        || state.townSelectionKnowledge[8][0] != 0
        || state.townSelectionKnowledge[8][1] != 0
        || state.townSelectionKnowledge[8][2] != 0
        || state.townSelectionKnowledge[8][3] != 0
        || state.townSelectionKnowledge[8][4] != 0) {
        state.townSelectionKnowledge[13][0] = 1;
    }

    if (state.townSelectionKnowledge[7][1] != 0
        || state.townSelectionKnowledge[7][2] != 0
        || state.townSelectionKnowledge[7][3] != 0
        || state.townSelectionKnowledge[7][4] != 0) {
        state.townSelectionKnowledge[14][0] = 1;
    }
}

} // namespace fallout

#endif /* UNIFIED_FALLOUT1_WORLDMAP_GLOBALS_H */
