#ifndef UNIFIED_WORLDMAP_GRID_PROFILE_H
#define UNIFIED_WORLDMAP_GRID_PROFILE_H

#include "unified_campaign.h"
#include "unified_fallout1_worldmap_state.h"

namespace fallout {

int wmSubTileMarkRadiusVisited(int x, int y, int radius);
int wmSubTileGetVisitedState(int x, int y, int* statePtr);

inline constexpr int kUnifiedFallout1TravelColumns = 28;
inline constexpr int kUnifiedFallout1TravelRows = 30;
inline constexpr int kUnifiedFallout1TravelCellSize = 50;

inline bool unifiedFallout1WorldGridCellIsValid(int column, int row)
{
    return column >= 0
        && column < kUnifiedFallout1TravelColumns
        && row >= 0
        && row < kUnifiedFallout1TravelRows;
}

inline void unifiedFallout1WorldGridRevealAround(int worldX, int worldY, int radius)
{
    if (worldX < 0 || worldY < 0) {
        return;
    }

    int centerColumn = worldX / kUnifiedFallout1TravelCellSize;
    int centerRow = worldY / kUnifiedFallout1TravelCellSize;
    if (!unifiedFallout1WorldGridCellIsValid(centerColumn, centerRow)) {
        return;
    }

    UnifiedFallout1WorldMapState& state = unifiedFallout1WorldMapGetState();
    if (radius < 0) {
        radius = 0;
    }

    for (int row = centerRow - radius; row <= centerRow + radius; row++) {
        for (int column = centerColumn - radius; column <= centerColumn + radius; column++) {
            if (!unifiedFallout1WorldGridCellIsValid(column, row)) {
                continue;
            }

            // The current cell is physically travelled and therefore visited.
            // Neighbour cells become known without erasing an already visited
            // state. This matches F1's original 0/1/2 exploration contract.
            if (row == centerRow && column == centerColumn) {
                state.worldGrid[row][column] = 2;
            } else if (state.worldGrid[row][column] == 0) {
                state.worldGrid[row][column] = 1;
            }
        }
    }
}

inline int unifiedWmSubTileMarkRadiusVisited(int x, int y, int radius)
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmSubTileMarkRadiusVisited(x, y, radius);
    }

    if (x < 0 || y < 0 || radius < 0) {
        return -1;
    }

    int centerColumn = x / kUnifiedFallout1TravelCellSize;
    int centerRow = y / kUnifiedFallout1TravelCellSize;
    if (!unifiedFallout1WorldGridCellIsValid(centerColumn, centerRow)) {
        return -1;
    }

    UnifiedFallout1WorldMapState& state = unifiedFallout1WorldMapGetState();
    for (int row = centerRow - radius; row <= centerRow + radius; row++) {
        for (int column = centerColumn - radius; column <= centerColumn + radius; column++) {
            if (unifiedFallout1WorldGridCellIsValid(column, row)) {
                state.worldGrid[row][column] = 2;
            }
        }
    }

    return 0;
}

inline int unifiedWmSubTileGetVisitedState(int x, int y, int* statePtr)
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmSubTileGetVisitedState(x, y, statePtr);
    }

    if (statePtr == nullptr || x < 0 || y < 0) {
        return -1;
    }

    int column = x / kUnifiedFallout1TravelCellSize;
    int row = y / kUnifiedFallout1TravelCellSize;
    if (!unifiedFallout1WorldGridCellIsValid(column, row)) {
        *statePtr = 0;
        return -1;
    }

    const UnifiedFallout1WorldMapState& state = unifiedFallout1WorldMapGetStateConst();
    *statePtr = state.worldGrid[row][column];
    return 0;
}

} // namespace fallout

#ifndef WORLD_MAP_H
#define wmSubTileMarkRadiusVisited unifiedWmSubTileMarkRadiusVisited
#define wmSubTileGetVisitedState unifiedWmSubTileGetVisitedState
#endif

#endif /* UNIFIED_WORLDMAP_GRID_PROFILE_H */
