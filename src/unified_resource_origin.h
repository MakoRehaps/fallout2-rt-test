#ifndef UNIFIED_RESOURCE_ORIGIN_H
#define UNIFIED_RESOURCE_ORIGIN_H

#include "unified_campaign.h"

namespace fallout {

// The fused runtime keeps both Fallout data sets mounted. `activeGame` describes
// the origin of the map/script/proto world currently executing, while this
// optional override lets engine-level UI systems explicitly request resources
// from either original game without changing campaign state or remounting data.
inline bool gUnifiedResourceOriginOverrideActive = false;
inline UnifiedGameId gUnifiedResourceOriginOverride = UnifiedGameId::Fallout2;

inline UnifiedGameId unifiedResourceGetPreferredGame()
{
    if (gUnifiedResourceOriginOverrideActive) {
        return gUnifiedResourceOriginOverride;
    }

    return unifiedCampaignGetActiveGame();
}

inline UnifiedGameId unifiedResourceGetOtherGame(UnifiedGameId game)
{
    return game == UnifiedGameId::Fallout1
        ? UnifiedGameId::Fallout2
        : UnifiedGameId::Fallout1;
}

class UnifiedResourceOriginScope {
public:
    UnifiedResourceOriginScope(UnifiedGameId game, bool enabled = true)
        : _previousActive(gUnifiedResourceOriginOverrideActive)
        , _previousGame(gUnifiedResourceOriginOverride)
        , _enabled(enabled && unifiedCampaignIsEnabled())
    {
        if (_enabled) {
            gUnifiedResourceOriginOverrideActive = true;
            gUnifiedResourceOriginOverride = game;
        }
    }

    ~UnifiedResourceOriginScope()
    {
        if (_enabled) {
            gUnifiedResourceOriginOverrideActive = _previousActive;
            gUnifiedResourceOriginOverride = _previousGame;
        }
    }

    UnifiedResourceOriginScope(const UnifiedResourceOriginScope&) = delete;
    UnifiedResourceOriginScope& operator=(const UnifiedResourceOriginScope&) = delete;

private:
    bool _previousActive;
    UnifiedGameId _previousGame;
    bool _enabled;
};

} // namespace fallout

#endif /* UNIFIED_RESOURCE_ORIGIN_H */
