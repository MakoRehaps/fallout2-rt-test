#ifndef GAME_MOVIE_H
#define GAME_MOVIE_H

#include "db.h"

namespace fallout {

typedef enum GameMovieFlags {
    GAME_MOVIE_FADE_IN = 0x01,
    GAME_MOVIE_FADE_OUT = 0x02,
    GAME_MOVIE_STOP_MUSIC = 0x04,
    GAME_MOVIE_PAUSE_MUSIC = 0x08,
} GameMovieFlags;

typedef enum GameMovie {
    MOVIE_IPLOGO,
    MOVIE_INTRO,
    MOVIE_ELDER,
    MOVIE_VSUIT,
    MOVIE_AFAILED,
    MOVIE_ADESTROY,
    MOVIE_CAR,
    MOVIE_CARTUCCI,
    MOVIE_TIMEOUT,
    MOVIE_TANKER,
    MOVIE_ENCLAVE,
    MOVIE_DERRICK,
    MOVIE_ARTIMER1,
    MOVIE_ARTIMER2,
    MOVIE_ARTIMER3,
    MOVIE_ARTIMER4,
    MOVIE_CREDITS,
    MOVIE_COUNT,
} GameMovie;

int gameMoviesInit();
void gameMoviesReset();
int gameMoviesLoad(File* stream);
int gameMoviesSave(File* stream);
int gameMoviePlay(int movie, int flags);
void gameMovieFadeOut();
bool gameMovieIsSeen(int movie);
bool gameMovieIsPlaying();

} // namespace fallout

// proto.cc chooses the unarmored player critter from the Fallout 2 native-look
// state: before MOVIE_VSUIT it selects the tribal model, afterwards the jumpsuit.
// Fallout 1 never uses that Fallout 2 Temple-of-Trials transition; its player is
// already the vault-jumpsuit character. Treat MOVIE_VSUIT as seen only for the
// proto translation unit while an F1-origin world is active. This keeps the F1
// critter LST on hmjmps/hfjmps instead of falling through to the missing F2
// tribal name and index 0 (the visible fire-guy fallback).
#if defined(PROTO_H)
#include "unified_campaign.h"

namespace fallout {

inline bool unifiedProtoGameMovieIsSeen(int movie)
{
    if (unifiedCampaignIsEnabled()
        && unifiedCampaignGetActiveGame() == UnifiedGameId::Fallout1
        && movie == MOVIE_VSUIT) {
        return true;
    }

    return gameMovieIsSeen(movie);
}

} // namespace fallout

#define gameMovieIsSeen unifiedProtoGameMovieIsSeen
#endif

// main.cc includes main.h before its own game_movie.h include. Route only that
// executable translation unit through the profile-aware startup movie table;
// game_movie.cc includes this header directly and therefore retains the stock
// implementation and Fallout 2 movie table unchanged.
#if defined(MAIN_H)
#include "unified_main_movie_profile.h"
#define gameMoviePlay unifiedMainGameMoviePlay
#endif

#endif /* GAME_MOVIE_H */