#ifndef UNIFIED_LOADED_MAP_PROFILE_H
#define UNIFIED_LOADED_MAP_PROFILE_H

#include "platform_compat.h"
#include "unified_campaign.h"
#include "unified_worldmap_profile.h"
#include "unified_worldmap_state_profile.h"

namespace fallout {

// Stock Fallout 2 CE symbols. The wrappers below are installed at call sites
// only; worldmap.cc itself remains the untouched Fallout 2 backend.
bool wmMapIsSaveable();
bool wmMapDeadBodiesAge();
bool wmMapCanRestHere(int elevation);
bool wmMapPipboyActive();
int wmMapMarkVisited(int mapIdx);
int wmMapMarkMapEntranceState(int mapIdx, int elevation, int state);
bool wmMapIsKnown(int mapIdx);
int mapLoadByName(char* fileName);

inline bool unifiedWmMapIsSaveable()
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmMapIsSaveable();
    }

    return gMapHeader.field_34 >= 0
        && gMapHeader.field_34 < kUnifiedFallout1MapCount
        && unifiedWmMapIdxIsSaveable(gMapHeader.field_34);
}

inline bool unifiedWmMapDeadBodiesAge()
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmMapDeadBodiesAge();
    }

    // Fallout 1's map_age_dead_critters runs for every saved-map reload. The
    // old world-map flag test is present in fallout1-ce but intentionally
    // commented out, so there is no per-map F2-style opt-out to emulate.
    return true;
}

inline bool unifiedWmMapCanRestHere(int elevation)
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmMapCanRestHere(elevation);
    }

    // Fallout 1 gates Pip-Boy resting through critter_can_obj_dude_rest rather
    // than worldmap.txt map flags. Returning true here preserves that original
    // division of responsibility while still rejecting nonsensical elevations.
    return elevation >= 0 && elevation < 3;
}

inline bool unifiedWmMapPipboyActive()
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmMapPipboyActive();
    }

    // Fallout 1 has no Fallout 2 VSUIT-movie gate for Pip-Boy availability.
    return true;
}

inline int unifiedWmMapMarkVisited(int mapIdx)
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmMapMarkVisited(mapIdx);
    }

    if (mapIdx < 0 || mapIdx >= kUnifiedFallout1MapCount) {
        return -1;
    }

    // Match Fallout 2's harmless behavior for temporary/random maps: there is
    // no persistent town state to update, but this is not an error.
    if (!unifiedWmMapIdxIsSaveable(mapIdx)) {
        return 0;
    }

    int areaIdx = unifiedFallout1MapTown(mapIdx);
    if (!unifiedFallout1TownIndexIsValid(areaIdx)) {
        return -1;
    }

    unifiedFallout1MarkTownKnown(areaIdx, true);
    return 0;
}

inline bool unifiedWmMapIsKnown(int mapIdx)
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmMapIsKnown(mapIdx);
    }

    if (mapIdx < 0 || mapIdx >= kUnifiedFallout1MapCount) {
        return false;
    }

    int areaIdx = unifiedFallout1MapTown(mapIdx);
    return unifiedFallout1TownIndexIsValid(areaIdx)
        && unifiedWmAreaIsKnown(areaIdx);
}

inline int unifiedWmMapMarkMapEntranceState(int mapIdx, int elevation, int state)
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmMapMarkMapEntranceState(mapIdx, elevation, state);
    }

    // Fallout 1 does not expose Fallout 2's METARULE3_MARK_MAP_ENTRANCE
    // contract. Its town entrance knowledge is derived from original global
    // variables 558-600. Until that GVAR synchronization layer runs, reject an
    // F2-only request safely instead of indexing wmAreaInfoList/wmMapInfoList.
    (void)mapIdx;
    (void)elevation;
    (void)state;
    return -1;
}

inline int unifiedProfileMapLoadByName(char* fileName)
{
    if (unifiedCampaignGetActiveGame() == UnifiedGameId::Fallout1
        && fileName != nullptr
        && compat_stricmp(fileName, "artemple.map") == 0) {
        // Fallout 2's shared main loop defaults a new game to artemple.map.
        // Original Fallout 1 starts at V13Ent.map after OVRINTRO. Keep the
        // executable main loop shared and translate only this impossible F1
        // map request at the call-site boundary.
        char fallout1StartMap[] = "V13Ent.map";
        return mapLoadByName(fallout1StartMap);
    }

    return mapLoadByName(fileName);
}

} // namespace fallout

#ifndef WORLD_MAP_H
#define wmMapIsSaveable unifiedWmMapIsSaveable
#define wmMapDeadBodiesAge unifiedWmMapDeadBodiesAge
#define wmMapCanRestHere unifiedWmMapCanRestHere
#define wmMapPipboyActive unifiedWmMapPipboyActive
#define wmMapMarkVisited unifiedWmMapMarkVisited
#define wmMapMarkMapEntranceState unifiedWmMapMarkMapEntranceState
#define wmMapIsKnown unifiedWmMapIsKnown
#endif

// main.cc enters through main.h; map.cc does not. Restrict the new-game map
// translation to the executable-side call sites so the stock mapLoadByName
// definition itself is never macro-renamed.
#if defined(MAIN_H)
#define mapLoadByName unifiedProfileMapLoadByName
#endif

#endif /* UNIFIED_LOADED_MAP_PROFILE_H */
