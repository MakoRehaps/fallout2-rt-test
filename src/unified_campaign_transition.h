#ifndef UNIFIED_CAMPAIGN_TRANSITION_H
#define UNIFIED_CAMPAIGN_TRANSITION_H

#include "loadsave.h"
#include "unified_campaign.h"

namespace fallout {

inline bool gUnifiedCampaignAutoStartNewGame = false;
inline bool gUnifiedCampaignPostgameResumePending = false;

inline bool unifiedCampaignAdvanceToFallout2AndAutoStart()
{
    if (lsgSaveUnifiedCampaignCheckpoint(static_cast<int>(UnifiedGameId::Fallout1)) != 1) {
        return false;
    }

    if (!unifiedCampaignAdvanceToFallout2()) {
        return false;
    }

    gUnifiedCampaignAutoStartNewGame = true;
    return true;
}

inline bool unifiedCampaignRequestPostgameWorldSwitchAndResume()
{
    UnifiedGameId source = unifiedCampaignGetActiveGame();
    UnifiedGameId destination = source == UnifiedGameId::Fallout1
        ? UnifiedGameId::Fallout2
        : UnifiedGameId::Fallout1;

    if (lsgSaveUnifiedCampaignCheckpoint(static_cast<int>(source)) != 1) {
        return false;
    }

    // A completed world must already have a checkpoint: F1 gets one when Act I
    // advances to F2, and F2 gets one on its first postgame departure. Refuse to
    // silently create a fresh campaign if that persistent state is missing.
    if (!lsgUnifiedCampaignCheckpointExists(static_cast<int>(destination))) {
        return false;
    }

    if (!unifiedCampaignRequestOtherPostgameWorld()) {
        return false;
    }

    gUnifiedCampaignAutoStartNewGame = false;
    gUnifiedCampaignPostgameResumePending = true;
    return true;
}

inline bool unifiedCampaignConsumeAutoStartNewGame()
{
    if (!gUnifiedCampaignAutoStartNewGame) {
        return false;
    }

    gUnifiedCampaignAutoStartNewGame = false;
    return true;
}

inline bool unifiedCampaignConsumePostgameResume()
{
    if (!gUnifiedCampaignPostgameResumePending
        || !unifiedCampaignBothGamesCompleted()) {
        return false;
    }

    gUnifiedCampaignPostgameResumePending = false;
    return true;
}

inline void unifiedCampaignCancelAutoStartNewGame()
{
    gUnifiedCampaignAutoStartNewGame = false;
}

inline void unifiedCampaignCancelPostgameResume()
{
    gUnifiedCampaignPostgameResumePending = false;
}

} // namespace fallout

#endif /* UNIFIED_CAMPAIGN_TRANSITION_H */
