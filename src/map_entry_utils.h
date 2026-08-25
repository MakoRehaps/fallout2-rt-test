#ifndef MAP_ENTRY_UTILS_H
#define MAP_ENTRY_UTILS_H

#include <algorithm>

#include "animation.h"
#include "object.h"
#include "proto.h"
#include "tile.h"
#include "unified_world_system.h"

namespace fallout {

// Shared validation for every scripted, road, encounter, and co-op spawn.
inline bool mapEntryHexIsClear(
    Object* mover,
    int tile,
    int elevation,
    bool allowExitGrid = false)
{
    return mover != nullptr
        && tileIsValid(tile)
        && elevationIsValid(elevation)
        && (allowExitGrid || !isExitGridAt(tile, elevation))
        && _obj_scroll_blocking_at(tile, elevation) != 0
        && _obj_blocking_at(mover, tile, elevation) == nullptr;
}

inline int mapEntryHexOpenNeighborCount(Object* mover, int tile, int elevation)
{
    if (!tileIsValid(tile)) {
        return 0;
    }

    int count = 0;
    for (int rotation = 0; rotation < ROTATION_COUNT; rotation++) {
        int neighbor = tileGetTileInDirection(tile, rotation, 1);
        if (mapEntryHexIsClear(mover, neighbor, elevation)) {
            count++;
        }
    }
    return count;
}

inline bool mapEntryHexHasWalkableEscape(Object* mover, int tile, int elevation)
{
    return mapEntryHexIsClear(mover, tile, elevation)
        && mapEntryHexOpenNeighborCount(mover, tile, elevation) > 0;
}

inline bool mapEntryHexIsReachable(
    Object* mover,
    int from,
    int to,
    int elevation)
{
    if (from == to) {
        return mapEntryHexHasWalkableEscape(mover, to, elevation);
    }
    if (!mapEntryHexIsClear(mover, from, elevation)
        || !mapEntryHexIsClear(mover, to, elevation)) {
        return false;
    }

    unsigned char rotations[800] {};
    return pathfinderFindPath(
               mover,
               from,
               to,
               rotations,
               static_cast<int>(sizeof(rotations)),
               _obj_blocking_at)
        > 0;
}

inline int mapEntryFindNearestSafeHex(
    Object* mover,
    int anchorTile,
    int elevation,
    int maximumRadius,
    bool requireReachable)
{
    if (mover == nullptr || !tileIsValid(anchorTile)) {
        return -1;
    }

    int bestTile = -1;
    int bestScore = -1000000;
    for (int tile = 1; tile < 40000; tile++) {
        int distance = tileDistanceBetween(anchorTile, tile);
        if (distance > maximumRadius
            || !mapEntryHexHasWalkableEscape(mover, tile, elevation)
            || (requireReachable
                && !mapEntryHexIsReachable(mover, anchorTile, tile, elevation))) {
            continue;
        }

        int openNeighbors = mapEntryHexOpenNeighborCount(mover, tile, elevation);
        int score = openNeighbors * 100 - distance;
        if (score > bestScore) {
            bestScore = score;
            bestTile = tile;
        }
    }
    return bestTile;
}

inline int mapEntryFindAuthoredExitGrid(
    UnifiedWorldSystemRoadDirection direction,
    int sourceMap,
    int elevation,
    bool exactBacklink)
{
    int bestTile = -1;
    int bestScore = -1000000;

    Object* object = objectFindFirstAtElevation(elevation);
    while (object != nullptr) {
        if ((object->flags & OBJECT_HIDDEN) == 0
            && isExitGridPid(object->pid)
            && (!exactBacklink || object->data.misc.map == sourceMap)) {
            int x = object->tile % 200;
            int y = object->tile / 200;
            int edgeScore = 0;

            // Enter the destination on the edge opposite the travel direction.
            switch (direction) {
            case UnifiedWorldSystemRoadDirection::North:
                edgeScore = y;
                break;
            case UnifiedWorldSystemRoadDirection::East:
                edgeScore = 199 - x;
                break;
            case UnifiedWorldSystemRoadDirection::South:
                edgeScore = 199 - y;
                break;
            case UnifiedWorldSystemRoadDirection::West:
                edgeScore = x;
                break;
            }

            int score = edgeScore * 10;
            if (object->data.misc.map == sourceMap) {
                score += 100000;
            }
            if (score > bestScore) {
                bestScore = score;
                bestTile = object->tile;
            }
        }
        object = objectFindNextAtElevation();
    }

    return bestTile;
}

inline int mapEntryResolveSafeEntrance(
    Object* mover,
    UnifiedWorldSystemRoadDirection direction,
    int sourceMap,
    int authoredStartTile,
    int elevation,
    const char** methodPtr)
{
    auto setMethod = [methodPtr](const char* method) {
        if (methodPtr != nullptr) {
            *methodPtr = method;
        }
    };

    // Strongest contract: an exit grid in the new map explicitly links back to
    // the map we just left. Stand beside that authored grid, never on it.
    int exitGrid = mapEntryFindAuthoredExitGrid(
        direction,
        sourceMap,
        elevation,
        true);
    if (exitGrid != -1) {
        int tile = mapEntryFindNearestSafeHex(
            mover,
            exitGrid,
            elevation,
            8,
            false);
        if (tile != -1) {
            setMethod("pid-backlink");
            return tile;
        }
    }

    // Every MAP header already contains an authored entering tile. Preserve it
    // whenever it is actually usable instead of inventing a coordinate.
    if (mapEntryHexHasWalkableEscape(mover, authoredStartTile, elevation)) {
        setMethod("map-header");
        return authoredStartTile;
    }

    // Random encounter maps often use generic -1/-2 exit destinations. In that
    // case select the authored grid on the arrival edge and stand beside it.
    exitGrid = mapEntryFindAuthoredExitGrid(
        direction,
        sourceMap,
        elevation,
        false);
    if (exitGrid != -1) {
        int tile = mapEntryFindNearestSafeHex(
            mover,
            exitGrid,
            elevation,
            8,
            false);
        if (tile != -1) {
            setMethod("directional-exit-grid");
            return tile;
        }
    }

    // Last resort stays near the MAP header and requires connectivity from the
    // current authored start whenever that start itself is valid.
    bool requireReachable =
        mapEntryHexIsClear(mover, authoredStartTile, elevation);
    int tile = mapEntryFindNearestSafeHex(
        mover,
        authoredStartTile,
        elevation,
        20,
        requireReachable);
    if (tile != -1) {
        setMethod("validated-nearby");
        return tile;
    }

    setMethod("none");
    return -1;
}

} // namespace fallout

#endif /* MAP_ENTRY_UTILS_H */
