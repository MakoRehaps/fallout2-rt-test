#ifndef UNIFIED_FALLOUT1_ENDGAME_PROFILE_H
#define UNIFIED_FALLOUT1_ENDGAME_PROFILE_H

#include "credits.h"
#include "game.h"
#include "game_movie.h"
#include "object.h"
#include "proto_types.h"
#include "stat.h"
#include "unified_campaign.h"
#include "unified_campaign_transition.h"
#include "unified_fallout1_movie_profile.h"

namespace fallout {

// Stock Fallout 2 entry points. endgame.h declares these before including this
// profile, and the call-site macros are installed only after this header has
// finished, so these calls remain true stock fallbacks.
void endgamePlaySlideshow();
void endgamePlayMovie();

inline bool gUnifiedFallout1EndingSlideshowRequested = false;
inline bool gUnifiedFallout1EndingMoviePlayed = false;

inline void unifiedFallout1EndgameResetTransient()
{
    gUnifiedFallout1EndingSlideshowRequested = false;
    gUnifiedFallout1EndingMoviePlayed = false;
}

inline void unifiedFallout1EndgamePlaySlideshow()
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        endgamePlaySlideshow();
        return;
    }

    // Fallout 1 and Fallout 2 use different ending GVARs, slide art and narrator
    // rules. Never feed F1 state into Fallout 2's slideshow selector. Record the
    // request so the ending sequence remains ordered, then let the F1 movie/
    // campaign handoff proceed. A source-faithful F1 slide renderer can be
    // layered here without changing the script ABI or touching F2 endgame.cc.
    gUnifiedFallout1EndingSlideshowRequested = true;
}

inline void unifiedFallout1EndgamePlayMovie()
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        endgamePlayMovie();
        return;
    }

    if (gUnifiedFallout1EndingMoviePlayed) {
        return;
    }
    gUnifiedFallout1EndingMoviePlayed = true;

    // Original Fallout 1 endgame_movie selects WALKM/WALKW from the player's
    // gender, then shows credits.txt. The movie player is filename-based, so F1
    // cannot accidentally hit Fallout 2's incompatible movie enum.
    UnifiedFallout1Movie endingMovie = UnifiedFallout1Movie::WalkMale;
    if (gDude != nullptr && critterGetStat(gDude, STAT_GENDER) == GENDER_FEMALE) {
        endingMovie = UnifiedFallout1Movie::WalkFemale;
    }

    unifiedFallout1MoviePlay(endingMovie, 0);
    creditsOpen("credits.txt", -1, false);

    if (unifiedCampaignIsEnabled()) {
        // Do not switch cwd/databases under live F1 scripts. The existing main
        // loop sees this request, exits through the normal teardown path, then
        // performs a full Fallout 2 rebootstrap. The transition flag makes the
        // fresh F2 main-menu event call choose NEW GAME exactly once so Act II
        // enters the stock character-selector/startup path automatically.
        // This source path is compiler-verified; real F1/F2 data still requires
        // runtime validation before the transition can be called stable.
        if (unifiedCampaignAdvanceToFallout2AndAutoStart()) {
            _game_user_wants_to_quit = 2;
            return;
        }
    }

    // Standalone --fallout1 keeps the original semantic of ending the game.
    _game_user_wants_to_quit = 2;
}

} // namespace fallout

#endif /* UNIFIED_FALLOUT1_ENDGAME_PROFILE_H */
