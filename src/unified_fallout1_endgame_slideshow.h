#ifndef UNIFIED_FALLOUT1_ENDGAME_SLIDESHOW_H
#define UNIFIED_FALLOUT1_ENDGAME_SLIDESHOW_H

#include <climits>
#include <cmath>
#include <cstring>

#include "art.h"
#include "color.h"
#include "cycle.h"
#include "draw.h"
#include "game.h"
#include "game_mouse.h"
#include "game_sound.h"
#include "input.h"
#include "map.h"
#include "mouse.h"
#include "object.h"
#include "palette.h"
#include "svga.h"
#include "text_font.h"
#include "window_manager.h"

namespace fallout {

inline constexpr int kUnifiedFallout1EndingWidth = 640;
inline constexpr int kUnifiedFallout1EndingHeight = 480;

inline int gUnifiedFallout1EndingWindow = -1;
inline int gUnifiedFallout1EndingOverlay = -1;
inline unsigned char* gUnifiedFallout1EndingBuffer = nullptr;
inline bool gUnifiedFallout1EndingSpeechEnded = false;
inline bool gUnifiedFallout1EndingMapWasEnabled = false;
inline bool gUnifiedFallout1EndingCursorWasVisible = false;
inline int gUnifiedFallout1EndingOldFont = 0;

inline void unifiedFallout1EndingSpeechCallback()
{
    gUnifiedFallout1EndingSpeechEnded = true;
}

inline void unifiedFallout1EndingDrainMouse()
{
    while (mouseGetEvent() != 0) {
        sharedFpsLimiter.mark();
        inputGetInput();
        renderPresent();
        sharedFpsLimiter.throttle();
    }
}

inline void unifiedFallout1EndingLoadPalette(int artId)
{
    char fileName[13];
    if (artCopyFileName(OBJ_TYPE_INTERFACE, artId, fileName) != 0) {
        return;
    }

    char* extension = std::strrchr(fileName, '.');
    if (extension != nullptr) {
        *extension = '\0';
    }

    if (std::strlen(fileName) <= 8) {
        char path[COMPAT_MAX_PATH];
        std::snprintf(path, sizeof(path), "art\\intrface\\%s.pal", fileName);
        colorPaletteLoad(path);
    }
}

inline bool unifiedFallout1EndingInit()
{
    backgroundSoundDelete();
    gUnifiedFallout1EndingMapWasEnabled = isoDisable();
    colorCycleDisable();
    gameMouseSetCursor(MOUSE_CURSOR_NONE);

    bool cursorWasHidden = cursorIsHidden();
    gUnifiedFallout1EndingCursorWasVisible = !cursorWasHidden;
    if (cursorWasHidden) {
        mouseShowCursor();
    }

    gUnifiedFallout1EndingOldFont = fontGetCurrent();
    fontSetCurrent(101);
    paletteFadeTo(gPaletteBlack);

    gUnifiedFallout1EndingOverlay = windowCreate(
        0,
        0,
        screenGetWidth(),
        screenGetHeight(),
        _colorTable[0],
        WINDOW_MOVE_ON_TOP);
    if (gUnifiedFallout1EndingOverlay == -1) {
        return false;
    }

    gUnifiedFallout1EndingWindow = windowCreate(
        (screenGetWidth() - kUnifiedFallout1EndingWidth) / 2,
        (screenGetHeight() - kUnifiedFallout1EndingHeight) / 2,
        kUnifiedFallout1EndingWidth,
        kUnifiedFallout1EndingHeight,
        _colorTable[0],
        WINDOW_MOVE_ON_TOP);
    if (gUnifiedFallout1EndingWindow == -1) {
        windowDestroy(gUnifiedFallout1EndingOverlay);
        gUnifiedFallout1EndingOverlay = -1;
        return false;
    }

    gUnifiedFallout1EndingBuffer = windowGetBuffer(gUnifiedFallout1EndingWindow);
    if (gUnifiedFallout1EndingBuffer == nullptr) {
        windowDestroy(gUnifiedFallout1EndingWindow);
        windowDestroy(gUnifiedFallout1EndingOverlay);
        gUnifiedFallout1EndingWindow = -1;
        gUnifiedFallout1EndingOverlay = -1;
        return false;
    }

    speechSetEndCallback(unifiedFallout1EndingSpeechCallback);
    return true;
}

inline void unifiedFallout1EndingFree()
{
    speechDelete();
    speechSetEndCallback(nullptr);

    if (gUnifiedFallout1EndingWindow != -1) {
        windowDestroy(gUnifiedFallout1EndingWindow);
    }
    if (gUnifiedFallout1EndingOverlay != -1) {
        windowDestroy(gUnifiedFallout1EndingOverlay);
    }

    gUnifiedFallout1EndingWindow = -1;
    gUnifiedFallout1EndingOverlay = -1;
    gUnifiedFallout1EndingBuffer = nullptr;

    fontSetCurrent(gUnifiedFallout1EndingOldFont);
    if (!gUnifiedFallout1EndingCursorWasVisible) {
        mouseHideCursor();
    }
    gameMouseSetCursor(MOUSE_CURSOR_ARROW);

    colorPaletteLoad("color.pal");
    paletteFadeTo(_cmap);
    colorCycleEnable();
    if (gUnifiedFallout1EndingMapWasEnabled) {
        isoEnable();
    }
}

inline bool unifiedFallout1EndingStartSpeech(const char* narratorFileName)
{
    speechDelete();
    gUnifiedFallout1EndingSpeechEnded = false;

    char path[COMPAT_MAX_PATH];
    std::snprintf(path, sizeof(path), "narrator\\%s", narratorFileName);
    if (speechLoad(path, 10, 14, 15) == -1) {
        return false;
    }

    _gsound_speech_play_preloaded();
    return true;
}

inline void unifiedFallout1EndingRenderStatic(int artId, const char* narratorFileName)
{
    int fid = buildFid(OBJ_TYPE_INTERFACE, artId, 0, 0, 0);
    CacheEntry* handle = nullptr;
    Art* art = artLock(fid, &handle);
    if (art == nullptr) {
        return;
    }

    unsigned char* data = artGetFrameData(art, 0, 0);
    if (data == nullptr) {
        artUnlock(handle);
        return;
    }

    blitBufferToBuffer(
        data,
        kUnifiedFallout1EndingWidth,
        kUnifiedFallout1EndingHeight,
        kUnifiedFallout1EndingWidth,
        gUnifiedFallout1EndingBuffer,
        kUnifiedFallout1EndingWidth);
    windowRefresh(gUnifiedFallout1EndingWindow);

    unifiedFallout1EndingLoadPalette(artId);
    paletteFadeTo(_cmap);

    bool speechLoaded = unifiedFallout1EndingStartSpeech(narratorFileName);
    inputPauseForTocks(500);

    unsigned int started = getTicks();
    unsigned int fallbackDelay = 3000;
    tickersDisable();
    while (true) {
        sharedFpsLimiter.mark();

        if (inputGetInput() != -1) {
            break;
        }
        if (speechLoaded && gUnifiedFallout1EndingSpeechEnded) {
            break;
        }
        if (!speechLoaded && getTicksSince(started) >= fallbackDelay) {
            break;
        }

        soundContinueAll();
        renderPresent();
        sharedFpsLimiter.throttle();
    }
    tickersEnable();

    speechDelete();
    paletteFadeTo(gPaletteBlack);
    bufferFill(
        gUnifiedFallout1EndingBuffer,
        kUnifiedFallout1EndingWidth,
        kUnifiedFallout1EndingHeight,
        kUnifiedFallout1EndingWidth,
        _colorTable[0]);
    windowRefresh(gUnifiedFallout1EndingWindow);
    artUnlock(handle);
    unifiedFallout1EndingDrainMouse();
}

inline void unifiedFallout1EndingRenderPan(int direction, const char* narratorFileName)
{
    int fid = buildFid(OBJ_TYPE_INTERFACE, 327, 0, 0, 0);
    CacheEntry* handle = nullptr;
    Art* art = artLock(fid, &handle);
    if (art == nullptr) {
        return;
    }

    int width = artGetWidth(art, 0, 0);
    unsigned char* data = artGetFrameData(art, 0, 0);
    if (data == nullptr || width <= kUnifiedFallout1EndingWidth) {
        artUnlock(handle);
        return;
    }

    unifiedFallout1EndingLoadPalette(327);
    unsigned char fullPalette[768];
    std::memcpy(fullPalette, _cmap, sizeof(fullPalette));
    paletteSetEntries(gPaletteBlack);

    int distance = width - kUnifiedFallout1EndingWidth;
    int quarter = distance / 4;
    int start = direction < 0 ? distance : 0;
    int end = direction < 0 ? 0 : distance;
    int voiceStart = direction < 0 ? kUnifiedFallout1EndingWidth - quarter : quarter;
    bool voiceStarted = false;
    unsigned int frameTick = 0;

    tickersDisable();
    while (start != end) {
        sharedFpsLimiter.mark();

        if (getTicksSince(frameTick) >= 16) {
            blitBufferToBuffer(
                data + start,
                kUnifiedFallout1EndingWidth,
                kUnifiedFallout1EndingHeight,
                width,
                gUnifiedFallout1EndingBuffer,
                kUnifiedFallout1EndingWidth);
            windowRefresh(gUnifiedFallout1EndingWindow);
            frameTick = getTicks();

            if (!voiceStarted
                && ((direction > 0 && start >= voiceStart)
                    || (direction < 0 && start <= voiceStart))) {
                unifiedFallout1EndingStartSpeech(narratorFileName);
                voiceStarted = true;
            }

            double fade = 1.0;
            if (start < quarter) {
                fade = static_cast<double>(start) / static_cast<double>(quarter == 0 ? 1 : quarter);
            } else if (start > distance - quarter) {
                fade = static_cast<double>(distance - start) / static_cast<double>(quarter == 0 ? 1 : quarter);
            }

            if (fade < 1.0) {
                unsigned char darkPalette[768];
                for (int index = 0; index < 768; index++) {
                    darkPalette[index] = static_cast<unsigned char>(std::trunc(fullPalette[index] * fade));
                }
                paletteSetEntries(darkPalette);
            } else {
                paletteSetEntries(fullPalette);
            }

            start += direction;
        }

        soundContinueAll();
        if (inputGetInput() != -1) {
            break;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }
    tickersEnable();

    speechDelete();
    paletteFadeTo(gPaletteBlack);
    bufferFill(
        gUnifiedFallout1EndingBuffer,
        kUnifiedFallout1EndingWidth,
        kUnifiedFallout1EndingHeight,
        kUnifiedFallout1EndingWidth,
        _colorTable[0]);
    windowRefresh(gUnifiedFallout1EndingWindow);
    artUnlock(handle);
    unifiedFallout1EndingDrainMouse();
}

inline int unifiedFallout1EndingGlobal(int index)
{
    if (index < 0 || index >= gGameGlobalVarsLength) {
        return 0;
    }
    return gameGetGlobalVar(index);
}

inline void unifiedFallout1EndgamePlaySlideshowImpl()
{
    // Numeric indices are Fallout 1's original global-variable ABI. They are
    // deliberately local to this F1-only renderer so Fallout 2's game_vars.h
    // enum remains untouched.
    constexpr int GVAR_FOLLOWERS_INVADED_F1 = 7;
    constexpr int GVAR_SHADY_SANDS_INVADED_F1 = 12;
    constexpr int GVAR_NECROPOLIS_INVADED_F1 = 13;
    constexpr int GVAR_HUB_INVADED_F1 = 14;
    constexpr int GVAR_JUNKTOWN_INVADED_F1 = 15;
    constexpr int GVAR_TANDI_STATUS_F1 = 26;
    constexpr int GVAR_NECROP_WATER_CHIP_TAKEN_F1 = 30;
    constexpr int GVAR_NECROP_WATER_PUMP_FIXED_F1 = 31;
    constexpr int GVAR_KILLIAN_DEAD_F1 = 37;
    constexpr int GVAR_GIZMO_DEAD_F1 = 38;
    constexpr int GVAR_VATS_STATUS_F1 = 51;
    constexpr int GVAR_RAIDERS_F1 = 69;
    constexpr int GVAR_CAPTURE_GIZMO_F1 = 104;
    constexpr int GVAR_BECOME_AN_INITIATE_F1 = 108;
    constexpr int GVAR_GARL_DEAD_F1 = 114;
    constexpr int GVAR_TOTAL_RAIDERS_F1 = 115;
    constexpr int GVAR_TRAIN_FOLLOWERS_F1 = 132;
    constexpr int GVAR_ENEMY_BROTHERHOOD_F1 = 250;
    constexpr int GVAR_CALM_REBELS_2_F1 = 299;
    constexpr int GVAR_ARADESH_STATUS_F1 = 604;
    constexpr int GVAR_KIND_TO_HAROLD_F1 = 606;

    if (!unifiedFallout1EndingInit()) {
        return;
    }

    unifiedFallout1EndingRenderPan(
        1,
        unifiedFallout1EndingGlobal(GVAR_VATS_STATUS_F1) != 0 ? "nar_11" : "nar_10");

    if (unifiedFallout1EndingGlobal(GVAR_NECROPOLIS_INVADED_F1) != 0) {
        unifiedFallout1EndingRenderStatic(311, "nar_15");
    } else if (unifiedFallout1EndingGlobal(GVAR_NECROP_WATER_CHIP_TAKEN_F1) != 0) {
        if (unifiedFallout1EndingGlobal(GVAR_NECROP_WATER_PUMP_FIXED_F1) == 2) {
            unifiedFallout1EndingRenderStatic(312, "nar_13");
        } else {
            unifiedFallout1EndingRenderStatic(311, "nar_12");
        }
    }

    if (unifiedFallout1EndingGlobal(GVAR_FOLLOWERS_INVADED_F1) != 0) {
        unifiedFallout1EndingRenderStatic(314, "nar_18");
    } else if (unifiedFallout1EndingGlobal(GVAR_TRAIN_FOLLOWERS_F1) != 0) {
        unifiedFallout1EndingRenderStatic(313, "nar_16");
    }

    if (unifiedFallout1EndingGlobal(GVAR_SHADY_SANDS_INVADED_F1) != 0) {
        unifiedFallout1EndingRenderStatic(324, "nar_23");
    } else {
        int tandiStatus = unifiedFallout1EndingGlobal(GVAR_TANDI_STATUS_F1);
        if (unifiedFallout1EndingGlobal(GVAR_ARADESH_STATUS_F1) != 0) {
            if (tandiStatus != 2 && tandiStatus != 0) {
                unifiedFallout1EndingRenderStatic(324, "nar_22");
            } else {
                unifiedFallout1EndingRenderStatic(323, "nar_21");
            }
        } else if (tandiStatus != 2 && tandiStatus != 0) {
            unifiedFallout1EndingRenderStatic(323, "nar_20");
        } else {
            unifiedFallout1EndingRenderStatic(323, "nar_19");
        }
    }

    if (unifiedFallout1EndingGlobal(GVAR_JUNKTOWN_INVADED_F1) != 0) {
        unifiedFallout1EndingRenderStatic(317, "nar_27");
    } else if (unifiedFallout1EndingGlobal(GVAR_CAPTURE_GIZMO_F1) != 2
        || unifiedFallout1EndingGlobal(GVAR_KILLIAN_DEAD_F1) != 0) {
        if (unifiedFallout1EndingGlobal(GVAR_GIZMO_DEAD_F1) == 0) {
            unifiedFallout1EndingRenderStatic(316, "nar_25");
        }
    } else {
        unifiedFallout1EndingRenderStatic(315, "nar_24");
    }

    if (unifiedFallout1EndingGlobal(GVAR_BECOME_AN_INITIATE_F1) == 2
        && unifiedFallout1EndingGlobal(GVAR_ENEMY_BROTHERHOOD_F1) != 0) {
        unifiedFallout1EndingRenderStatic(319, "nar_29");
    } else {
        unifiedFallout1EndingRenderStatic(318, "nar_28");
    }

    if (unifiedFallout1EndingGlobal(GVAR_HUB_INVADED_F1) != 0) {
        unifiedFallout1EndingRenderStatic(326, "nar_34");
    } else if (unifiedFallout1EndingGlobal(GVAR_KIND_TO_HAROLD_F1) == 1) {
        unifiedFallout1EndingRenderStatic(325, "nar_32");
    }

    if (unifiedFallout1EndingGlobal(GVAR_RAIDERS_F1) < 2) {
        unifiedFallout1EndingRenderStatic(320, "nar_37");
    } else {
        int totalRaiders = unifiedFallout1EndingGlobal(GVAR_TOTAL_RAIDERS_F1);
        if ((unifiedFallout1EndingGlobal(GVAR_GARL_DEAD_F1) != 0 && totalRaiders < 8)
            || totalRaiders < 4) {
            unifiedFallout1EndingRenderStatic(320, "nar_35");
        } else {
            unifiedFallout1EndingRenderStatic(320, "nar_36");
        }
    }

    unifiedFallout1EndingRenderPan(-1, "nar_40");
    unifiedFallout1EndingFree();

    if (GVAR_CALM_REBELS_2_F1 < gGameGlobalVarsLength) {
        gameSetGlobalVar(GVAR_CALM_REBELS_2_F1, 0);
    }
}

} // namespace fallout

#endif /* UNIFIED_FALLOUT1_ENDGAME_SLIDESHOW_H */
