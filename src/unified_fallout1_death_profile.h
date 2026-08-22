#ifndef UNIFIED_FALLOUT1_DEATH_PROFILE_H
#define UNIFIED_FALLOUT1_DEATH_PROFILE_H

#include "random.h"
#include "unified_campaign.h"

namespace fallout {

// Stock Fallout 2 endgame entry points. endgame.h declares these before this
// profile is included, and the call-site macros are installed afterwards.
void endgameSetupDeathEnding(int reason);
char* endgameDeathEndingGetFileName();

inline void unifiedFallout1SetupDeathEnding(int reason)
{
    if (unifiedCampaignGetActiveGame() == UnifiedGameId::Fallout1) {
        // Fallout 1 does not build a Fallout 2 death-ending candidate table
        // when the player dies. Its main loop simply opens DEATH.FRM and picks
        // one of narrator nar_3 through nar_6.
        (void)reason;
        return;
    }

    endgameSetupDeathEnding(reason);
}

inline char* unifiedFallout1DeathEndingGetFileName()
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return endgameDeathEndingGetFileName();
    }

    // Exact original Fallout 1 death-screen narrator set from main_death_scene.
    static char deathFileNames[4][32] = {
        "narrator\\nar_3",
        "narrator\\nar_4",
        "narrator\\nar_5",
        "narrator\\nar_6",
    };

    return deathFileNames[randomBetween(0, 3)];
}

} // namespace fallout

#endif /* UNIFIED_FALLOUT1_DEATH_PROFILE_H */
