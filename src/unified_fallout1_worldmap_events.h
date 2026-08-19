#ifndef UNIFIED_FALLOUT1_WORLDMAP_EVENTS_H
#define UNIFIED_FALLOUT1_WORLDMAP_EVENTS_H

#include "unified_campaign.h"

namespace fallout {

int wmCheckGameAreaEvents();

inline int unifiedWmCheckGameAreaEvents()
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmCheckGameAreaEvents();
    }

    // Fallout 2's implementation reads wmGenData.currentAreaId and mutates its
    // fake Vault 13 city records. Those tables intentionally do not exist while
    // the Fallout 1 profile is active. Fallout 1's own timed water/VATS/Master
    // events are handled by the F1 event backend; until its movie-file adapter
    // is installed there is deliberately no fallback into F2 area data here.
    return 0;
}

} // namespace fallout

#ifndef WORLD_MAP_H
#define wmCheckGameAreaEvents unifiedWmCheckGameAreaEvents
#endif

#endif /* UNIFIED_FALLOUT1_WORLDMAP_EVENTS_H */
