#ifndef UNIFIED_FALLOUT1_TOWNMAP_VISUAL_H
#define UNIFIED_FALLOUT1_TOWNMAP_VISUAL_H

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
#include "window_manager.h"

namespace fallout {

inline constexpr int kUnifiedFallout1TownMapFirstArtId = 156;
inline constexpr int kUnifiedFallout1TownMapBoxArtId = 136;
inline constexpr int kUnifiedFallout1TownMapLabelsArtId = 137;
inline constexpr int kUnifiedFallout1TownMapWindowWidth = 640;
inline constexpr int kUnifiedFallout1TownMapWindowHeight = 480;
inline constexpr int kUnifiedFallout1TownMapImageX = 20;
inline constexpr int kUnifiedFallout1TownMapImageY = 20;
inline constexpr int kUnifiedFallout1TownMapImageWidth = 453;
inline constexpr int kUnifiedFallout1TownMapImageHeight = 444;

struct UnifiedFallout1TownHotspotPosition {
    short x;
    short y;
};

// Exact x/y coordinates from Fallout 1 TownHotSpots. The entry order matches
// kUnifiedFallout1TownEntrances in unified_fallout1_travel_profile.h.
inline constexpr UnifiedFallout1TownHotspotPosition kUnifiedFallout1TownHotspotPositions[12][6] = {
    { { 202, 303 }, { 271, 282 }, { 292, 237 }, { 309, 204 }, { 0, 0 }, { 0, 0 } },
    { { 68, 250 }, { 107, 209 }, { 298, 187 }, { 135, 290 }, { 0, 0 }, { 0, 0 } },
    { { 158, 192 }, { 270, 253 }, { 314, 217 }, { 0, 0 }, { 0, 0 }, { 0, 0 } },
    { { 400, 317 }, { 304, 257 }, { 200, 279 }, { 0, 0 }, { 0, 0 }, { 0, 0 } },
    { { 241, 398 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } },
    { { 398, 265 }, { 239, 224 }, { 79, 207 }, { 0, 0 }, { 0, 0 }, { 0, 0 } },
    { { 238, 78 }, { 205, 172 }, { 128, 138 }, { 306, 137 }, { 272, 238 }, { 125, 216 } },
    { { 172, 167 }, { 254, 194 }, { 136, 263 }, { 280, 306 }, { 161, 373 }, { 0, 0 } },
    { { 197, 83 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } },
    { { 340, 149 }, { 334, 195 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } },
    { { 276, 239 }, { 229, 195 }, { 179, 185 }, { 346, 114 }, { 285, 159 }, { 0, 0 } },
    { { 86, 328 }, { 229, 313 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } },
};

inline void unifiedFallout1TownMapDrawHotspot(
    int win,
    int town,
    int entrance,
    bool selected)
{
    if (!unifiedFallout1TownIndexIsValid(town) || entrance < 0 || entrance >= 6) {
        return;
    }

    const UnifiedFallout1TownHotspotPosition& hotspot =
        kUnifiedFallout1TownHotspotPositions[town][entrance];
    if (hotspot.x == 0 || hotspot.y == 0) {
        return;
    }

    int radius = selected ? 6 : 3;
    int color = _colorTable[992];
    windowDrawRect(
        win,
        hotspot.x - radius,
        hotspot.y - radius,
        hotspot.x + radius,
        hotspot.y + radius,
        color);
    if (selected) {
        windowDrawLine(win, hotspot.x - 9, hotspot.y, hotspot.x + 9, hotspot.y, color);
        windowDrawLine(win, hotspot.x, hotspot.y - 9, hotspot.x, hotspot.y + 9, color);
    }
}

inline bool unifiedFallout1TownMapRender(
    int win,
    FrmImage& townImage,
    FrmImage& boxImage,
    FrmImage& labelsImage,
    int town,
    const std::array<int, 6>& entrances,
    int entranceCount,
    int selected)
{
    if (win == -1 || !townImage.isLocked()) {
        return false;
    }

    if (townImage.getWidth() < kUnifiedFallout1TownMapImageWidth
        || townImage.getHeight() < kUnifiedFallout1TownMapImageHeight) {
        return false;
    }

    unsigned char* buffer = windowGetBuffer(win);
    int pitch = windowGetWidth(win);
    if (buffer == nullptr || pitch < kUnifiedFallout1TownMapWindowWidth) {
        return false;
    }

    windowFill(
        win,
        0,
        0,
        kUnifiedFallout1TownMapWindowWidth,
        kUnifiedFallout1TownMapWindowHeight,
        _colorTable[0]);

    blitBufferToBuffer(
        townImage.getData(),
        kUnifiedFallout1TownMapImageWidth,
        kUnifiedFallout1TownMapImageHeight,
        townImage.getWidth(),
        buffer + kUnifiedFallout1TownMapImageY * pitch + kUnifiedFallout1TownMapImageX,
        pitch);

    // Fallout 1 overlays these two original interface FRMs over every town map.
    if (boxImage.isLocked()
        && boxImage.getWidth() >= kUnifiedFallout1TownMapWindowWidth
        && boxImage.getHeight() >= kUnifiedFallout1TownMapWindowHeight) {
        blitBufferToBufferTrans(
            boxImage.getData(),
            kUnifiedFallout1TownMapWindowWidth,
            kUnifiedFallout1TownMapWindowHeight,
            boxImage.getWidth(),
            buffer,
            pitch);
    }
    if (labelsImage.isLocked()
        && labelsImage.getWidth() >= kUnifiedFallout1TownMapWindowWidth
        && labelsImage.getHeight() >= kUnifiedFallout1TownMapWindowHeight) {
        blitBufferToBufferTrans(
            labelsImage.getData(),
            kUnifiedFallout1TownMapWindowWidth,
            kUnifiedFallout1TownMapWindowHeight,
            labelsImage.getWidth(),
            buffer,
            pitch);
    }

    for (int index = 0; index < entranceCount; index++) {
        unifiedFallout1TownMapDrawHotspot(
            win,
            town,
            entrances[index],
            index == selected);
    }

    char townName[40] = {};
    unifiedWmGetAreaIdxName(town, townName);
    windowDrawText(
        win,
        townName[0] != '\0' ? townName : "FALLOUT TOWN",
        136,
        490,
        26,
        _colorTable[992]);
    windowDrawText(win, "D-PAD SELECT", 136, 490, 52, _colorTable[992]);
    windowDrawText(win, "A ENTER", 136, 490, 70, _colorTable[992]);
    windowDrawText(win, "B WORLD MAP", 136, 490, 88, _colorTable[992]);

    int entrance = entrances[selected];
    const UnifiedFallout1TownEntrance& entry = kUnifiedFallout1TownEntrances[town][entrance];
    char selection[96];
    std::snprintf(selection, sizeof(selection), "ENTRANCE %d", entrance + 1);
    windowDrawText(win, selection, 136, 490, 396, _colorTable[992]);
    windowDrawText(win, entry.mapName, 136, 490, 416, _colorTable[992]);

    windowRefresh(win);
    return true;
}

inline int unifiedFallout1SelectAndLoadTownEntranceVisual(int town)
{
    if (!unifiedFallout1TownIndexIsValid(town)) {
        return -1;
    }

    // Preserve the direct destroyed-site crater substitutions from the base F1
    // town-entry backend. Those states intentionally bypass normal selection.
    if ((town == 11 && unifiedFallout1TravelGetGlobal(kUnifiedFallout1MasterBlownGvar) != 0)
        || (town == 8 && unifiedFallout1TravelGetGlobal(kUnifiedFallout1VatsBlownGvar) != 0)) {
        return unifiedFallout1SelectAndLoadTownEntrance(town);
    }

    std::array<int, 6> entrances {};
    int entranceCount = 0;
    for (int entrance = 0; entrance < 6; entrance++) {
        if (unifiedFallout1TownEntranceKnown(town, entrance)) {
            entrances[entranceCount++] = entrance;
        }
    }

    if (entranceCount == 0 && kUnifiedFallout1TownEntrances[town][0].mapName != nullptr) {
        entrances[entranceCount++] = 0;
    }
    if (entranceCount == 0) {
        return -1;
    }

    FrmImage townImage;
    FrmImage boxImage;
    FrmImage labelsImage;
    int townFid = buildFid(
        OBJ_TYPE_INTERFACE,
        kUnifiedFallout1TownMapFirstArtId + town,
        0,
        0,
        0);
    int boxFid = buildFid(OBJ_TYPE_INTERFACE, kUnifiedFallout1TownMapBoxArtId, 0, 0, 0);
    int labelsFid = buildFid(OBJ_TYPE_INTERFACE, kUnifiedFallout1TownMapLabelsArtId, 0, 0, 0);

    if (!townImage.lock(townFid)
        || screenGetWidth() < kUnifiedFallout1TownMapWindowWidth
        || screenGetVisibleHeight() < kUnifiedFallout1TownMapWindowHeight) {
        return unifiedFallout1SelectAndLoadTownEntrance(town);
    }

    // BOX/LABELS are visual polish rather than functional requirements; use
    // them when the owned F1 installation exposes the expected original FRMs.
    boxImage.lock(boxFid);
    labelsImage.lock(labelsFid);

    int win = windowCreate(
        (screenGetWidth() - kUnifiedFallout1TownMapWindowWidth) / 2,
        (screenGetVisibleHeight() - kUnifiedFallout1TownMapWindowHeight) / 2,
        kUnifiedFallout1TownMapWindowWidth,
        kUnifiedFallout1TownMapWindowHeight,
        _colorTable[0],
        WINDOW_MOVE_ON_TOP);
    if (win == -1) {
        return unifiedFallout1SelectAndLoadTownEntrance(town);
    }

    int selected = 0;
    UnifiedFallout1WorldMapState& state = unifiedFallout1WorldMapGetState();
    for (int index = 0; index < entranceCount; index++) {
        if (entrances[index] == state.currentSection) {
            selected = index;
            break;
        }
    }

    UnifiedFallout1PadEdges previous {};
    bool dirty = true;
    while (true) {
        if (dirty) {
            if (!unifiedFallout1TownMapRender(
                    win,
                    townImage,
                    boxImage,
                    labelsImage,
                    town,
                    entrances,
                    entranceCount,
                    selected)) {
                windowDestroy(win);
                return unifiedFallout1SelectAndLoadTownEntrance(town);
            }
            dirty = false;
        }

        int key = inputGetInput();
        UnifiedFallout1PadEdges edges = unifiedFallout1TravelReadPadEdges(previous);
        if (key == KEY_ARROW_UP || key == KEY_ARROW_LEFT || edges.up || edges.left) {
            selected = (selected + entranceCount - 1) % entranceCount;
            dirty = true;
        } else if (key == KEY_ARROW_DOWN || key == KEY_ARROW_RIGHT || edges.down || edges.right) {
            selected = (selected + 1) % entranceCount;
            dirty = true;
        } else if (key == KEY_RETURN || edges.accept) {
            int entrance = entrances[selected];
            const UnifiedFallout1TownEntrance entry = kUnifiedFallout1TownEntrances[town][entrance];
            state.currentTown = town;
            state.currentSection = entrance;
            unifiedFallout1MarkTownKnown(town, true);
            windowDestroy(win);
            return unifiedFallout1LoadMapName(entry.mapName, entry.loadMapIndex);
        } else if (key == KEY_ESCAPE || edges.cancel) {
            windowDestroy(win);
            return 0;
        }

        SDL_Delay(8);
    }
}

inline void unifiedWmTownMapVisual()
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        wmTownMap();
        return;
    }

    UnifiedFallout1WorldMapState& state = unifiedFallout1WorldMapGetState();
    int town = state.currentTown;
    if (!unifiedFallout1TownIndexIsValid(town)) {
        town = unifiedFallout1TownAtWorldPos(state.worldX, state.worldY);
    }

    if (unifiedFallout1TownIndexIsValid(town)) {
        unifiedFallout1SelectAndLoadTownEntranceVisual(town);
    }
}

} // namespace fallout

#endif /* UNIFIED_FALLOUT1_TOWNMAP_VISUAL_H */
