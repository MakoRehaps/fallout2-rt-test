#ifndef UNIFIED_CAMPAIGN_TRANSITION_H
#define UNIFIED_CAMPAIGN_TRANSITION_H

#include "unified_campaign.h"

namespace fallout {

inline bool gUnifiedCampaignAutoStartNewGame = false;
inline bool gUnifiedCampaignPostgameResumePending = false;

inline bool unifiedCampaignAdvanceToFallout2AndAutoStart()
{
    if (!unifiedCampaignAdvanceToFallout2()) {
        return false;
    }

    gUnifiedCampaignAutoStartNewGame = true;
    return true;
}

inline bool unifiedCampaignRequestPostgameWorldSwitchAndResume()
{
    if (!unifiedCampaignRequestOtherPostgameWorld()) {
        return false;
    }

    gUnifiedCampaignPostgameResumePending = true;
    return true;
}

inline bool unifiedCampaignConsumeAutoStartNewGame()
{
    if (!gUnifiedCampaignAutoStartNewGame
        || unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout2) {
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
