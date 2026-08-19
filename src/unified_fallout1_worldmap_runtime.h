#ifndef UNIFIED_FALLOUT1_WORLDMAP_RUNTIME_H
#define UNIFIED_FALLOUT1_WORLDMAP_RUNTIME_H

#include "game.h"
#include "unified_campaign.h"
#include "unified_fallout1_travel_profile.h"
#include "unified_fallout1_worldmap_events.h"
#include "unified_fallout1_worldmap_visual.h"
#include "unified_worldmap_state_profile.h"

namespace fallout {

inline bool unifiedFallout1WorldMapHandlePendingVaultReturn()
{
    if (!unifiedFallout1ConsumeReturnToVault13()) {
        return false;
    }

    unifiedWmTeleportToArea(0);
    unifiedFallout1LoadMapName("V13ENT.MAP", 0);
    return true;
}

inline void unifiedWmWorldMapRuntime()
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        unifiedWmWorldMapVisual();
        return;
    }

    ScopedGameMode worldMapMode(GameMode::kWorldmap);

    if (unifiedFallout1ProcessWorldMapEvents() != 0) {
        return;
    }
    if (unifiedFallout1WorldMapHandlePendingVaultReturn()) {
        return;
    }

    unifiedWmWorldMapVisual();

    // Travel advances game time in large world-map chunks. Re-evaluate the
    // original F1 timers when control returns from the travel loop so VATS or
    // Cathedral destruction cannot remain deferred until some unrelated map
    // load. A later pass can move this check into each travel-day tick for exact
    // mid-route interruption timing.
    if (unifiedFallout1ProcessWorldMapEvents() != 0) {
        return;
    }
    unifiedFallout1WorldMapHandlePendingVaultReturn();
}

} // namespace fallout

#endif /* UNIFIED_FALLOUT1_WORLDMAP_RUNTIME_H */
