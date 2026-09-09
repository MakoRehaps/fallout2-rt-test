#ifndef UNIFIED_MAIN_MOVIE_PROFILE_H
#define UNIFIED_MAIN_MOVIE_PROFILE_H

#include "unified_campaign.h"
#include "unified_fallout1_movie_profile.h"

namespace fallout {

// game_movie.h declares this stock symbol before including the profile.
int gameMoviePlay(int movie, int flags);

inline int unifiedMainGameMoviePlay(int movie, int flags)
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return gameMoviePlay(movie, flags);
    }

    switch (movie) {
    case MOVIE_IPLOGO:
        return unifiedFallout1MoviePlay(UnifiedFallout1Movie::InterplayLogo, flags);
    case MOVIE_INTRO:
        return unifiedFallout1MoviePlay(UnifiedFallout1Movie::Intro, flags);
    case MOVIE_ELDER:
        // main.cc uses Fallout 2's Elder movie after character selection. The
        // equivalent original Fallout 1 new-game movie is Overseer Intro.
        return unifiedFallout1MoviePlay(UnifiedFallout1Movie::OverseerIntro, flags);
    case MOVIE_CREDITS:
        // F2's startup sequence has a separate credits movie after intro; F1's
        // original main loop does not. Ending credits still use credits.txt.
        return 0;
    default:
        // No other stock F2 movie IDs are valid F1 main-menu semantics. Refuse
        // rather than accidentally selecting an unrelated F1 file by index.
        return -1;
    }
}

} // namespace fallout

#endif /* UNIFIED_MAIN_MOVIE_PROFILE_H */
