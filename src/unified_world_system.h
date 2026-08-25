#ifndef UNIFIED_WORLD_SYSTEM_H
#define UNIFIED_WORLD_SYSTEM_H

#include <stdint.h>
#include <string.h>

#include <algorithm>

#include "unified_campaign.h"
#include "unified_fallout1_worldmap_state.h"

namespace fallout {

inline constexpr int kUnifiedWorldSystemColumns = 28;
inline constexpr int kUnifiedWorldSystemRows = 30;
inline constexpr int kUnifiedWorldSystemCellCount =
    kUnifiedWorldSystemColumns * kUnifiedWorldSystemRows;
inline constexpr int kUnifiedWorldSystemGameCount = 2;
inline constexpr int kUnifiedWorldSystemMaxChainMaps = 4;
inline constexpr int kUnifiedWorldSystemMaxRegisteredMaps = 150;
inline constexpr int kUnifiedWorldSystemLogCapacity = 32;
inline constexpr int kUnifiedWorldSystemCellSize = 50;
inline constexpr uint32_t kUnifiedWorldSystemChunkMagic = 0x31535755; // "UWS1"
inline constexpr uint32_t kUnifiedWorldSystemChunkVersion = 1;
inline constexpr uint32_t kUnifiedWorldSystemDefaultSeed = 0x5753544D;
inline constexpr uint32_t kUnifiedWorldSystemEventLifetime = 7 * 864000;

enum UnifiedWorldSystemCellFlags : uint8_t {
    UNIFIED_WORLD_CELL_DISCOVERED = 0x01,
    UNIFIED_WORLD_CELL_VISITED = 0x02,
    UNIFIED_WORLD_CELL_CLEARED = 0x04,
    UNIFIED_WORLD_CELL_ACTIVE_EVENT = 0x08,
    UNIFIED_WORLD_CELL_TEMPORARY_DUNGEON = 0x10,
};

enum class UnifiedWorldSystemLogType : uint8_t {
    CellEntered = 0,
    EncounterStarted = 1,
    ChainAdvanced = 2,
    ChainCleared = 3,
    SpecialEncounter = 4,
    DungeonCreated = 5,
    DungeonExpired = 6,
    MapVisited = 7,
};

struct UnifiedWorldSystemCellState {
    uint32_t seed;
    int32_t lastVisitGameTime;
    int32_t temporaryEventExpiry;
    int16_t templateMapIdx;
    int16_t temporaryDungeonMapIdx;
    int16_t chainMaps[kUnifiedWorldSystemMaxChainMaps];
    uint8_t flags;
    uint8_t terrain;
    uint8_t chainLength;
    uint8_t chainDepth;
};

struct UnifiedWorldSystemWorldState {
    uint32_t seed;
    int16_t lastPhysicalCell;
    int16_t reserved;
    uint8_t visitedMaps[kUnifiedWorldSystemMaxRegisteredMaps];
    UnifiedWorldSystemCellState cells[kUnifiedWorldSystemCellCount];
};

struct UnifiedWorldSystemActiveChain {
    int32_t gameId;
    int32_t cellX;
    int32_t cellY;
    int32_t currentMapIdx;
    int32_t encounterTableId;
    int32_t encounterEntryId;
    int16_t maps[kUnifiedWorldSystemMaxChainMaps];
    uint8_t valid;
    uint8_t special;
    uint8_t depth;
    uint8_t length;
};

struct UnifiedWorldSystemTravelState {
    int32_t fallout1TargetTown;
    uint8_t fallout1TravelActive;
    uint8_t pipboyTravelActive;
    uint8_t selectionConfirmed;
    uint8_t reserved;
    int16_t currentCellX[kUnifiedWorldSystemGameCount];
    int16_t currentCellY[kUnifiedWorldSystemGameCount];
    int16_t selectedCellX[kUnifiedWorldSystemGameCount];
    int16_t selectedCellY[kUnifiedWorldSystemGameCount];
    int16_t targetCellX[kUnifiedWorldSystemGameCount];
    int16_t targetCellY[kUnifiedWorldSystemGameCount];
};

struct UnifiedWorldSystemLogEntry {
    uint32_t gameTime;
    int16_t gameId;
    int16_t cellX;
    int16_t cellY;
    int16_t mapIdx;
    uint8_t type;
    uint8_t detail;
};

struct UnifiedWorldSystemState {
    uint32_t revision;
    uint32_t logSequence;
    uint8_t logHead;
    uint8_t logCount;
    uint8_t reserved[2];
    UnifiedWorldSystemWorldState worlds[kUnifiedWorldSystemGameCount];
    UnifiedWorldSystemActiveChain activeChain;
    UnifiedWorldSystemTravelState travel;
    UnifiedWorldSystemLogEntry log[kUnifiedWorldSystemLogCapacity];
};

inline UnifiedWorldSystemState gUnifiedWorldSystemState {};
inline UnifiedWorldSystemState gUnifiedWorldSystemPendingState {};
inline bool gUnifiedWorldSystemStateInitialized = false;
inline bool gUnifiedWorldSystemPendingStateValid = false;

inline constexpr int kUnifiedWorldSystemFallout1OrdinaryMaps[] = {
    0, 1, 2, 19, 20, 21, 49, 50,
    56, 57, 58, 59, 61, 62, 63, 64,
};

inline constexpr int kUnifiedWorldSystemFallout2OrdinaryMaps[] = {
    0, 1, 2,
    68, 69, 70, 71, 72, 73, 74, 75, 76, 77,
    80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91,
    94, 95,
    110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120,
    121, 122, 123, 124, 125,
    141, 142, 143, 144, 145, 146,
};

inline int unifiedWorldSystemGameIndex(UnifiedGameId game)
{
    return game == UnifiedGameId::Fallout1 ? 0 : 1;
}

inline uint32_t unifiedWorldSystemMixSeed(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7FEB352D;
    value ^= value >> 15;
    value *= 0x846CA68B;
    value ^= value >> 16;
    return value;
}

inline int unifiedWorldSystemCellIndex(int cellX, int cellY)
{
    if (cellX < 0 || cellX >= kUnifiedWorldSystemColumns
        || cellY < 0 || cellY >= kUnifiedWorldSystemRows) {
        return -1;
    }
    return cellY * kUnifiedWorldSystemColumns + cellX;
}

inline int unifiedWorldSystemMapCount(UnifiedGameId game)
{
    return game == UnifiedGameId::Fallout1 ? 66 : 150;
}

inline bool unifiedWorldSystemMapIsRegistered(UnifiedGameId game, int mapIdx)
{
    return mapIdx >= 0 && mapIdx < unifiedWorldSystemMapCount(game);
}

inline bool unifiedWorldSystemMapIsSpecial(UnifiedGameId game, int mapIdx)
{
    if (game == UnifiedGameId::Fallout1) {
        return mapIdx == 22 || mapIdx == 23
            || (mapIdx >= 51 && mapIdx <= 54);
    }

    return (mapIdx >= 96 && mapIdx <= 108) || mapIdx == 149;
}

inline bool unifiedWorldSystemPoolContains(UnifiedGameId game, int mapIdx)
{
    if (game == UnifiedGameId::Fallout1) {
        for (int candidate : kUnifiedWorldSystemFallout1OrdinaryMaps) {
            if (candidate == mapIdx) {
                return true;
            }
        }
    } else {
        for (int candidate : kUnifiedWorldSystemFallout2OrdinaryMaps) {
            if (candidate == mapIdx) {
                return true;
            }
        }
    }
    return false;
}

inline int unifiedWorldSystemPoolSize(UnifiedGameId game)
{
    return game == UnifiedGameId::Fallout1
        ? static_cast<int>(sizeof(kUnifiedWorldSystemFallout1OrdinaryMaps) / sizeof(int))
        : static_cast<int>(sizeof(kUnifiedWorldSystemFallout2OrdinaryMaps) / sizeof(int));
}

inline int unifiedWorldSystemPoolMap(UnifiedGameId game, uint32_t selection)
{
    int count = unifiedWorldSystemPoolSize(game);
    if (count <= 0) {
        return -1;
    }
    int index = static_cast<int>(selection % static_cast<uint32_t>(count));
    return game == UnifiedGameId::Fallout1
        ? kUnifiedWorldSystemFallout1OrdinaryMaps[index]
        : kUnifiedWorldSystemFallout2OrdinaryMaps[index];
}

inline void unifiedWorldSystemReset(UnifiedWorldSystemState& state)
{
    memset(&state, 0, sizeof(state));
    state.revision = 1;
    state.travel.fallout1TargetTown = -1;
    state.travel.currentCellX[0] = 16;
    state.travel.currentCellY[0] = 1;
    state.travel.currentCellX[1] = 17;
    state.travel.currentCellY[1] = 28;
    for (int gameIndex = 0; gameIndex < kUnifiedWorldSystemGameCount; gameIndex++) {
        state.travel.selectedCellX[gameIndex] = state.travel.currentCellX[gameIndex];
        state.travel.selectedCellY[gameIndex] = state.travel.currentCellY[gameIndex];
        state.travel.targetCellX[gameIndex] = state.travel.currentCellX[gameIndex];
        state.travel.targetCellY[gameIndex] = state.travel.currentCellY[gameIndex];
    }
    state.activeChain.gameId = -1;
    state.activeChain.currentMapIdx = -1;
    state.activeChain.encounterTableId = -1;
    state.activeChain.encounterEntryId = -1;

    for (int gameIndex = 0; gameIndex < kUnifiedWorldSystemGameCount; gameIndex++) {
        UnifiedWorldSystemWorldState& world = state.worlds[gameIndex];
        world.seed = unifiedWorldSystemMixSeed(
            kUnifiedWorldSystemDefaultSeed ^ static_cast<uint32_t>(gameIndex * 0x9E3779B9));
        world.lastPhysicalCell = -1;

        for (int cellIndex = 0; cellIndex < kUnifiedWorldSystemCellCount; cellIndex++) {
            UnifiedWorldSystemCellState& cell = world.cells[cellIndex];
            cell.seed = unifiedWorldSystemMixSeed(
                world.seed ^ static_cast<uint32_t>(cellIndex * 0x9E3779B9));
            cell.templateMapIdx = -1;
            cell.temporaryDungeonMapIdx = -1;
            cell.chainLength = static_cast<uint8_t>(1 + cell.seed % 4);
            for (int chainIndex = 0; chainIndex < kUnifiedWorldSystemMaxChainMaps; chainIndex++) {
                cell.chainMaps[chainIndex] = -1;
            }
        }
    }
}

inline void unifiedWorldSystemEnsureInitialized()
{
    if (!gUnifiedWorldSystemStateInitialized) {
        unifiedWorldSystemReset(gUnifiedWorldSystemState);
        gUnifiedWorldSystemStateInitialized = true;
    }
}

inline UnifiedWorldSystemState& unifiedWorldSystemGetState()
{
    unifiedWorldSystemEnsureInitialized();
    return gUnifiedWorldSystemState;
}

inline const UnifiedWorldSystemState& unifiedWorldSystemGetStateConst()
{
    unifiedWorldSystemEnsureInitialized();
    return gUnifiedWorldSystemState;
}

inline void unifiedWorldSystemAppendLog(
    UnifiedWorldSystemLogType type,
    UnifiedGameId game,
    int cellX,
    int cellY,
    int mapIdx,
    int detail,
    uint32_t gameTime)
{
    UnifiedWorldSystemState& state = unifiedWorldSystemGetState();
    UnifiedWorldSystemLogEntry& entry = state.log[state.logHead];
    entry.gameTime = gameTime;
    entry.gameId = static_cast<int16_t>(static_cast<uint32_t>(game));
    entry.cellX = static_cast<int16_t>(cellX);
    entry.cellY = static_cast<int16_t>(cellY);
    entry.mapIdx = static_cast<int16_t>(mapIdx);
    entry.type = static_cast<uint8_t>(type);
    entry.detail = static_cast<uint8_t>(detail);
    state.logHead = static_cast<uint8_t>((state.logHead + 1) % kUnifiedWorldSystemLogCapacity);
    if (state.logCount < kUnifiedWorldSystemLogCapacity) {
        state.logCount++;
    }
    state.logSequence++;
}

inline UnifiedWorldSystemCellState* unifiedWorldSystemGetCell(
    UnifiedGameId game,
    int cellX,
    int cellY)
{
    int cellIndex = unifiedWorldSystemCellIndex(cellX, cellY);
    if (cellIndex == -1) {
        return nullptr;
    }
    return &unifiedWorldSystemGetState().worlds[unifiedWorldSystemGameIndex(game)].cells[cellIndex];
}

inline void unifiedWorldSystemExpireDungeon(
    UnifiedGameId game,
    int cellX,
    int cellY,
    UnifiedWorldSystemCellState& cell,
    uint32_t gameTime)
{
    if ((cell.flags & UNIFIED_WORLD_CELL_TEMPORARY_DUNGEON) == 0
        || cell.temporaryEventExpiry <= 0
        || gameTime < static_cast<uint32_t>(cell.temporaryEventExpiry)) {
        return;
    }

    int expiredMap = cell.temporaryDungeonMapIdx;
    cell.flags &= ~(UNIFIED_WORLD_CELL_ACTIVE_EVENT | UNIFIED_WORLD_CELL_TEMPORARY_DUNGEON);
    cell.temporaryDungeonMapIdx = -1;
    cell.temporaryEventExpiry = 0;
    unifiedWorldSystemAppendLog(
        UnifiedWorldSystemLogType::DungeonExpired,
        game,
        cellX,
        cellY,
        expiredMap,
        0,
        gameTime);
}

inline void unifiedWorldSystemPrepareCell(
    UnifiedGameId game,
    int cellX,
    int cellY,
    int preferredMap,
    uint32_t gameTime)
{
    UnifiedWorldSystemCellState* cell = unifiedWorldSystemGetCell(game, cellX, cellY);
    if (cell == nullptr) {
        return;
    }

    unifiedWorldSystemExpireDungeon(game, cellX, cellY, *cell, gameTime);
    if (cell->templateMapIdx == -1) {
        cell->templateMapIdx = static_cast<int16_t>(
            unifiedWorldSystemPoolContains(game, preferredMap)
                ? preferredMap
                : unifiedWorldSystemPoolMap(game, cell->seed));
    }

    if ((cell->seed % 37) == 0
        && (cell->flags & UNIFIED_WORLD_CELL_TEMPORARY_DUNGEON) == 0) {
        cell->flags |= UNIFIED_WORLD_CELL_ACTIVE_EVENT | UNIFIED_WORLD_CELL_TEMPORARY_DUNGEON;
        cell->temporaryDungeonMapIdx = static_cast<int16_t>(
            unifiedWorldSystemPoolMap(game, unifiedWorldSystemMixSeed(cell->seed ^ 0xD06E0A11)));
        cell->temporaryEventExpiry = static_cast<int32_t>(gameTime + kUnifiedWorldSystemEventLifetime);
        unifiedWorldSystemAppendLog(
            UnifiedWorldSystemLogType::DungeonCreated,
            game,
            cellX,
            cellY,
            cell->temporaryDungeonMapIdx,
            7,
            gameTime);
    }
}

inline void unifiedWorldSystemBuildChain(
    UnifiedGameId game,
    int cellX,
    int cellY,
    int initialMap,
    int encounterTableId,
    int encounterEntryId,
    uint32_t gameTime)
{
    unifiedWorldSystemPrepareCell(game, cellX, cellY, initialMap, gameTime);
    UnifiedWorldSystemCellState* cell = unifiedWorldSystemGetCell(game, cellX, cellY);
    if (cell == nullptr) {
        return;
    }

    int firstMap = unifiedWorldSystemPoolContains(game, initialMap)
        ? initialMap
        : cell->templateMapIdx;
    int length = cell->chainLength;
    if ((cell->flags & UNIFIED_WORLD_CELL_TEMPORARY_DUNGEON) != 0) {
        length = kUnifiedWorldSystemMaxChainMaps;
    }
    if (length < 1) {
        length = 1;
    }
    if (length > kUnifiedWorldSystemMaxChainMaps) {
        length = kUnifiedWorldSystemMaxChainMaps;
    }

    for (int index = 0; index < length; index++) {
        int mapIdx = index == 0
            ? firstMap
            : unifiedWorldSystemPoolMap(
                  game,
                  unifiedWorldSystemMixSeed(cell->seed ^ static_cast<uint32_t>(index * 0x85EBCA6B)));
        if (index > 0 && mapIdx == cell->chainMaps[index - 1]) {
            mapIdx = unifiedWorldSystemPoolMap(game, static_cast<uint32_t>(mapIdx + index + 1));
        }
        cell->chainMaps[index] = static_cast<int16_t>(mapIdx);
    }
    if ((cell->flags & UNIFIED_WORLD_CELL_TEMPORARY_DUNGEON) != 0) {
        cell->chainMaps[length - 1] = cell->temporaryDungeonMapIdx;
    }
    for (int index = length; index < kUnifiedWorldSystemMaxChainMaps; index++) {
        cell->chainMaps[index] = -1;
    }

    cell->chainLength = static_cast<uint8_t>(length);
    cell->chainDepth = 0;
    cell->flags |= UNIFIED_WORLD_CELL_DISCOVERED | UNIFIED_WORLD_CELL_VISITED;
    cell->lastVisitGameTime = static_cast<int32_t>(gameTime);

    UnifiedWorldSystemActiveChain& active = unifiedWorldSystemGetState().activeChain;
    memset(&active, 0, sizeof(active));
    active.gameId = static_cast<int32_t>(static_cast<uint32_t>(game));
    active.cellX = cellX;
    active.cellY = cellY;
    active.currentMapIdx = firstMap;
    active.encounterTableId = encounterTableId;
    active.encounterEntryId = encounterEntryId;
    active.valid = 1;
    active.length = static_cast<uint8_t>(length);
    for (int index = 0; index < kUnifiedWorldSystemMaxChainMaps; index++) {
        active.maps[index] = cell->chainMaps[index];
    }

    unifiedWorldSystemAppendLog(
        UnifiedWorldSystemLogType::EncounterStarted,
        game,
        cellX,
        cellY,
        firstMap,
        length,
        gameTime);
}

inline bool unifiedWorldSystemBeginEncounter(
    UnifiedGameId game,
    int worldX,
    int worldY,
    int mapIdx,
    bool special,
    int encounterTableId,
    int encounterEntryId,
    uint32_t gameTime)
{
    int cellX = worldX / kUnifiedWorldSystemCellSize;
    int cellY = worldY / kUnifiedWorldSystemCellSize;
    if (unifiedWorldSystemCellIndex(cellX, cellY) == -1 || mapIdx < 0) {
        return false;
    }

    UnifiedWorldSystemWorldState& world =
        unifiedWorldSystemGetState().worlds[unifiedWorldSystemGameIndex(game)];
    world.lastPhysicalCell = static_cast<int16_t>(unifiedWorldSystemCellIndex(cellX, cellY));

    if (special || unifiedWorldSystemMapIsSpecial(game, mapIdx)) {
        unifiedWorldSystemGetState().activeChain.valid = 0;
        unifiedWorldSystemAppendLog(
            UnifiedWorldSystemLogType::SpecialEncounter,
            game,
            cellX,
            cellY,
            mapIdx,
            0,
            gameTime);
        return false;
    }

    unifiedWorldSystemBuildChain(
        game,
        cellX,
        cellY,
        mapIdx,
        encounterTableId,
        encounterEntryId,
        gameTime);
    return true;
}

inline bool unifiedWorldSystemEnterTravelCell(
    UnifiedGameId game,
    int worldX,
    int worldY,
    uint32_t gameTime,
    int* mapIdxPtr)
{
    int cellX = worldX / kUnifiedWorldSystemCellSize;
    int cellY = worldY / kUnifiedWorldSystemCellSize;
    int cellIndex = unifiedWorldSystemCellIndex(cellX, cellY);
    if (cellIndex == -1 || mapIdxPtr == nullptr) {
        return false;
    }

    UnifiedWorldSystemWorldState& world =
        unifiedWorldSystemGetState().worlds[unifiedWorldSystemGameIndex(game)];
    if (world.lastPhysicalCell == cellIndex) {
        return false;
    }
    world.lastPhysicalCell = static_cast<int16_t>(cellIndex);

    unifiedWorldSystemPrepareCell(game, cellX, cellY, -1, gameTime);
    UnifiedWorldSystemCellState* cell = unifiedWorldSystemGetCell(game, cellX, cellY);
    if (cell == nullptr || cell->templateMapIdx < 0) {
        return false;
    }

    unifiedWorldSystemBuildChain(game, cellX, cellY, cell->templateMapIdx, -1, -1, gameTime);
    *mapIdxPtr = cell->templateMapIdx;
    unifiedWorldSystemAppendLog(
        UnifiedWorldSystemLogType::CellEntered,
        game,
        cellX,
        cellY,
        *mapIdxPtr,
        cell->chainLength,
        gameTime);
    return true;
}

inline void unifiedWorldSystemSetCurrentWorldPosition(
    UnifiedGameId game,
    int worldX,
    int worldY)
{
    int gameIndex = unifiedWorldSystemGameIndex(game);
    int cellX = std::max(0, std::min(worldX / kUnifiedWorldSystemCellSize, kUnifiedWorldSystemColumns - 1));
    int cellY = std::max(0, std::min(worldY / kUnifiedWorldSystemCellSize, kUnifiedWorldSystemRows - 1));
    UnifiedWorldSystemTravelState& travel = unifiedWorldSystemGetState().travel;
    travel.currentCellX[gameIndex] = static_cast<int16_t>(cellX);
    travel.currentCellY[gameIndex] = static_cast<int16_t>(cellY);
    if (!travel.pipboyTravelActive) {
        travel.selectedCellX[gameIndex] = static_cast<int16_t>(cellX);
        travel.selectedCellY[gameIndex] = static_cast<int16_t>(cellY);
    }
}

inline void unifiedWorldSystemMovePipboySelection(UnifiedGameId game, int dx, int dy)
{
    int gameIndex = unifiedWorldSystemGameIndex(game);
    UnifiedWorldSystemTravelState& travel = unifiedWorldSystemGetState().travel;
    travel.selectedCellX[gameIndex] = static_cast<int16_t>(std::max(
        0,
        std::min(static_cast<int>(travel.selectedCellX[gameIndex]) + dx, kUnifiedWorldSystemColumns - 1)));
    travel.selectedCellY[gameIndex] = static_cast<int16_t>(std::max(
        0,
        std::min(static_cast<int>(travel.selectedCellY[gameIndex]) + dy, kUnifiedWorldSystemRows - 1)));
}

inline void unifiedWorldSystemConfirmPipboySelection(UnifiedGameId game)
{
    int gameIndex = unifiedWorldSystemGameIndex(game);
    UnifiedWorldSystemTravelState& travel = unifiedWorldSystemGetState().travel;
    travel.targetCellX[gameIndex] = travel.selectedCellX[gameIndex];
    travel.targetCellY[gameIndex] = travel.selectedCellY[gameIndex];
    travel.selectionConfirmed = 1;
}

inline bool unifiedWorldSystemStartNextRouteCell(
    UnifiedGameId game,
    uint32_t gameTime,
    int* mapIdxPtr)
{
    if (mapIdxPtr == nullptr) {
        return false;
    }
    int gameIndex = unifiedWorldSystemGameIndex(game);
    UnifiedWorldSystemTravelState& travel = unifiedWorldSystemGetState().travel;
    if (travel.selectionConfirmed) {
        travel.selectionConfirmed = 0;
        travel.pipboyTravelActive = 1;
    }
    if (!travel.pipboyTravelActive) {
        return false;
    }

    int currentX = travel.currentCellX[gameIndex];
    int currentY = travel.currentCellY[gameIndex];
    int targetX = travel.targetCellX[gameIndex];
    int targetY = travel.targetCellY[gameIndex];
    if (currentX == targetX && currentY == targetY) {
        travel.pipboyTravelActive = 0;
        return false;
    }
    currentX += targetX > currentX ? 1 : (targetX < currentX ? -1 : 0);
    currentY += targetY > currentY ? 1 : (targetY < currentY ? -1 : 0);
    travel.currentCellX[gameIndex] = static_cast<int16_t>(currentX);
    travel.currentCellY[gameIndex] = static_cast<int16_t>(currentY);
    UnifiedWorldSystemWorldState& world = unifiedWorldSystemGetState().worlds[gameIndex];
    world.lastPhysicalCell = -1;
    return unifiedWorldSystemEnterTravelCell(
        game,
        currentX * kUnifiedWorldSystemCellSize + kUnifiedWorldSystemCellSize / 2,
        currentY * kUnifiedWorldSystemCellSize + kUnifiedWorldSystemCellSize / 2,
        gameTime,
        mapIdxPtr);
}

// Defined in worldmap.cc. It keeps Fallout 2's selected encounter population
// available when a four-map chain advances to another original random map.
void unifiedWorldSystemRestoreFallout2EncounterContext(
    int mapIdx,
    int encounterTableId,
    int encounterEntryId);

inline bool unifiedWorldSystemAdvanceEncounter(
    UnifiedGameId game,
    int currentMapIdx,
    int requestedMap,
    int* nextMapPtr,
    uint32_t gameTime)
{
    UnifiedWorldSystemActiveChain& active = unifiedWorldSystemGetState().activeChain;
    if (!active.valid
        || active.gameId != static_cast<int32_t>(static_cast<uint32_t>(game))
        || active.currentMapIdx != currentMapIdx
        || requestedMap != -2) {
        return false;
    }

    if (active.depth + 1 >= active.length) {
        UnifiedWorldSystemCellState* cell = unifiedWorldSystemGetCell(game, active.cellX, active.cellY);
        if (cell != nullptr) {
            cell->flags |= UNIFIED_WORLD_CELL_CLEARED;
            cell->chainDepth = active.depth;
        }
        unifiedWorldSystemAppendLog(
            UnifiedWorldSystemLogType::ChainCleared,
            game,
            active.cellX,
            active.cellY,
            active.currentMapIdx,
            active.length,
            gameTime);
        active.valid = 0;
        int routeMap = -1;
        if (unifiedWorldSystemStartNextRouteCell(game, gameTime, &routeMap)) {
            if (game == UnifiedGameId::Fallout2) {
                unifiedWorldSystemRestoreFallout2EncounterContext(routeMap, -1, -1);
            }
            if (nextMapPtr != nullptr) {
                *nextMapPtr = routeMap;
            }
            return true;
        }
        return false;
    }

    active.depth++;
    active.currentMapIdx = active.maps[active.depth];
    UnifiedWorldSystemCellState* cell = unifiedWorldSystemGetCell(game, active.cellX, active.cellY);
    if (cell != nullptr) {
        cell->chainDepth = active.depth;
        cell->lastVisitGameTime = static_cast<int32_t>(gameTime);
    }

    if (game == UnifiedGameId::Fallout2) {
        unifiedWorldSystemRestoreFallout2EncounterContext(
            active.currentMapIdx,
            active.encounterTableId,
            active.encounterEntryId);
    }

    if (nextMapPtr != nullptr) {
        *nextMapPtr = active.currentMapIdx;
    }
    unifiedWorldSystemAppendLog(
        UnifiedWorldSystemLogType::ChainAdvanced,
        game,
        active.cellX,
        active.cellY,
        active.currentMapIdx,
        active.depth,
        gameTime);
    return true;
}

inline void unifiedWorldSystemMarkMapVisited(
    UnifiedGameId game,
    int mapIdx,
    uint32_t gameTime)
{
    if (!unifiedWorldSystemMapIsRegistered(game, mapIdx)) {
        return;
    }
    UnifiedWorldSystemWorldState& world =
        unifiedWorldSystemGetState().worlds[unifiedWorldSystemGameIndex(game)];
    if (world.visitedMaps[mapIdx] == 0) {
        world.visitedMaps[mapIdx] = 1;
        unifiedWorldSystemAppendLog(
            UnifiedWorldSystemLogType::MapVisited,
            game,
            -1,
            -1,
            mapIdx,
            0,
            gameTime);
    }
}

inline void unifiedWorldSystemResetCurrent()
{
    unifiedWorldSystemReset(gUnifiedWorldSystemState);
    gUnifiedWorldSystemStateInitialized = true;
}

inline void unifiedWorldSystemClearPending()
{
    memset(&gUnifiedWorldSystemPendingState, 0, sizeof(gUnifiedWorldSystemPendingState));
    gUnifiedWorldSystemPendingStateValid = false;
}

inline void unifiedWorldSystemStage(const UnifiedWorldSystemState& state)
{
    gUnifiedWorldSystemPendingState = state;
    gUnifiedWorldSystemPendingStateValid = true;
}

inline bool unifiedWorldSystemApplyPending()
{
    if (!gUnifiedWorldSystemPendingStateValid) {
        return false;
    }
    gUnifiedWorldSystemState = gUnifiedWorldSystemPendingState;
    gUnifiedWorldSystemStateInitialized = true;
    unifiedWorldSystemClearPending();
    return true;
}

inline UnifiedCampaignMetaChunkHeader unifiedWorldSystemMakeChunkHeader()
{
    UnifiedCampaignMetaChunkHeader header {};
    header.magic = kUnifiedWorldSystemChunkMagic;
    header.version = kUnifiedWorldSystemChunkVersion;
    header.payloadSize = static_cast<uint32_t>(sizeof(UnifiedWorldSystemState));
    return header;
}

inline bool unifiedWorldSystemChunkIsSupported(const UnifiedCampaignMetaChunkHeader& header)
{
    return header.magic == kUnifiedWorldSystemChunkMagic
        && header.version == kUnifiedWorldSystemChunkVersion
        && header.payloadSize == sizeof(UnifiedWorldSystemState);
}

} // namespace fallout

#endif /* UNIFIED_WORLD_SYSTEM_H */
