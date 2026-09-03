#ifndef LOCAL_COOP_ISO_CAMERAS_H
#define LOCAL_COOP_ISO_CAMERAS_H

#include <algorithm>
#include <array>

#include "draw.h"
#include "local_coop.h"
#include "local_coop_fps.h"
#include "map.h"
#include "svga.h"
#include "tile.h"
#include "window_manager.h"

namespace fallout {

// COOP_FOUR_INDEPENDENT_ISOMETRIC_CAMERAS_V1
// Render the authoritative Fallout ISO world once per active local player,
// centered on that player's actor, then composite the captures into independent
// viewports. This changes presentation only: there is still one map, one object
// list, one simulation and one set of scripts.
struct LocalCoopIsoViewport {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

inline int gLocalCoopIsoSplitWindow = -1;
inline int gLocalCoopIsoSplitWidth = 0;
inline int gLocalCoopIsoSplitHeight = 0;
inline Uint32 gLocalCoopIsoNextRefreshTick = 0;

inline int localCoopIsoActivePlayerCount()
{
    int count = 0;
    for (const LocalCoopPlayer& player : gLocalCoopPlayers) {
        if (player.connected && player.humanOwned && player.actor != nullptr
            && (player.actor->flags & OBJECT_HIDDEN) == 0) {
            count++;
        }
    }
    return count;
}

inline void localCoopIsoDestroySplitWindow()
{
    if (gLocalCoopIsoSplitWindow != -1) {
        windowDestroy(gLocalCoopIsoSplitWindow);
        gLocalCoopIsoSplitWindow = -1;
    }
    gLocalCoopIsoSplitWidth = 0;
    gLocalCoopIsoSplitHeight = 0;
}

inline LocalCoopIsoViewport localCoopIsoViewportForOrdinal(int ordinal, int playerCount, int width, int height)
{
    LocalCoopIsoViewport view;
    if (playerCount <= 1) {
        view = { 0, 0, width, height };
        return view;
    }

    if (playerCount == 2) {
        int half = width / 2;
        view.x = ordinal == 0 ? 0 : half;
        view.y = 0;
        view.width = ordinal == 0 ? half : width - half;
        view.height = height;
        return view;
    }

    int halfW = width / 2;
    int halfH = height / 2;
    view.x = (ordinal & 1) != 0 ? halfW : 0;
    view.y = ordinal >= 2 ? halfH : 0;
    view.width = (ordinal & 1) != 0 ? width - halfW : halfW;
    view.height = ordinal >= 2 ? height - halfH : halfH;
    return view;
}

inline void localCoopIsoDrawWaitingPanel(const LocalCoopIsoViewport& view, int slot)
{
    windowFill(gLocalCoopIsoSplitWindow, view.x, view.y, view.width, view.height, _colorTable[0]);
    char label[64];
    snprintf(label, sizeof(label), "P%d CAMERA", slot + 1);
    windowDrawText(gLocalCoopIsoSplitWindow, label, std::max(20, view.width - 16), view.x + 8, view.y + 8, _colorTable[992]);
}

inline bool localCoopIsoCapturePlayer(LocalCoopPlayer& player,
    unsigned char* splitBuffer,
    int splitPitch,
    const LocalCoopIsoViewport& view)
{
    if (player.actor == nullptr || !tileIsValid(player.actor->tile) || gIsoWindow == -1) return false;

    int centerRc = tileSetCenter(player.actor->tile,
        TILE_SET_CENTER_REFRESH_WINDOW | TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS);
    if (centerRc == -1) return false;

    unsigned char* isoBuffer = windowGetBuffer(gIsoWindow);
    int isoWidth = windowGetWidth(gIsoWindow);
    int isoHeight = windowGetHeight(gIsoWindow);
    if (isoBuffer == nullptr || isoWidth <= 0 || isoHeight <= 0) return false;

    int copyHeight = std::min(isoHeight, screenGetVisibleHeight());
    if (copyHeight <= 0) return false;

    // Scale the stock ISO framebuffer into this player's viewport. Original
    // Fallout art, lighting, roofs, critters, scenery and effects therefore all
    // remain exactly the engine's own render rather than a reconstructed scene.
    blitBufferToBufferStretch(isoBuffer,
        isoWidth,
        copyHeight,
        isoWidth,
        splitBuffer + view.y * splitPitch + view.x,
        view.width,
        view.height,
        splitPitch);

    char label[96];
    snprintf(label, sizeof(label), "P%d ISO  %s", player.slot + 1, player.slot == 0 ? "MAP LEADER" : "FOLLOWER");
    windowDrawText(gLocalCoopIsoSplitWindow, label, std::max(20, view.width - 16), view.x + 8, view.y + 8, _colorTable[992]);
    return true;
}

inline void localCoopIsoCamerasTick()
{
    // FPS owns the presentation while active.
    if (localCoopFpsActive() || gIsoWindow == -1 || isoIsDisabled()) {
        localCoopIsoDestroySplitWindow();
        return;
    }

    std::array<int, kLocalCoopMaxPlayers> activeSlots {};
    int playerCount = 0;
    for (int slot = 0; slot < kLocalCoopMaxPlayers; slot++) {
        const LocalCoopPlayer& player = gLocalCoopPlayers[slot];
        if (player.connected && player.humanOwned && player.actor != nullptr
            && player.uiMode == LocalCoopUiMode::World
            && (player.actor->flags & OBJECT_HIDDEN) == 0) {
            activeSlots[playerCount++] = slot;
        }
    }

    // Preserve the stock full-screen ISO renderer when playing alone.
    if (playerCount <= 1) {
        localCoopIsoDestroySplitWindow();
        if (playerCount == 1) {
            Object* actor = gLocalCoopPlayers[activeSlots[0]].actor;
            if (actor != nullptr && tileIsValid(actor->tile) && actor->tile != gCenterTile) {
                tileSetCenter(actor->tile,
                    TILE_SET_CENTER_REFRESH_WINDOW | TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS);
            }
        }
        return;
    }

    Uint32 now = SDL_GetTicks();
    if (static_cast<Sint32>(now - gLocalCoopIsoNextRefreshTick) < 0) return;
    gLocalCoopIsoNextRefreshTick = now + 33;

    int width = screenGetWidth();
    int height = screenGetVisibleHeight();
    if (width <= 0 || height <= 0) return;

    if (gLocalCoopIsoSplitWindow == -1
        || gLocalCoopIsoSplitWidth != width
        || gLocalCoopIsoSplitHeight != height) {
        localCoopIsoDestroySplitWindow();
        gLocalCoopIsoSplitWindow = windowCreate(0, 0, width, height, _colorTable[0], WINDOW_MOVE_ON_TOP);
        if (gLocalCoopIsoSplitWindow == -1) return;
        gLocalCoopIsoSplitWidth = width;
        gLocalCoopIsoSplitHeight = height;
    }

    unsigned char* splitBuffer = windowGetBuffer(gLocalCoopIsoSplitWindow);
    if (splitBuffer == nullptr) return;
    windowFill(gLocalCoopIsoSplitWindow, 0, 0, width, height, _colorTable[0]);

    int restoreTile = gCenterTile;
    // P1 remains the authoritative base camera center after captures; if P1 is
    // unavailable, restore the center that existed before the capture pass.
    if (gLocalCoopPlayers[0].actor != nullptr && tileIsValid(gLocalCoopPlayers[0].actor->tile)) {
        restoreTile = gLocalCoopPlayers[0].actor->tile;
    }

    for (int ordinal = 0; ordinal < playerCount; ordinal++) {
        int slot = activeSlots[ordinal];
        LocalCoopIsoViewport view = localCoopIsoViewportForOrdinal(ordinal, playerCount, width, height);
        if (!localCoopIsoCapturePlayer(gLocalCoopPlayers[slot], splitBuffer, width, view)) {
            localCoopIsoDrawWaitingPanel(view, slot);
        }
    }

    if (tileIsValid(restoreTile) && restoreTile != gCenterTile) {
        tileSetCenter(restoreTile,
            TILE_SET_CENTER_REFRESH_WINDOW | TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS);
    }

    if (playerCount == 2) {
        int half = width / 2;
        windowDrawLine(gLocalCoopIsoSplitWindow, half, 0, half, height - 1, _colorTable[992]);
    } else {
        int halfW = width / 2;
        int halfH = height / 2;
        windowDrawLine(gLocalCoopIsoSplitWindow, halfW, 0, halfW, height - 1, _colorTable[992]);
        windowDrawLine(gLocalCoopIsoSplitWindow, 0, halfH, width - 1, halfH, _colorTable[992]);
    }

    windowRefresh(gLocalCoopIsoSplitWindow);
}

} // namespace fallout

#endif
