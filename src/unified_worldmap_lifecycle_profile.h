#ifndef UNIFIED_WORLDMAP_LIFECYCLE_PROFILE_H
#define UNIFIED_WORLDMAP_LIFECYCLE_PROFILE_H

#include "db.h"
#include "unified_campaign.h"
#include "unified_fallout1_encounter_bridge.h"
#include "unified_fallout1_worldmap_state.h"
#include "unified_worldmap_audio_profile.h"

namespace fallout {

int wmWorldMap_init();
void wmWorldMap_exit();
int wmWorldMap_reset();
int wmWorldMap_save(File* stream);
int wmWorldMap_load(File* stream);

inline int unifiedWmWorldMapInit()
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmWorldMap_init();
    }

    // F2CE normally parses Fallout 2's worldmap.txt here. Fallout 1 has no such
    // data model: its terrain, encounter and town tables are hard-coded. Start
    // the F1 profile with its original Vault 13 state and leave F2 tables alone.
    unifiedFallout1WorldMapClearPending();
    unifiedFallout1WorldMapConsumePreserveReset();
    unifiedFallout1MapMusicResetOverrides();
    unifiedFallout1EncounterBridgeReset();
    unifiedFallout1WorldMapResetCurrent();
    return 0;
}

inline void unifiedWmWorldMapExit()
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        wmWorldMap_exit();
        return;
    }

    // The F1 compatibility backend currently owns only inline/static state and
    // therefore has no F2 worldmap parser allocations to release.
    unifiedFallout1WorldMapClearPending();
    unifiedFallout1WorldMapConsumePreserveReset();
    unifiedFallout1MapMusicResetOverrides();
    unifiedFallout1EncounterBridgeReset();
}

inline int unifiedWmWorldMapReset()
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmWorldMap_reset();
    }

    // Runtime music overrides and forced encounters are transient and are not
    // part of Fallout 1's original world-map save payload.
    unifiedFallout1MapMusicResetOverrides();
    unifiedFallout1EncounterBridgeReset();

    if (unifiedFallout1WorldMapConsumePreserveReset()) {
        return 0;
    }

    // Ordinary gameReset means a new F1 game/runtime state. A load-game reset
    // sets the one-shot preserve flag before entering stock gameReset.
    unifiedFallout1WorldMapClearPending();
    unifiedFallout1WorldMapResetCurrent();
    return 0;
}

inline int unifiedWmWorldMapSave(File* stream)
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmWorldMap_save(stream);
    }

    // F1 world-map bytes live in the versioned COOPMETA.SAV FWM1 chunk. Writing
    // zero bytes here keeps the fixed F2CE handler list aligned with matching
    // unifiedWmWorldMapLoad saves without inventing an F2 worldmap payload.
    (void)stream;
    return 0;
}

inline int unifiedWmWorldMapLoad(File* stream)
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmWorldMap_load(stream);
    }

    // The sidecar was staged before _PrepLoad and applied around gameReset.
    // Unified F1 saves write no stock wmWorldMap payload, so consume no bytes.
    (void)stream;
    return 0;
}

} // namespace fallout

#ifndef WORLD_MAP_H
#define wmWorldMap_init unifiedWmWorldMapInit
#define wmWorldMap_exit unifiedWmWorldMapExit
#define wmWorldMap_reset unifiedWmWorldMapReset
#define wmWorldMap_save unifiedWmWorldMapSave
#define wmWorldMap_load unifiedWmWorldMapLoad
#endif

#endif /* UNIFIED_WORLDMAP_LIFECYCLE_PROFILE_H */
