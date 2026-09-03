#ifndef UNIFIED_WORLDMAP_STATE_PROFILE_H
#define UNIFIED_WORLDMAP_STATE_PROFILE_H

#include <cstring>

#include "message.h"
#include "unified_campaign.h"
#include "unified_fallout1_worldmap_globals.h"
#include "unified_fallout1_worldmap_state.h"

namespace fallout {

// Stock Fallout 2 CE symbols. The remap macros are installed only after these
// wrappers are defined, so fallback calls continue to target worldmap.cc.
int wmGetPartyWorldPos(int* xPtr, int* yPtr);
int wmGetPartyCurArea(int* areaIdxPtr);
void wmSetPartyWorldPos(int x, int y);
int wmGetAreaIdxName(int areaIdx, char* name);
bool wmAreaIsKnown(int areaIdx);
int wmAreaVisitedState(int areaIdx);
int wmAreaMarkVisited(int areaIdx);
bool wmAreaMarkVisitedState(int areaIdx, int state);
bool wmAreaSetVisibleState(int areaIdx, int state, bool force);
int wmAreaSetWorldPos(int areaIdx, int x, int y);
int wmTeleportToArea(int areaIdx);

extern MessageList gMapMessageList;

inline constexpr int kUnifiedFallout1WorldCellSize = 50;
inline constexpr int kUnifiedFallout1TownCountForState = 12;

struct UnifiedFallout1TownLocation {
    int column;
    int row;
};

// Original fallout1-ce city_location table, in TOWN_* enum order.
inline constexpr UnifiedFallout1TownLocation kUnifiedFallout1TownLocations[kUnifiedFallout1TownCountForState] = {
    { 16, 1 }, // Vault 13
    { 25, 1 }, // Vault 15
    { 21, 1 }, // Shady Sands
    { 17, 10 }, // Junktown
    { 22, 3 }, // Raiders
    { 22, 13 }, // Necropolis
    { 17, 14 }, // The Hub
    { 12, 9 }, // Brotherhood
    { 3, 1 }, // Military Base
    { 24, 25 }, // The Glow
    { 15, 18 }, // Boneyard
    { 15, 20 }, // Cathedral
};

inline bool unifiedFallout1TownIndexIsValid(int areaIdx)
{
    return areaIdx >= 0 && areaIdx < kUnifiedFallout1TownCountForState;
}

inline int unifiedFallout1TownWorldX(int areaIdx)
{
    return kUnifiedFallout1TownLocations[areaIdx].column * kUnifiedFallout1WorldCellSize
        + kUnifiedFallout1WorldCellSize / 2;
}

inline int unifiedFallout1TownWorldY(int areaIdx)
{
    return kUnifiedFallout1TownLocations[areaIdx].row * kUnifiedFallout1WorldCellSize
        + kUnifiedFallout1WorldCellSize / 2;
}

inline int unifiedFallout1TownAtWorldPos(int x, int y)
{
    if (x < 0 || y < 0) {
        return -1;
    }

    int column = x / kUnifiedFallout1WorldCellSize;
    int row = y / kUnifiedFallout1WorldCellSize;
    for (int areaIdx = 0; areaIdx < kUnifiedFallout1TownCountForState; areaIdx++) {
        if (kUnifiedFallout1TownLocations[areaIdx].column == column
            && kUnifiedFallout1TownLocations[areaIdx].row == row) {
            return areaIdx;
        }
    }

    return -1;
}

inline void unifiedFallout1MarkTownKnown(int areaIdx, bool known)
{
    if (!unifiedFallout1TownIndexIsValid(areaIdx)) {
        return;
    }

    UnifiedFallout1WorldMapState& state = unifiedFallout1WorldMapGetState();
    int mask = 1 << areaIdx;
    if (known) {
        state.firstVisitFlags |= mask;
        state.townSelectionKnowledge[areaIdx][0] = 1;

        const UnifiedFallout1TownLocation& location = kUnifiedFallout1TownLocations[areaIdx];
        if (location.row >= 0
            && location.row < kUnifiedFallout1WorldMapRows
            && location.column >= 0
            && location.column < kUnifiedFallout1WorldMapColumns
            && state.worldGrid[location.row][location.column] == 0) {
            state.worldGrid[location.row][location.column] = 1;
        }
    } else {
        state.firstVisitFlags &= ~mask;
    }
}

inline int unifiedWmGetPartyWorldPos(int* xPtr, int* yPtr)
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmGetPartyWorldPos(xPtr, yPtr);
    }

    const UnifiedFallout1WorldMapState& state = unifiedFallout1WorldMapGetStateConst();
    if (xPtr != nullptr) {
        *xPtr = state.worldX;
    }
    if (yPtr != nullptr) {
        *yPtr = state.worldY;
    }
    return 0;
}

inline void unifiedWmSetPartyWorldPos(int x, int y)
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        wmSetPartyWorldPos(x, y);
        return;
    }

    UnifiedFallout1WorldMapState& state = unifiedFallout1WorldMapGetState();
    state.worldX = x;
    state.worldY = y;

    int town = unifiedFallout1TownAtWorldPos(x, y);
    state.currentTown = town;

    if (x >= 0 && y >= 0) {
        int column = x / kUnifiedFallout1WorldCellSize;
        int row = y / kUnifiedFallout1WorldCellSize;
        if (row >= 0
            && row < kUnifiedFallout1WorldMapRows
            && column >= 0
            && column < kUnifiedFallout1WorldMapColumns) {
            state.worldGrid[row][column] = 2;
        }
    }
}

inline int unifiedWmGetPartyCurArea(int* areaIdxPtr)
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmGetPartyCurArea(areaIdxPtr);
    }

    if (areaIdxPtr == nullptr) {
        return -1;
    }

    const UnifiedFallout1WorldMapState& state = unifiedFallout1WorldMapGetStateConst();
    *areaIdxPtr = unifiedFallout1TownAtWorldPos(state.worldX, state.worldY);
    return 0;
}

inline int unifiedWmGetAreaIdxName(int areaIdx, char* name)
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmGetAreaIdxName(areaIdx, name);
    }

    if (!unifiedFallout1TownIndexIsValid(areaIdx) || name == nullptr) {
        return -1;
    }

    // Fallout 1's world-map hover/short-name path reads map.msg entries
    // TOWN_* + 500. F2 instead uses areaId + 1500.
    MessageListItem messageListItem {};
    char* text = getmsg(&gMapMessageList, &messageListItem, 500 + areaIdx);
    if (text == nullptr) {
        name[0] = '\0';
        return -1;
    }

    std::strncpy(name, text, 39);
    name[39] = '\0';
    return 0;
}

inline bool unifiedWmAreaIsKnown(int areaIdx)
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmAreaIsKnown(areaIdx);
    }

    if (!unifiedFallout1TownIndexIsValid(areaIdx)) {
        return false;
    }

    unifiedFallout1WorldMapSyncFromGlobals();
    const UnifiedFallout1WorldMapState& state = unifiedFallout1WorldMapGetStateConst();
    return (state.firstVisitFlags & (1 << areaIdx)) != 0;
}

inline int unifiedWmAreaVisitedState(int areaIdx)
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmAreaVisitedState(areaIdx);
    }

    // F1 persists one town-discovery bit rather than F2's separate visible and
    // visited fields. Expose discovered F1 towns as fully visited to F2 callers.
    return unifiedWmAreaIsKnown(areaIdx) ? 2 : 0;
}

inline bool unifiedWmAreaMarkVisitedState(int areaIdx, int state)
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmAreaMarkVisitedState(areaIdx, state);
    }

    if (!unifiedFallout1TownIndexIsValid(areaIdx)) {
        return false;
    }

    unifiedFallout1MarkTownKnown(areaIdx, state != 0);
    return true;
}

inline int unifiedWmAreaMarkVisited(int areaIdx)
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmAreaMarkVisited(areaIdx);
    }

    return unifiedWmAreaMarkVisitedState(areaIdx, 2);
}

inline bool unifiedWmAreaSetVisibleState(int areaIdx, int state, bool force)
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmAreaSetVisibleState(areaIdx, state, force);
    }

    (void)force;
    if (!unifiedFallout1TownIndexIsValid(areaIdx)) {
        return false;
    }

    // F1 has no separately persisted F2-style city visibility field. Its
    // first-visit/town-known bit is the closest original semantic contract.
    unifiedFallout1MarkTownKnown(areaIdx, state != 0 && state != -66);
    return true;
}

inline int unifiedWmAreaSetWorldPos(int areaIdx, int x, int y)
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmAreaSetWorldPos(areaIdx, x, y);
    }

    // F1 town positions are a fixed compile-time table. This F2 scripting API
    // has no original F1 equivalent; fail safely instead of touching F2 city
    // storage that is intentionally uninitialized in the F1 profile.
    (void)areaIdx;
    (void)x;
    (void)y;
    return -1;
}

inline int unifiedWmTeleportToArea(int areaIdx)
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmTeleportToArea(areaIdx);
    }

    if (!unifiedFallout1TownIndexIsValid(areaIdx)) {
        return -1;
    }

    UnifiedFallout1WorldMapState& state = unifiedFallout1WorldMapGetState();
    state.worldX = unifiedFallout1TownWorldX(areaIdx);
    state.worldY = unifiedFallout1TownWorldY(areaIdx);
    state.currentTown = areaIdx;
    state.currentSection = 1;
    unifiedFallout1MarkTownKnown(areaIdx, true);

    int column = kUnifiedFallout1TownLocations[areaIdx].column;
    int row = kUnifiedFallout1TownLocations[areaIdx].row;
    if (row >= 0
        && row < kUnifiedFallout1WorldMapRows
        && column >= 0
        && column < kUnifiedFallout1WorldMapColumns) {
        state.worldGrid[row][column] = 2;
    }

    return 0;
}

} // namespace fallout

#ifndef WORLD_MAP_H
#define wmGetPartyWorldPos unifiedWmGetPartyWorldPos
#define wmGetPartyCurArea unifiedWmGetPartyCurArea
#define wmSetPartyWorldPos unifiedWmSetPartyWorldPos
#define wmGetAreaIdxName unifiedWmGetAreaIdxName
#define wmAreaIsKnown unifiedWmAreaIsKnown
#define wmAreaVisitedState unifiedWmAreaVisitedState
#define wmAreaMarkVisited unifiedWmAreaMarkVisited
#define wmAreaMarkVisitedState unifiedWmAreaMarkVisitedState
#define wmAreaSetVisibleState unifiedWmAreaSetVisibleState
#define wmAreaSetWorldPos unifiedWmAreaSetWorldPos
#define wmTeleportToArea unifiedWmTeleportToArea
#endif

#endif /* UNIFIED_WORLDMAP_STATE_PROFILE_H */
