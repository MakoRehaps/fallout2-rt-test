#ifndef UNIFIED_FALLOUT1_ENCOUNTER_BRIDGE_H
#define UNIFIED_FALLOUT1_ENCOUNTER_BRIDGE_H

#include "unified_campaign.h"
#include "unified_worldmap_profile.h"

namespace fallout {

int wmSetupRandomEncounter();
void wmForceEncounter(int map, unsigned int flags);

inline constexpr unsigned int kUnifiedEncounterFlagNoCar = 0x01;
inline constexpr unsigned int kUnifiedEncounterFlagLock = 0x02;
inline constexpr unsigned int kUnifiedEncounterFlagNoIcon = 0x04;
inline constexpr unsigned int kUnifiedEncounterFlagSpecialIcon = 0x08;
inline constexpr unsigned int kUnifiedEncounterFlagFadeOut = 0x10;

struct UnifiedFallout1ForcedEncounterState {
    int mapIdx;
    unsigned int flags;
    bool valid;
    bool locked;
};

inline UnifiedFallout1ForcedEncounterState gUnifiedFallout1ForcedEncounter = {
    -1,
    0,
    false,
    false,
};

inline void unifiedFallout1EncounterBridgeReset()
{
    gUnifiedFallout1ForcedEncounter.mapIdx = -1;
    gUnifiedFallout1ForcedEncounter.flags = 0;
    gUnifiedFallout1ForcedEncounter.valid = false;
    gUnifiedFallout1ForcedEncounter.locked = false;
}

inline bool unifiedFallout1HasForcedEncounter()
{
    return gUnifiedFallout1ForcedEncounter.valid;
}

inline bool unifiedFallout1PeekForcedEncounter(int* mapIdxPtr, unsigned int* flagsPtr)
{
    if (!gUnifiedFallout1ForcedEncounter.valid) {
        return false;
    }

    if (mapIdxPtr != nullptr) {
        *mapIdxPtr = gUnifiedFallout1ForcedEncounter.mapIdx;
    }
    if (flagsPtr != nullptr) {
        *flagsPtr = gUnifiedFallout1ForcedEncounter.flags;
    }
    return true;
}

inline bool unifiedFallout1ConsumeForcedEncounter(int* mapIdxPtr, unsigned int* flagsPtr)
{
    if (!unifiedFallout1PeekForcedEncounter(mapIdxPtr, flagsPtr)) {
        return false;
    }

    unifiedFallout1EncounterBridgeReset();
    return true;
}

inline int unifiedWmSetupRandomEncounter()
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmSetupRandomEncounter();
    }

    // F2CE populates a just-loaded random map from wmEncounterTableList here.
    // Fallout 1 does not use that data model. Its selected DESERT/MOUNTN/CITY/
    // COAST/special map has already received GVAR_WORLD_TERRAIN, and the map's
    // original script performs the encounter-specific setup. Running F2 setup
    // would dereference tables that are deliberately never initialized in F1.
    return 0;
}

inline void unifiedWmForceEncounter(int map, unsigned int flags)
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        wmForceEncounter(map, flags);
        return;
    }

    // Mirror F2/Sfall's locked-force semantics without borrowing wmGenData.
    if (gUnifiedFallout1ForcedEncounter.valid && gUnifiedFallout1ForcedEncounter.locked) {
        return;
    }

    if (map < 0 || map >= kUnifiedFallout1MapCount) {
        return;
    }

    gUnifiedFallout1ForcedEncounter.mapIdx = map;
    gUnifiedFallout1ForcedEncounter.flags = flags;
    gUnifiedFallout1ForcedEncounter.valid = true;
    gUnifiedFallout1ForcedEncounter.locked = (flags & kUnifiedEncounterFlagLock) != 0;
}

} // namespace fallout

#ifndef WORLD_MAP_H
#define wmSetupRandomEncounter unifiedWmSetupRandomEncounter
#define wmForceEncounter unifiedWmForceEncounter
#endif

#endif /* UNIFIED_FALLOUT1_ENCOUNTER_BRIDGE_H */
