#ifndef UNIFIED_FALLOUT1_WORLDMAP_EVENTS_H
#define UNIFIED_FALLOUT1_WORLDMAP_EVENTS_H

#include <algorithm>

#include "game.h"
#include "game_movie.h"
#include "stat.h"
#include "unified_campaign.h"
#include "unified_fallout1_movie_profile.h"
#include "unified_fallout1_worldmap_globals.h"

namespace fallout {

int wmCheckGameAreaEvents();

inline constexpr int kUnifiedFallout1VaultWaterGvar = 10;
inline constexpr int kUnifiedFallout1FindWaterChipGvar = 101;
inline constexpr int kUnifiedFallout1CountdownToDestructionGvar = 55;
inline constexpr int kUnifiedFallout1VatsCountdownGvar = 147;
inline constexpr int kUnifiedFallout1PlayerReputationGvar = 155;
inline constexpr int kUnifiedFallout1DestroyMaster4Gvar = 308;
inline constexpr int kUnifiedFallout1DestroyMaster5Gvar = 309;

inline bool gUnifiedFallout1ReturnToVault13Pending = false;

inline void unifiedFallout1WriteGlobal(int index, int value)
{
    if (gGameGlobalVars != nullptr && index >= 0 && index < gGameGlobalVarsLength) {
        gGameGlobalVars[index] = value;
    }
}

inline void unifiedFallout1AdjustReputation(int amount)
{
    int value = unifiedFallout1ReadGlobal(kUnifiedFallout1PlayerReputationGvar) + amount;
    unifiedFallout1WriteGlobal(
        kUnifiedFallout1PlayerReputationGvar,
        std::max(-100, std::min(value, 100)));
}

inline bool unifiedFallout1EventCountdownExpired(int gvar)
{
    int startedAt = unifiedFallout1ReadGlobal(gvar);
    if (startedAt == 0) {
        return false;
    }

    // Original F1 CheckEvents compares (game_time - countdown) / 10 > 240.
    return static_cast<int>((gameTimeGetTime() - static_cast<unsigned int>(startedAt)) / 10) > 240;
}

inline int unifiedFallout1ProcessWorldMapEvents()
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return 0;
    }

    // Vault water timeout: original F1 plays boil3 and ends the game unless the
    // water-chip quest is already in completion state 2.
    if (unifiedFallout1ReadGlobal(kUnifiedFallout1VaultWaterGvar) == 0
        && unifiedFallout1ReadGlobal(kUnifiedFallout1FindWaterChipGvar) != 2) {
        unifiedFallout1MoviePlay(
            UnifiedFallout1Movie::Boil3,
            GAME_MOVIE_FADE_IN | GAME_MOVIE_FADE_OUT | GAME_MOVIE_PAUSE_MUSIC);
        _game_user_wants_to_quit = 1;
        return 1;
    }

    if (unifiedFallout1EventCountdownExpired(kUnifiedFallout1VatsCountdownGvar)) {
        unifiedFallout1MoviePlay(
            UnifiedFallout1Movie::VatsExplosion,
            GAME_MOVIE_FADE_IN | GAME_MOVIE_FADE_OUT | GAME_MOVIE_PAUSE_MUSIC);

        unifiedFallout1WriteGlobal(kUnifiedFallout1DestroyMaster4Gvar, 2);
        unifiedFallout1WriteGlobal(kUnifiedFallout1VatsCountdownGvar, 0);
        unifiedFallout1WriteGlobal(kUnifiedFallout1VatsBlownGvar, 1);
        pcAddExperience(10000);
        unifiedFallout1AdjustReputation(5);

        if (unifiedFallout1ReadGlobal(kUnifiedFallout1MasterBlownGvar) != 0) {
            gUnifiedFallout1ReturnToVault13Pending = true;
        }
    }

    if (unifiedFallout1EventCountdownExpired(kUnifiedFallout1CountdownToDestructionGvar)) {
        unifiedFallout1MoviePlay(
            UnifiedFallout1Movie::CathedralExplosion,
            GAME_MOVIE_FADE_IN | GAME_MOVIE_FADE_OUT | GAME_MOVIE_PAUSE_MUSIC);

        unifiedFallout1WriteGlobal(kUnifiedFallout1DestroyMaster5Gvar, 2);
        unifiedFallout1WriteGlobal(kUnifiedFallout1CountdownToDestructionGvar, 0);
        unifiedFallout1WriteGlobal(kUnifiedFallout1MasterBlownGvar, 1);
        pcAddExperience(10000);
        unifiedFallout1AdjustReputation(10);

        if (unifiedFallout1ReadGlobal(kUnifiedFallout1VatsBlownGvar) != 0) {
            gUnifiedFallout1ReturnToVault13Pending = true;
        }
    }

    return 0;
}

inline bool unifiedFallout1ConsumeReturnToVault13()
{
    bool pending = gUnifiedFallout1ReturnToVault13Pending;
    gUnifiedFallout1ReturnToVault13Pending = false;
    return pending;
}

inline int unifiedWmCheckGameAreaEvents()
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmCheckGameAreaEvents();
    }

    // F2CE invokes this from mapLoad, but Fallout 1's CheckEvents belongs to its
    // world-map loop. Keep ordinary F1 map loads inert; the dedicated F1
    // world-map runtime calls unifiedFallout1ProcessWorldMapEvents directly.
    return 0;
}

} // namespace fallout

#ifndef WORLD_MAP_H
#define wmCheckGameAreaEvents unifiedWmCheckGameAreaEvents
#endif

#endif /* UNIFIED_FALLOUT1_WORLDMAP_EVENTS_H */
