#ifndef UNIFIED_FALLOUT1_WILDERNESS_STATE_H
#define UNIFIED_FALLOUT1_WILDERNESS_STATE_H

#include <stdint.h>
#include <string.h>

#include "unified_fallout1_worldmap_state.h"

namespace fallout {

inline constexpr int kUnifiedFallout1WildernessColumns = 28;
inline constexpr int kUnifiedFallout1WildernessRows = 30;
inline constexpr int kUnifiedFallout1WildernessLogCapacity = 16;
inline constexpr uint32_t kUnifiedFallout1WildernessDefaultSeed = 0xF01C0A57;
inline constexpr uint32_t kUnifiedFallout1WildernessChunkMagic = 0x31444C57; // "WLD1"
inline constexpr uint32_t kUnifiedFallout1WildernessChunkVersion = 1;

enum UnifiedFallout1WildernessCellFlags : uint8_t {
    UNIFIED_WILDERNESS_DISCOVERED = 0x01,
    UNIFIED_WILDERNESS_VISITED = 0x02,
    UNIFIED_WILDERNESS_CLEARED = 0x04,
    UNIFIED_WILDERNESS_LOOTED = 0x08,
    UNIFIED_WILDERNESS_ACTIVE_EVENT = 0x10,
    UNIFIED_WILDERNESS_TEMPORARY_DUNGEON = 0x20,
};

enum class UnifiedFallout1WildernessLogType : uint8_t {
    Entered = 0,
    Encounter = 1,
    ChainAdvanced = 2,
    Cleared = 3,
    Looted = 4,
    EventCreated = 5,
    EventExpired = 6,
};

struct UnifiedFallout1WildernessCellState {
    uint32_t seed;
    int32_t lastVisitGameTime;
    int32_t temporaryEventExpiry;
    int16_t templateMapIdx;
    uint8_t terrain;
    uint8_t region;
    uint8_t flags;
    uint8_t chainDepth;
    uint8_t chainLength;
    uint8_t exitMask;
};

struct UnifiedFallout1WildernessLogEntry {
    uint32_t gameTime;
    int16_t cellX;
    int16_t cellY;
    int16_t templateMapIdx;
    uint8_t eventType;
    uint8_t detail;
};

struct UnifiedFallout1WildernessState {
    uint32_t worldSeed;
    uint32_t logSequence;
    uint8_t logHead;
    uint8_t logCount;
    uint8_t reserved[2];
    UnifiedFallout1WildernessCellState cells[kUnifiedFallout1WildernessRows][kUnifiedFallout1WildernessColumns];
    UnifiedFallout1WildernessLogEntry log[kUnifiedFallout1WildernessLogCapacity];
};

static_assert(sizeof(UnifiedFallout1WildernessCellState) == 20,
    "Fallout 1 wilderness cell payload must remain byte-stable");
static_assert(sizeof(UnifiedFallout1WildernessLogEntry) == 12,
    "Fallout 1 wilderness log payload must remain byte-stable");
static_assert(sizeof(UnifiedFallout1WildernessState) == 17004,
    "Fallout 1 wilderness registry payload must remain byte-stable");

inline UnifiedFallout1WildernessState gUnifiedFallout1WildernessState {};
inline UnifiedFallout1WildernessState gUnifiedFallout1PendingWildernessState {};
inline bool gUnifiedFallout1WildernessStateInitialized = false;
inline bool gUnifiedFallout1PendingWildernessStateValid = false;
inline bool gUnifiedFallout1PreserveWildernessOnNextReset = false;

inline uint32_t unifiedFallout1WildernessMixSeed(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7FEB352D;
    value ^= value >> 15;
    value *= 0x846CA68B;
    value ^= value >> 16;
    return value;
}

inline uint32_t unifiedFallout1WildernessCellSeed(uint32_t worldSeed, int cellX, int cellY)
{
    uint32_t coordinate = static_cast<uint32_t>(cellX + cellY * kUnifiedFallout1WildernessColumns);
    return unifiedFallout1WildernessMixSeed(worldSeed ^ (coordinate * 0x9E3779B9));
}

inline uint8_t unifiedFallout1WildernessExitMask(int cellX, int cellY)
{
    uint8_t mask = 0;
    if (cellY > 0) {
        mask |= 0x01; // north
    }
    if (cellX + 1 < kUnifiedFallout1WildernessColumns) {
        mask |= 0x02; // east
    }
    if (cellY + 1 < kUnifiedFallout1WildernessRows) {
        mask |= 0x04; // south
    }
    if (cellX > 0) {
        mask |= 0x08; // west
    }
    return mask;
}

inline void unifiedFallout1WildernessReset(UnifiedFallout1WildernessState& state, uint32_t worldSeed)
{
    memset(&state, 0, sizeof(state));
    state.worldSeed = worldSeed != 0 ? worldSeed : kUnifiedFallout1WildernessDefaultSeed;

    for (int cellY = 0; cellY < kUnifiedFallout1WildernessRows; cellY++) {
        for (int cellX = 0; cellX < kUnifiedFallout1WildernessColumns; cellX++) {
            UnifiedFallout1WildernessCellState& cell = state.cells[cellY][cellX];
            cell.seed = unifiedFallout1WildernessCellSeed(state.worldSeed, cellX, cellY);
            cell.templateMapIdx = -1;
            cell.chainLength = static_cast<uint8_t>(1 + cell.seed % 4);
            cell.exitMask = unifiedFallout1WildernessExitMask(cellX, cellY);
        }
    }
}

inline void unifiedFallout1WildernessEnsureInitialized()
{
    if (!gUnifiedFallout1WildernessStateInitialized) {
        unifiedFallout1WildernessReset(
            gUnifiedFallout1WildernessState,
            kUnifiedFallout1WildernessDefaultSeed);
        gUnifiedFallout1WildernessStateInitialized = true;
    }
}

inline UnifiedFallout1WildernessState& unifiedFallout1WildernessGetState()
{
    unifiedFallout1WildernessEnsureInitialized();
    return gUnifiedFallout1WildernessState;
}

inline const UnifiedFallout1WildernessState& unifiedFallout1WildernessGetStateConst()
{
    unifiedFallout1WildernessEnsureInitialized();
    return gUnifiedFallout1WildernessState;
}

inline UnifiedFallout1WildernessCellState* unifiedFallout1WildernessGetCell(int cellX, int cellY)
{
    if (cellX < 0
        || cellX >= kUnifiedFallout1WildernessColumns
        || cellY < 0
        || cellY >= kUnifiedFallout1WildernessRows) {
        return nullptr;
    }

    return &unifiedFallout1WildernessGetState().cells[cellY][cellX];
}

inline const UnifiedFallout1WildernessCellState* unifiedFallout1WildernessGetCellConst(int cellX, int cellY)
{
    if (cellX < 0
        || cellX >= kUnifiedFallout1WildernessColumns
        || cellY < 0
        || cellY >= kUnifiedFallout1WildernessRows) {
        return nullptr;
    }

    return &unifiedFallout1WildernessGetStateConst().cells[cellY][cellX];
}

inline void unifiedFallout1WildernessAppendLog(
    UnifiedFallout1WildernessLogType type,
    int cellX,
    int cellY,
    int mapIdx,
    int detail,
    uint32_t gameTime)
{
    UnifiedFallout1WildernessState& state = unifiedFallout1WildernessGetState();
    UnifiedFallout1WildernessLogEntry& entry = state.log[state.logHead];
    entry.gameTime = gameTime;
    entry.cellX = static_cast<int16_t>(cellX);
    entry.cellY = static_cast<int16_t>(cellY);
    entry.templateMapIdx = static_cast<int16_t>(mapIdx);
    entry.eventType = static_cast<uint8_t>(type);
    entry.detail = static_cast<uint8_t>(detail);

    state.logHead = static_cast<uint8_t>((state.logHead + 1) % kUnifiedFallout1WildernessLogCapacity);
    if (state.logCount < kUnifiedFallout1WildernessLogCapacity) {
        state.logCount++;
    }
    state.logSequence++;
}

inline void unifiedFallout1WildernessResetCurrent(uint32_t worldSeed)
{
    unifiedFallout1WildernessReset(gUnifiedFallout1WildernessState, worldSeed);
    gUnifiedFallout1WildernessStateInitialized = true;
}

inline void unifiedFallout1WildernessPreserveNextReset()
{
    gUnifiedFallout1PreserveWildernessOnNextReset = true;
}

inline bool unifiedFallout1WildernessConsumePreserveReset()
{
    bool preserve = gUnifiedFallout1PreserveWildernessOnNextReset;
    gUnifiedFallout1PreserveWildernessOnNextReset = false;
    return preserve;
}

inline void unifiedFallout1WildernessClearPending()
{
    gUnifiedFallout1PendingWildernessState = UnifiedFallout1WildernessState {};
    gUnifiedFallout1PendingWildernessStateValid = false;
}

inline void unifiedFallout1WildernessStage(const UnifiedFallout1WildernessState& state)
{
    gUnifiedFallout1PendingWildernessState = state;
    gUnifiedFallout1PendingWildernessStateValid = true;
}

inline bool unifiedFallout1WildernessApplyPending()
{
    if (!gUnifiedFallout1PendingWildernessStateValid) {
        return false;
    }

    gUnifiedFallout1WildernessState = gUnifiedFallout1PendingWildernessState;
    gUnifiedFallout1WildernessStateInitialized = true;
    unifiedFallout1WildernessClearPending();
    return true;
}

inline UnifiedCampaignMetaChunkHeader unifiedFallout1WildernessMakeChunkHeader()
{
    UnifiedCampaignMetaChunkHeader header {};
    header.magic = kUnifiedFallout1WildernessChunkMagic;
    header.version = kUnifiedFallout1WildernessChunkVersion;
    header.payloadSize = static_cast<uint32_t>(sizeof(UnifiedFallout1WildernessState));
    return header;
}

inline bool unifiedFallout1WildernessChunkIsSupported(const UnifiedCampaignMetaChunkHeader& header)
{
    return header.magic == kUnifiedFallout1WildernessChunkMagic
        && header.version == kUnifiedFallout1WildernessChunkVersion
        && header.payloadSize == sizeof(UnifiedFallout1WildernessState);
}

} // namespace fallout

#endif /* UNIFIED_FALLOUT1_WILDERNESS_STATE_H */
