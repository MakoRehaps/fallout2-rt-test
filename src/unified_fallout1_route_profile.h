#ifndef UNIFIED_FALLOUT1_ROUTE_PROFILE_H
#define UNIFIED_FALLOUT1_ROUTE_PROFILE_H

#include <algorithm>
#include <array>

#include "unified_fallout1_travel_profile.h"
#include "unified_fallout1_walkmask_data.h"

namespace fallout {

// Original Fallout 1 OceanSeeXTable. In the stock engine, columns strictly
// west of this boundary are treated as already-known ocean for each 50-pixel
// world-map row. This coarse boundary keeps the destination router out of deep
// ocean; the exact generated walkmask below remains the per-pixel authority.
inline constexpr int kUnifiedFallout1OceanBoundaryColumns[kUnifiedFallout1TravelRows] = {
    0, 0, 0, 0, 0, 0, 0, 1, 1, 1,
    2, 3, 4, 5, 5, 5, 5, 8, 11, 12,
    14, 14, 17, 18, 19, 19, 20, 20, 20, 21,
};

inline constexpr int kUnifiedFallout1RouteCellCount =
    kUnifiedFallout1TravelColumns * kUnifiedFallout1TravelRows;

struct UnifiedFallout1WorldRoute {
    std::array<int, kUnifiedFallout1RouteCellCount> cells {};
    int count = 0;
};

inline int unifiedFallout1RouteCellIndex(int column, int row)
{
    return row * kUnifiedFallout1TravelColumns + column;
}

inline bool unifiedFallout1RouteCellAllowed(int column, int row)
{
    if (!unifiedFallout1WorldGridCellIsValid(column, row)) {
        return false;
    }

    // OceanSeeXTable reveals [0, boundary) as ocean. The boundary column itself
    // is deliberately left available to the coarse router because it contains
    // mixed shoreline pixels resolved by the exact F1 walkmask.
    return column >= kUnifiedFallout1OceanBoundaryColumns[row];
}

inline bool unifiedFallout1RoutePointAllowed(int worldX, int worldY)
{
    if (worldX < 0 || worldY < 0
        || worldX > kUnifiedFallout1TravelMaxX
        || worldY > kUnifiedFallout1TravelMaxY) {
        return false;
    }

    int column = worldX / kUnifiedFallout1TravelCellSize;
    int row = worldY / kUnifiedFallout1TravelCellSize;
    return unifiedFallout1RouteCellAllowed(column, row)
        && !unifiedFallout1WalkmaskBlocked(worldX, worldY);
}

inline bool unifiedFallout1RouteFindAllowedPointInCell(
    int column,
    int row,
    int preferredX,
    int preferredY,
    int& worldX,
    int& worldY)
{
    if (!unifiedFallout1RouteCellAllowed(column, row)) {
        return false;
    }

    int left = column * kUnifiedFallout1TravelCellSize;
    int top = row * kUnifiedFallout1TravelCellSize;
    int right = left + kUnifiedFallout1TravelCellSize - 1;
    int bottom = top + kUnifiedFallout1TravelCellSize - 1;
    preferredX = std::max(left, std::min(preferredX, right));
    preferredY = std::max(top, std::min(preferredY, bottom));

    if (unifiedFallout1RoutePointAllowed(preferredX, preferredY)) {
        worldX = preferredX;
        worldY = preferredY;
        return true;
    }

    // Search outward from the desired point. A 50x50 coarse cell is small, so
    // this deterministic ring search cheaply finds the closest legal shoreline
    // pixel without allocating a large pathfinding grid.
    for (int radius = 1; radius < kUnifiedFallout1TravelCellSize; radius++) {
        int minX = std::max(left, preferredX - radius);
        int maxX = std::min(right, preferredX + radius);
        int minY = std::max(top, preferredY - radius);
        int maxY = std::min(bottom, preferredY + radius);

        for (int x = minX; x <= maxX; x++) {
            if (unifiedFallout1RoutePointAllowed(x, minY)) {
                worldX = x;
                worldY = minY;
                return true;
            }
            if (maxY != minY && unifiedFallout1RoutePointAllowed(x, maxY)) {
                worldX = x;
                worldY = maxY;
                return true;
            }
        }

        for (int y = minY + 1; y < maxY; y++) {
            if (unifiedFallout1RoutePointAllowed(minX, y)) {
                worldX = minX;
                worldY = y;
                return true;
            }
            if (maxX != minX && unifiedFallout1RoutePointAllowed(maxX, y)) {
                worldX = maxX;
                worldY = y;
                return true;
            }
        }
    }

    return false;
}

inline bool unifiedFallout1BuildCoarseRoute(
    int startX,
    int startY,
    int targetX,
    int targetY,
    UnifiedFallout1WorldRoute& route)
{
    route.count = 0;

    int startColumn = startX / kUnifiedFallout1TravelCellSize;
    int startRow = startY / kUnifiedFallout1TravelCellSize;
    int targetColumn = targetX / kUnifiedFallout1TravelCellSize;
    int targetRow = targetY / kUnifiedFallout1TravelCellSize;
    if (!unifiedFallout1WorldGridCellIsValid(startColumn, startRow)
        || !unifiedFallout1WorldGridCellIsValid(targetColumn, targetRow)) {
        return false;
    }

    int start = unifiedFallout1RouteCellIndex(startColumn, startRow);
    int target = unifiedFallout1RouteCellIndex(targetColumn, targetRow);

    std::array<int, kUnifiedFallout1RouteCellCount> previous {};
    previous.fill(-2);
    std::array<int, kUnifiedFallout1RouteCellCount> queue {};
    int queueRead = 0;
    int queueWrite = 0;

    previous[start] = -1;
    queue[queueWrite++] = start;

    // Four-way routing prevents a diagonal segment between legal coarse cells
    // from cutting across a fully-ocean neighboring cell.
    static constexpr int kDirections[4][2] = {
        { 1, 0 },
        { -1, 0 },
        { 0, 1 },
        { 0, -1 },
    };

    while (queueRead < queueWrite && previous[target] == -2) {
        int cell = queue[queueRead++];
        int column = cell % kUnifiedFallout1TravelColumns;
        int row = cell / kUnifiedFallout1TravelColumns;

        for (const auto& direction : kDirections) {
            int nextColumn = column + direction[0];
            int nextRow = row + direction[1];
            if (!unifiedFallout1WorldGridCellIsValid(nextColumn, nextRow)) {
                continue;
            }

            int next = unifiedFallout1RouteCellIndex(nextColumn, nextRow);
            if (previous[next] != -2) {
                continue;
            }

            // Permit the current start cell even if an old sidecar happens to
            // place the party near a shoreline boundary, but never route into a
            // known deep-ocean cell.
            if (next != target && !unifiedFallout1RouteCellAllowed(nextColumn, nextRow)) {
                continue;
            }
            if (next == target && !unifiedFallout1RouteCellAllowed(nextColumn, nextRow)) {
                return false;
            }

            previous[next] = cell;
            queue[queueWrite++] = next;
        }
    }

    if (previous[target] == -2) {
        return false;
    }

    std::array<int, kUnifiedFallout1RouteCellCount> reverse {};
    int reverseCount = 0;
    for (int cell = target; cell != -1; cell = previous[cell]) {
        if (reverseCount >= kUnifiedFallout1RouteCellCount) {
            return false;
        }
        reverse[reverseCount++] = cell;
    }

    for (int index = reverseCount - 1; index >= 0; index--) {
        route.cells[route.count++] = reverse[index];
    }
    return route.count != 0;
}

inline bool unifiedFallout1RouteStepToward(
    UnifiedFallout1TravelLine& line,
    int& worldX,
    int& worldY,
    int waypointX,
    int waypointY,
    int& previousX,
    int& previousY)
{
    UnifiedFallout1TravelLine candidateLine = line;
    int candidateX = worldX;
    int candidateY = worldY;
    if (unifiedFallout1TravelLineStep(candidateLine, candidateX, candidateY)
        && unifiedFallout1RoutePointAllowed(candidateX, candidateY)) {
        previousX = worldX;
        previousY = worldY;
        worldX = candidateX;
        worldY = candidateY;
        line = candidateLine;
        return true;
    }

    // The stock F1 walkmask stops a straight move on blocked pixels. For the
    // controller destination router, slide around that exact pixel instead of
    // forcing the player to manually retarget repeatedly. Prefer the legal
    // neighboring pixel closest to the current waypoint and avoid immediate
    // backtracking when another choice exists.
    static constexpr int kDirections[8][2] = {
        { 1, 0 },
        { 0, 1 },
        { 0, -1 },
        { -1, 0 },
        { 1, 1 },
        { 1, -1 },
        { -1, 1 },
        { -1, -1 },
    };

    int bestX = worldX;
    int bestY = worldY;
    int bestScore = 0x7FFFFFFF;
    bool found = false;

    for (int pass = 0; pass < 2 && !found; pass++) {
        for (const auto& direction : kDirections) {
            int nextX = worldX + direction[0];
            int nextY = worldY + direction[1];
            if (!unifiedFallout1RoutePointAllowed(nextX, nextY)) {
                continue;
            }
            if (pass == 0 && nextX == previousX && nextY == previousY) {
                continue;
            }

            int dx = waypointX - nextX;
            int dy = waypointY - nextY;
            int score = dx * dx + dy * dy;
            if (!found || score < bestScore) {
                bestScore = score;
                bestX = nextX;
                bestY = nextY;
                found = true;
            }
        }
    }

    if (!found) {
        return false;
    }

    previousX = worldX;
    previousY = worldY;
    worldX = bestX;
    worldY = bestY;
    line = unifiedFallout1TravelLineCreate(worldX, worldY, waypointX, waypointY);
    return true;
}

inline int unifiedFallout1TravelToTownRouted(int town)
{
    if (!unifiedFallout1TownIndexIsValid(town)) {
        return -1;
    }

    UnifiedFallout1WorldMapState& state = unifiedFallout1WorldMapGetState();
    int targetX = unifiedFallout1TownWorldX(town);
    int targetY = unifiedFallout1TownWorldY(town);

    // Preserve Fallout 1's original +/-0..16 destination jitter.
    int offset = randomBetween(0, 16);
    targetX += randomBetween(0, 1) != 0 ? offset : -offset;
    offset = randomBetween(0, 16);
    targetY += randomBetween(0, 1) != 0 ? offset : -offset;
    targetX = std::max(0, std::min(targetX, kUnifiedFallout1TravelMaxX));
    targetY = std::max(0, std::min(targetY, kUnifiedFallout1TravelMaxY));

    int targetColumn = unifiedFallout1TownWorldX(town) / kUnifiedFallout1TravelCellSize;
    int targetRow = unifiedFallout1TownWorldY(town) / kUnifiedFallout1TravelCellSize;
    if (!unifiedFallout1RouteFindAllowedPointInCell(
            targetColumn,
            targetRow,
            targetX,
            targetY,
            targetX,
            targetY)) {
        return 0;
    }

    UnifiedFallout1WorldRoute route;
    if (!unifiedFallout1BuildCoarseRoute(state.worldX, state.worldY, targetX, targetY, route)) {
        return 0;
    }

    int milesPerDay;
    int timeAdder;
    unifiedFallout1TravelTiming(milesPerDay, timeAdder);

    int travelMile = 0;
    int mountainCounter = 0;
    int cityCounter = 0;
    int revealCounter = 0;
    int win = unifiedFallout1TravelOpenWindow("FALLOUT WORLD MAP - TRAVELLING");
    UnifiedFallout1PadEdges previousPad {};
    int previousWorldX = state.worldX;
    int previousWorldY = state.worldY;

    for (int routeIndex = 1; routeIndex < route.count; routeIndex++) {
        int cell = route.cells[routeIndex];
        int column = cell % kUnifiedFallout1TravelColumns;
        int row = cell / kUnifiedFallout1TravelColumns;
        bool finalWaypoint = routeIndex == route.count - 1;
        int waypointX = finalWaypoint
            ? targetX
            : column * kUnifiedFallout1TravelCellSize + kUnifiedFallout1TravelCellSize / 2;
        int waypointY = finalWaypoint
            ? targetY
            : row * kUnifiedFallout1TravelCellSize + kUnifiedFallout1TravelCellSize / 2;

        if (!finalWaypoint
            && !unifiedFallout1RouteFindAllowedPointInCell(
                column,
                row,
                waypointX,
                waypointY,
                waypointX,
                waypointY)) {
            if (win != -1) {
                windowDestroy(win);
            }
            return 0;
        }

        UnifiedFallout1TravelLine line = unifiedFallout1TravelLineCreate(
            state.worldX,
            state.worldY,
            waypointX,
            waypointY);
        int waypointStepGuard = 0;

        while (state.worldX != waypointX || state.worldY != waypointY) {
            if (++waypointStepGuard > kUnifiedFallout1TravelCellSize * 20) {
                if (win != -1) {
                    windowDestroy(win);
                }
                return 0;
            }

            int terrain = unifiedFallout1TerrainAt(state.worldX, state.worldY);
            bool shouldStep = true;
            bool extraStep = false;

            if (terrain == static_cast<int>(UnifiedFallout1Terrain::Mountain)) {
                mountainCounter++;
                shouldStep = (mountainCounter % 2) == 0;
            } else if (terrain == static_cast<int>(UnifiedFallout1Terrain::City)) {
                cityCounter++;
                extraStep = (cityCounter % 4) == 0;
            }

            if (shouldStep) {
                if (!unifiedFallout1RouteStepToward(
                        line,
                        state.worldX,
                        state.worldY,
                        waypointX,
                        waypointY,
                        previousWorldX,
                        previousWorldY)) {
                    if (win != -1) {
                        windowDestroy(win);
                    }
                    return 0;
                }

                if (extraStep && (state.worldX != waypointX || state.worldY != waypointY)) {
                    if (!unifiedFallout1RouteStepToward(
                            line,
                            state.worldX,
                            state.worldY,
                            waypointX,
                            waypointY,
                            previousWorldX,
                            previousWorldY)) {
                        if (win != -1) {
                            windowDestroy(win);
                        }
                        return 0;
                    }
                }
            }

            if (!unifiedFallout1TravelAdvanceTime(timeAdder)) {
                if (win != -1) {
                    windowDestroy(win);
                }
                return 0;
            }

            if (++revealCounter >= 3) {
                revealCounter = 0;
                unifiedFallout1WorldGridRevealAround(state.worldX, state.worldY, 1);
            }

            travelMile++;
            if (travelMile >= milesPerDay) {
                travelMile = 0;
                _partyMemberRestingHeal(24);

                int luck = gDude != nullptr ? critterGetStat(gDude, STAT_LUCK) : 5;
                int explorer = gDude != nullptr ? perkGetRank(gDude, PERK_EXPLORER) : 0;
                UnifiedFallout1EncounterSelection encounter = unifiedFallout1SelectTravelEncounter(
                    state.worldX,
                    state.worldY,
                    luck,
                    explorer);
                if (encounter.triggered) {
                    if (win != -1) {
                        windowDestroy(win);
                    }
                    return unifiedFallout1LoadEncounterMap(encounter);
                }
            }

            if (win != -1 && (travelMile % 8) == 0) {
                int width = windowGetWidth(win);
                int height = windowGetHeight(win);
                windowFill(win, 4, 32, width - 8, height - 36, _colorTable[0]);
                char name[40] = {};
                unifiedWmGetAreaIdxName(town, name);
                char lineText[128];
                std::snprintf(lineText, sizeof(lineText), "DESTINATION: %s", name);
                windowDrawText(win, lineText, width - 24, 12, 42, _colorTable[992]);
                std::snprintf(lineText, sizeof(lineText), "POSITION: %d, %d", state.worldX, state.worldY);
                windowDrawText(win, lineText, width - 24, 12, 68, _colorTable[992]);
                windowDrawText(win, "B / ESC: STOP TRAVELLING", width - 24, 12, 96, _colorTable[992]);
                windowRefresh(win);
            }

            int key = inputGetInput();
            UnifiedFallout1PadEdges edges = unifiedFallout1TravelReadPadEdges(previousPad);
            if (key == KEY_ESCAPE || edges.cancel) {
                if (win != -1) {
                    windowDestroy(win);
                }
                state.currentTown = unifiedFallout1TownAtWorldPos(state.worldX, state.worldY);
                return 0;
            }
        }
    }

    if (win != -1) {
        windowDestroy(win);
    }

    state.currentTown = town;
    state.currentSection = 0;
    unifiedFallout1MarkTownKnown(town, true);
    unifiedFallout1WorldGridRevealAround(state.worldX, state.worldY, 1);
    return unifiedFallout1SelectAndLoadTownEntrance(town);
}

} // namespace fallout

#endif /* UNIFIED_FALLOUT1_ROUTE_PROFILE_H */
