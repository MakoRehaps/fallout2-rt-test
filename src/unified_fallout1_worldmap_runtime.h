#ifndef UNIFIED_FALLOUT1_WORLDMAP_RUNTIME_H
#define UNIFIED_FALLOUT1_WORLDMAP_RUNTIME_H

#include "game.h"
#include "unified_campaign.h"
#include "unified_fallout1_travel_profile.h"
#include "unified_fallout1_worldmap_events.h"

namespace fallout {

// Fallout 1 runs CheckEvents from inside its world-map loop. Wrap only the
// routed F1 travel path so every game-time movement tick can fire the original
// water/death and destruction timers immediately instead of deferring them
// until the party reaches a destination.
inline bool unifiedFallout1TravelAdvanceTimeAndEvents(int ticks)
{
    if (!unifiedFallout1TravelAdvanceTime(ticks)) {
        return false;
    }

    if (unifiedFallout1ProcessWorldMapEvents() != 0) {
        return false;
    }

    // Once both destruction events are complete, the F1 runtime must stop the
    // current trip so control can perform the pending return-to-Vault-13 jump.
    return !gUnifiedFallout1ReturnToVault13Pending;
}

} // namespace fallout

// Route travel's time advancement through the F1 event-aware wrapper without
// changing the generic travel helper or Fallout 2's stock world-map path.
#define unifiedFallout1TravelAdvanceTime unifiedFallout1TravelAdvanceTimeAndEvents
#include "unified_fallout1_route_profile.h"
#undef unifiedFallout1TravelAdvanceTime

// Keep the existing visual layer intact while routing its single travel handoff
// through the exact-walkmask controller travel backend.
#define unifiedFallout1TravelToTown unifiedFallout1TravelToTownRouted
#include "unified_fallout1_worldmap_visual.h"
#undef unifiedFallout1TravelToTown

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

    // The routed travel loop already checks events after each game-time step.
    // Recheck once when control returns as a defensive boundary for cancelled
    // travel and any state changed while an encounter/town map was staged.
    if (unifiedFallout1ProcessWorldMapEvents() != 0) {
        return;
    }
    unifiedFallout1WorldMapHandlePendingVaultReturn();
}

} // namespace fallout

#endif /* UNIFIED_FALLOUT1_WORLDMAP_RUNTIME_H */
