#ifndef UNIFIED_FALLOUT1_WORLDMAP_STATE_H
#define UNIFIED_FALLOUT1_WORLDMAP_STATE_H

#include <stdint.h>
#include <string.h>

namespace fallout {

// Fallout 1's original world-map save payload. Keep the field order and fixed
// widths identical to fallout1-ce's save_world_map/load_world_map contract:
// WorldGrid[31][29], TwnSelKnwFlag[15][7], then six 32-bit integers.
struct UnifiedFallout1WorldMapState {
    uint8_t worldGrid[31][29];
    uint8_t townSelectionKnowledge[15][7];
    int32_t firstVisitFlags;
    int32_t specialEncounterFlags;
    int32_t currentTown;
    int32_t currentSection;
    int32_t worldX;
    int32_t worldY;
};

struct UnifiedCampaignMetaChunkHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t payloadSize;
};

inline constexpr uint32_t kUnifiedFallout1WorldMapChunkMagic = 0x314D5746; // "FWM1"
inline constexpr uint32_t kUnifiedFallout1WorldMapChunkVersion = 1;
inline constexpr int kUnifiedFallout1WorldMapRows = 31;
inline constexpr int kUnifiedFallout1WorldMapColumns = 29;
inline constexpr int kUnifiedFallout1TownKnowledgeRows = 15;
inline constexpr int kUnifiedFallout1TownKnowledgeColumns = 7;

static_assert(sizeof(UnifiedFallout1WorldMapState) == 1028,
    "Fallout 1 world-map sidecar payload must remain byte-stable");

inline UnifiedFallout1WorldMapState gUnifiedFallout1WorldMapState {};
inline UnifiedFallout1WorldMapState gUnifiedFallout1PendingWorldMapState {};
inline bool gUnifiedFallout1WorldMapStateInitialized = false;
inline bool gUnifiedFallout1PendingWorldMapStateValid = false;

inline void unifiedFallout1WorldMapReset(UnifiedFallout1WorldMapState& state)
{
    memset(&state, 0, sizeof(state));

    // Fallout 1 InitWorldMapData starts the party at Vault 13. The original
    // city_location entry is column 16, row 1 and map cells are 50 pixels.
    state.worldX = 50 * 16 + 25;
    state.worldY = 50 * 1 + 25;
    state.currentTown = 0;
    state.currentSection = 1;
    state.firstVisitFlags = 1;
    state.townSelectionKnowledge[0][0] = 1;
}

inline void unifiedFallout1WorldMapEnsureInitialized()
{
    if (!gUnifiedFallout1WorldMapStateInitialized) {
        unifiedFallout1WorldMapReset(gUnifiedFallout1WorldMapState);
        gUnifiedFallout1WorldMapStateInitialized = true;
    }
}

inline UnifiedFallout1WorldMapState& unifiedFallout1WorldMapGetState()
{
    unifiedFallout1WorldMapEnsureInitialized();
    return gUnifiedFallout1WorldMapState;
}

inline const UnifiedFallout1WorldMapState& unifiedFallout1WorldMapGetStateConst()
{
    unifiedFallout1WorldMapEnsureInitialized();
    return gUnifiedFallout1WorldMapState;
}

inline void unifiedFallout1WorldMapSetState(const UnifiedFallout1WorldMapState& state)
{
    gUnifiedFallout1WorldMapState = state;
    gUnifiedFallout1WorldMapStateInitialized = true;
}

inline void unifiedFallout1WorldMapClearPending()
{
    gUnifiedFallout1PendingWorldMapState = UnifiedFallout1WorldMapState {};
    gUnifiedFallout1PendingWorldMapStateValid = false;
}

inline void unifiedFallout1WorldMapStage(const UnifiedFallout1WorldMapState& state)
{
    gUnifiedFallout1PendingWorldMapState = state;
    gUnifiedFallout1PendingWorldMapStateValid = true;
}

inline bool unifiedFallout1WorldMapApplyPending()
{
    if (!gUnifiedFallout1PendingWorldMapStateValid) {
        return false;
    }

    UnifiedFallout1WorldMapState state = gUnifiedFallout1PendingWorldMapState;
    unifiedFallout1WorldMapClearPending();
    unifiedFallout1WorldMapSetState(state);
    return true;
}

inline UnifiedCampaignMetaChunkHeader unifiedFallout1WorldMapMakeChunkHeader()
{
    UnifiedCampaignMetaChunkHeader header {};
    header.magic = kUnifiedFallout1WorldMapChunkMagic;
    header.version = kUnifiedFallout1WorldMapChunkVersion;
    header.payloadSize = static_cast<uint32_t>(sizeof(UnifiedFallout1WorldMapState));
    return header;
}

inline bool unifiedFallout1WorldMapChunkIsSupported(const UnifiedCampaignMetaChunkHeader& header)
{
    return header.magic == kUnifiedFallout1WorldMapChunkMagic
        && header.version == kUnifiedFallout1WorldMapChunkVersion
        && header.payloadSize == sizeof(UnifiedFallout1WorldMapState);
}

} // namespace fallout

#endif /* UNIFIED_FALLOUT1_WORLDMAP_STATE_H */
