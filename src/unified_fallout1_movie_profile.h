#ifndef UNIFIED_FALLOUT1_MOVIE_PROFILE_H
#define UNIFIED_FALLOUT1_MOVIE_PROFILE_H

#include <array>
#include <cstdio>
#include <cstring>

#include "color.h"
#include "cycle.h"
#include "db.h"
#include "game.h"
#include "game_mouse.h"
#include "game_movie.h"
#include "game_sound.h"
#include "input.h"
#include "mouse.h"
#include "movie.h"
#include "movie_effect.h"
#include "palette.h"
#include "settings.h"
#include "svga.h"
#include "text_font.h"
#include "touch.h"
#include "window_manager.h"

namespace fallout {

enum class UnifiedFallout1Movie : int {
    InterplayLogo = 0,
    MplayerLogo = 1,
    Intro = 2,
    VatsExplosion = 3,
    CathedralExplosion = 4,
    OverseerIntro = 5,
    Boil3 = 6,
    OverseerRun = 7,
    WalkMale = 8,
    WalkFemale = 9,
    Dipped = 10,
    Boil1 = 11,
    Boil2 = 12,
    RaeKills = 13,
    Count = 14,
};

inline constexpr const char* kUnifiedFallout1MovieFileNames[14] = {
    "iplogo.mve",
    "mplogo.mve",
    "intro.mve",
    "vexpld.mve",
    "cathexp.mve",
    "ovrintro.mve",
    "boil3.mve",
    "ovrrun.mve",
    "walkm.mve",
    "walkw.mve",
    "dipedv.mve",
    "boil1.mve",
    "boil2.mve",
    "raekills.mve",
};

inline std::array<unsigned char, 14> gUnifiedFallout1MoviesSeen {};
inline bool gUnifiedFallout1MoviePlaying = false;
inline char gUnifiedFallout1SubtitlePath[COMPAT_MAX_PATH] {};

inline bool unifiedFallout1MovieIndexIsValid(int movie)
{
    return movie >= 0 && movie < static_cast<int>(UnifiedFallout1Movie::Count);
}

inline char* unifiedFallout1MovieBuildSubtitlePath(const char* movieFilePath)
{
    const char* fileName = std::strrchr(movieFilePath, '\\');
    fileName = fileName != nullptr ? fileName + 1 : movieFilePath;

    std::snprintf(gUnifiedFallout1SubtitlePath,
        sizeof(gUnifiedFallout1SubtitlePath),
        "text\\%s\\cuts\\%s",
        settings.system.language.c_str(),
        fileName);

    char* extension = std::strrchr(gUnifiedFallout1SubtitlePath, '.');
    if (extension != nullptr) {
        *extension = '\0';
    }
    std::strncat(gUnifiedFallout1SubtitlePath,
        ".SVE",
        sizeof(gUnifiedFallout1SubtitlePath) - std::strlen(gUnifiedFallout1SubtitlePath) - 1);
    return gUnifiedFallout1SubtitlePath;
}

inline bool unifiedFallout1MovieIsSeen(UnifiedFallout1Movie movie)
{
    int index = static_cast<int>(movie);
    return unifiedFallout1MovieIndexIsValid(index) && gUnifiedFallout1MoviesSeen[index] != 0;
}

inline void unifiedFallout1MoviesReset()
{
    gUnifiedFallout1MoviesSeen.fill(0);
    gUnifiedFallout1MoviePlaying = false;
}

inline int unifiedFallout1MoviePlay(UnifiedFallout1Movie movie, int flags)
{
    int movieIndex = static_cast<int>(movie);
    if (!unifiedFallout1MovieIndexIsValid(movieIndex)) {
        return -1;
    }

    const char* movieFileName = kUnifiedFallout1MovieFileNames[movieIndex];
    const char* language = settings.system.language.c_str();
    char movieFilePath[COMPAT_MAX_PATH];
    int movieFileSize = 0;
    bool movieFound = false;

    if (compat_stricmp(language, ENGLISH) != 0) {
        std::snprintf(movieFilePath,
            sizeof(movieFilePath),
            "art\\%s\\cuts\\%s",
            language,
            movieFileName);
        movieFound = dbGetFileSize(movieFilePath, &movieFileSize) == 0;
    }

    if (!movieFound) {
        std::snprintf(movieFilePath, sizeof(movieFilePath), "art\\cuts\\%s", movieFileName);
        movieFound = dbGetFileSize(movieFilePath, &movieFileSize) == 0;
    }

    if (!movieFound) {
        return -1;
    }

    gUnifiedFallout1MoviePlaying = true;

    if ((flags & GAME_MOVIE_FADE_IN) != 0) {
        paletteFadeTo(gPaletteBlack);
    }

    int win = windowCreate(
        (screenGetWidth() - 640) / 2,
        (screenGetHeight() - 480) / 2,
        640,
        480,
        0,
        WINDOW_MODAL);
    if (win == -1) {
        gUnifiedFallout1MoviePlaying = false;
        return -1;
    }

    if ((flags & GAME_MOVIE_STOP_MUSIC) != 0) {
        backgroundSoundDelete();
    } else if ((flags & GAME_MOVIE_PAUSE_MUSIC) != 0) {
        backgroundSoundPause();
    }

    windowRefresh(win);

    bool forceSubtitles = movie == UnifiedFallout1Movie::Boil3
        || movie == UnifiedFallout1Movie::Boil1
        || movie == UnifiedFallout1Movie::Boil2;
    bool subtitlesEnabled = forceSubtitles || settings.preferences.subtitles;
    int movieFlags = 4;

    if (subtitlesEnabled) {
        char* subtitlePath = unifiedFallout1MovieBuildSubtitlePath(movieFilePath);
        int subtitleFileSize = 0;
        if (dbGetFileSize(subtitlePath, &subtitleFileSize) == 0) {
            movieFlags |= 0x8;
        } else {
            subtitlesEnabled = false;
        }
    }

    movieSetFlags(movieFlags);

    int oldTextColor = 0;
    int oldFont = 0;
    if (subtitlesEnabled) {
        colorPaletteLoad("art\\cuts\\subtitle.pal");
        oldTextColor = windowGetTextColor();
        windowSetTextColor(1.0f, 1.0f, 1.0f);
        oldFont = fontGetCurrent();
        windowSetFont(101);
    }

    bool cursorWasHidden = cursorIsHidden();
    if (cursorWasHidden) {
        gameMouseSetCursor(MOUSE_CURSOR_NONE);
        mouseShowCursor();
    }

    while (mouseGetEvent() != 0) {
        _mouse_info();
    }

    mouseHideCursor();
    colorCycleDisable();
    movieEffectsLoad(movieFilePath);
    _zero_vid_mem();
    _movieRun(win, movieFilePath);

    int accumulatedButtons = 0;
    int buttons = 0;
    do {
        if (!_moviePlaying() || _game_user_wants_to_quit || inputGetInput() != -1) {
            break;
        }

        Gesture gesture;
        if (touch_get_gesture(&gesture) && gesture.state == kEnded) {
            break;
        }

        int x;
        int y;
        _mouse_get_raw_state(&x, &y, &buttons);
        accumulatedButtons |= buttons;
    } while (((accumulatedButtons & 1) == 0 && (accumulatedButtons & 2) == 0)
        || (buttons & 1) != 0
        || (buttons & 2) != 0);

    _movieStop();
    _moviefx_stop();
    _movieUpdate();
    paletteSetEntries(gPaletteBlack);
    gUnifiedFallout1MoviesSeen[movieIndex] = 1;

    colorCycleEnable();
    gameMouseSetCursor(MOUSE_CURSOR_ARROW);
    if (!cursorWasHidden) {
        mouseShowCursor();
    }

    if (subtitlesEnabled) {
        colorPaletteLoad("color.pal");
        windowSetFont(oldFont);

        float r = static_cast<float>((Color2RGB(oldTextColor) & 0x7C00) >> 10) / 31.0f;
        float g = static_cast<float>((Color2RGB(oldTextColor) & 0x03E0) >> 5) / 31.0f;
        float b = static_cast<float>(Color2RGB(oldTextColor) & 0x001F) / 31.0f;
        windowSetTextColor(r, g, b);
    }

    windowDestroy(win);
    windowRefreshAll(&_scr_size);

    if ((flags & GAME_MOVIE_PAUSE_MUSIC) != 0) {
        backgroundSoundResume();
    }

    if ((flags & GAME_MOVIE_FADE_OUT) != 0) {
        if (!subtitlesEnabled) {
            colorPaletteLoad("color.pal");
        }
        paletteFadeTo(_cmap);
    }

    gUnifiedFallout1MoviePlaying = false;
    return 0;
}

} // namespace fallout

#endif /* UNIFIED_FALLOUT1_MOVIE_PROFILE_H */
