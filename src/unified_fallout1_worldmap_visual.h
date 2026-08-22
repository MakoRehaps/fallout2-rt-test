#ifndef UNIFIED_FALLOUT1_WORLDMAP_VISUAL_H
#define UNIFIED_FALLOUT1_WORLDMAP_VISUAL_H

#include <algorithm>
#include <array>
#include <cstdio>

#include "art.h"
#include "color.h"
#include "draw.h"
#include "input.h"
#include "kb.h"
#include "proto_types.h"
#include "svga.h"
#include "unified_campaign.h"
#include "unified_fallout1_travel_profile.h"
#include "unified_fallout1_worldmap_state.h"
#include "unified_worldmap_state_profile.h"
#include "window_manager.h"

namespace fallout {

inline constexpr int kUnifiedFallout1WorldMapArtId = 135;
inline constexpr int kUnifiedFallout1WorldMapWindowWidth = 640;
inline constexpr int kUnifiedFallout1WorldMapWindowHeight = 480;
inline constexpr int kUnifiedFallout1WorldMapViewportX = 20;
inline constexpr int kUnifiedFallout1WorldMapViewportY = 20;
inline constexpr int kUnifiedFallout1WorldMapViewportWidth = 450;
inline constexpr int kUnifiedFallout1WorldMapViewportHeight = 442;

struct UnifiedFallout1WorldMapViewport {
    int x;
    int y;
};

inline UnifiedFallout1WorldMapViewport unifiedFallout1WorldMapViewportFor(
    int worldX,
    int worldY,
    int imageWidth,
    int imageHeight)
{
    UnifiedFallout1WorldMapViewport viewport {};
    viewport.x = worldX - kUnifiedFallout1WorldMapViewportWidth / 2;
    viewport.y = worldY - kUnifiedFallout1WorldMapViewportHeight / 2;
    viewport.x = std::max(0, std::min(viewport.x, std::max(0, imageWidth - kUnifiedFallout1WorldMapViewportWidth)));
    viewport.y = std::max(0, std::min(viewport.y, std::max(0, imageHeight - kUnifiedFallout1WorldMapViewportHeight)));
    return viewport;
}

inline void unifiedFallout1WorldMapFogRect(
    unsigned char* buffer,
    int pitch,
    int left,
    int top,
    int width,
    int height,
    int state)
{
    if (buffer == nullptr || width <= 0 || height <= 0) {
        return;
    }

    if (state == 0) {
        bufferFill(buffer + top * pitch + left, width, height, pitch, _colorTable[0]);
        return;
    }

    if (state == 1) {
        // Fallout 1 renders partially explored cells darkened. Use the stock
        // palette's black entry in a 50% checker without introducing any new
        // art or assuming F2's world-map blend tables were initialized.
        unsigned char black = _colorTable[0];
        for (int y = 0; y < height; y++) {
            unsigned char* row = buffer + (top + y) * pitch + left;
            for (int x = (y & 1); x < width; x += 2) {
                row[x] = black;
            }
        }
    }
}

inline void unifiedFallout1WorldMapApplyFog(
    int win,
    const UnifiedFallout1WorldMapViewport& viewport)
{
    unsigned char* buffer = windowGetBuffer(win);
    int pitch = windowGetWidth(win);
    if (buffer == nullptr || pitch <= 0) {
        return;
    }

    const UnifiedFallout1WorldMapState& state = unifiedFallout1WorldMapGetStateConst();

    int firstColumn = std::max(0, viewport.x / 50);
    int lastColumn = std::min(kUnifiedFallout1WorldMapColumns - 1,
        (viewport.x + kUnifiedFallout1WorldMapViewportWidth - 1) / 50);
    int firstRow = std::max(0, viewport.y / 50);
    int lastRow = std::min(kUnifiedFallout1WorldMapRows - 1,
        (viewport.y + kUnifiedFallout1WorldMapViewportHeight - 1) / 50);

    for (int row = firstRow; row <= lastRow; row++) {
        for (int column = firstColumn; column <= lastColumn; column++) {
            int worldLeft = column * 50;
            int worldTop = row * 50;
            int worldRight = worldLeft + 50;
            int worldBottom = worldTop + 50;

            int clippedLeft = std::max(worldLeft, viewport.x);
            int clippedTop = std::max(worldTop, viewport.y);
            int clippedRight = std::min(worldRight, viewport.x + kUnifiedFallout1WorldMapViewportWidth);
            int clippedBottom = std::min(worldBottom, viewport.y + kUnifiedFallout1WorldMapViewportHeight);
            if (clippedRight <= clippedLeft || clippedBottom <= clippedTop) {
                continue;
            }

            int left = kUnifiedFallout1WorldMapViewportX + clippedLeft - viewport.x;
            int top = kUnifiedFallout1WorldMapViewportY + clippedTop - viewport.y;
            unifiedFallout1WorldMapFogRect(
                buffer,
                pitch,
                left,
                top,
                clippedRight - clippedLeft,
                clippedBottom - clippedTop,
                state.worldGrid[row][column]);
        }
    }
}

inline void unifiedFallout1WorldMapDrawMarker(
    int win,
    int worldX,
    int worldY,
    const UnifiedFallout1WorldMapViewport& viewport,
    bool selected)
{
    int x = kUnifiedFallout1WorldMapViewportX + worldX - viewport.x;
    int y = kUnifiedFallout1WorldMapViewportY + worldY - viewport.y;
    if (x < kUnifiedFallout1WorldMapViewportX
        || x >= kUnifiedFallout1WorldMapViewportX + kUnifiedFallout1WorldMapViewportWidth
        || y < kUnifiedFallout1WorldMapViewportY
        || y >= kUnifiedFallout1WorldMapViewportY + kUnifiedFallout1WorldMapViewportHeight) {
        return;
    }

    int radius = selected ? 5 : 2;
    int color = _colorTable[992];
    windowDrawRect(win, x - radius, y - radius, x + radius, y + radius, color);
    if (selected) {
        windowDrawLine(win, x - 7, y, x + 7, y, color);
        windowDrawLine(win, x, y - 7, x, y + 7, color);
    }
}

inline bool unifiedFallout1WorldMapRender(
    int win,
    FrmImage& worldMapImage,
    int selectedTown)
{
    if (win == -1 || !worldMapImage.isLocked()) {
        return false;
    }

    int imageWidth = worldMapImage.getWidth();
    int imageHeight = worldMapImage.getHeight();
    if (imageWidth < kUnifiedFallout1WorldMapViewportWidth
        || imageHeight < kUnifiedFallout1WorldMapViewportHeight) {
        return false;
    }

    unsigned char* buffer = windowGetBuffer(win);
    int pitch = windowGetWidth(win);
    if (buffer == nullptr || pitch < kUnifiedFallout1WorldMapWindowWidth) {
        return false;
    }

    const UnifiedFallout1WorldMapState& state = unifiedFallout1WorldMapGetStateConst();
    UnifiedFallout1WorldMapViewport viewport = unifiedFallout1WorldMapViewportFor(
        state.worldX,
        state.worldY,
        imageWidth,
        imageHeight);

    windowFill(win, 0, 0, kUnifiedFallout1WorldMapWindowWidth, kUnifiedFallout1WorldMapWindowHeight, _colorTable[0]);
    blitBufferToBuffer(
        worldMapImage.getData() + viewport.y * imageWidth + viewport.x,
        kUnifiedFallout1WorldMapViewportWidth,
        kUnifiedFallout1WorldMapViewportHeight,
        imageWidth,
        buffer + kUnifiedFallout1WorldMapViewportY * pitch + kUnifiedFallout1WorldMapViewportX,
        pitch);

    unifiedFallout1WorldMapApplyFog(win, viewport);

    // Mark all known Fallout 1 locations, then emphasize the selected town and
    // the party's exact persisted world position.
    for (int town = 0; town < kUnifiedFallout1TownCountForState; town++) {
        if (unifiedWmAreaIsKnown(town)) {
            unifiedFallout1WorldMapDrawMarker(
                win,
                unifiedFallout1TownWorldX(town),
                unifiedFallout1TownWorldY(town),
                viewport,
                town == selectedTown);
        }
    }
    unifiedFallout1WorldMapDrawMarker(win, state.worldX, state.worldY, viewport, true);

    windowDrawRect(
        win,
        kUnifiedFallout1WorldMapViewportX - 1,
        kUnifiedFallout1WorldMapViewportY - 1,
        kUnifiedFallout1WorldMapViewportX + kUnifiedFallout1WorldMapViewportWidth,
        kUnifiedFallout1WorldMapViewportY + kUnifiedFallout1WorldMapViewportHeight,
        _colorTable[992]);

    windowDrawText(win, "DESTINATIONS", 138, 490, 24, _colorTable[992]);
    windowDrawText(win, "D-PAD  SELECT", 138, 490, 44, _colorTable[992]);
    windowDrawText(win, "A      TRAVEL", 138, 490, 60, _colorTable[992]);
    windowDrawText(win, "B      BACK", 138, 490, 76, _colorTable[992]);

    int line = 0;
    for (int town = 0; town < kUnifiedFallout1TownCountForState; town++) {
        if (!unifiedWmAreaIsKnown(town)) {
            continue;
        }

        char name[40] = {};
        unifiedWmGetAreaIdxName(town, name);
        char text[48];
        std::snprintf(text, sizeof(text), "%c%s", town == selectedTown ? '>' : ' ', name[0] != '\0' ? name : "UNKNOWN");
        windowDrawText(win, text, 138, 490, 108 + line * 22, _colorTable[992]);
        line++;
    }

    char position[64];
    std::snprintf(position, sizeof(position), "X:%d Y:%d", state.worldX, state.worldY);
    windowDrawText(win, position, 138, 490, 446, _colorTable[992]);
    windowRefresh(win);
    return true;
}

inline int unifiedFallout1SelectKnownTownVisual()
{
    unifiedFallout1WorldMapSyncFromGlobals();

    std::array<int, kUnifiedFallout1TownCountForState> towns {};
    int count = 0;
    for (int town = 0; town < kUnifiedFallout1TownCountForState; town++) {
        if (unifiedWmAreaIsKnown(town)) {
            towns[count++] = town;
        }
    }
    if (count == 0) {
        return -1;
    }

    FrmImage worldMapImage;
    int fid = buildFid(OBJ_TYPE_INTERFACE, kUnifiedFallout1WorldMapArtId, 0, 0, 0);
    if (!worldMapImage.lock(fid)
        || screenGetWidth() < kUnifiedFallout1WorldMapWindowWidth
        || screenGetVisibleHeight() < kUnifiedFallout1WorldMapWindowHeight) {
        return unifiedFallout1SelectKnownTown();
    }

    int win = windowCreate(
        (screenGetWidth() - kUnifiedFallout1WorldMapWindowWidth) / 2,
        (screenGetVisibleHeight() - kUnifiedFallout1WorldMapWindowHeight) / 2,
        kUnifiedFallout1WorldMapWindowWidth,
        kUnifiedFallout1WorldMapWindowHeight,
        _colorTable[0],
        WINDOW_MOVE_ON_TOP);
    if (win == -1) {
        return unifiedFallout1SelectKnownTown();
    }

    int selected = 0;
    int currentTown = unifiedFallout1WorldMapGetStateConst().currentTown;
    for (int index = 0; index < count; index++) {
        if (towns[index] == currentTown) {
            selected = index;
            break;
        }
    }

    UnifiedFallout1PadEdges previous {};
    bool dirty = true;
    while (true) {
        if (dirty) {
            unifiedFallout1WorldMapRender(win, worldMapImage, towns[selected]);
            dirty = false;
        }

        int key = inputGetInput();
        UnifiedFallout1PadEdges edges = unifiedFallout1TravelReadPadEdges(previous);
        if (key == KEY_ARROW_UP || key == KEY_ARROW_LEFT || edges.up || edges.left) {
            selected = (selected + count - 1) % count;
            dirty = true;
        } else if (key == KEY_ARROW_DOWN || key == KEY_ARROW_RIGHT || edges.down || edges.right) {
            selected = (selected + 1) % count;
            dirty = true;
        } else if (key == KEY_RETURN || edges.accept) {
            int town = towns[selected];
            windowDestroy(win);
            return town;
        } else if (key == KEY_ESCAPE || edges.cancel) {
            windowDestroy(win);
            return -1;
        }

        SDL_Delay(8);
    }
}

inline void unifiedWmWorldMapVisual()
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        unifiedWmWorldMap();
        return;
    }

    int destination = unifiedFallout1SelectKnownTownVisual();
    if (destination == -1) {
        return;
    }

    UnifiedFallout1WorldMapState& state = unifiedFallout1WorldMapGetState();
    if (unifiedFallout1TownAtWorldPos(state.worldX, state.worldY) == destination) {
        state.currentTown = destination;
        unifiedFallout1SelectAndLoadTownEntrance(destination);
        return;
    }

    unifiedFallout1TravelToTown(destination);
}

} // namespace fallout

#endif /* UNIFIED_FALLOUT1_WORLDMAP_VISUAL_H */
