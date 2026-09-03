#ifndef LOCAL_COOP_CHARACTER_STATE_H
#define LOCAL_COOP_CHARACTER_STATE_H

#include <stdint.h>
#include <string.h>

#include "unified_fallout1_worldmap_state.h"

namespace fallout {

inline constexpr int kLocalCoopCharacterSlotCount = 4;
inline constexpr uint32_t kLocalCoopCharacterChunkMagic = 0x53504F43; // "COPS"
inline constexpr uint32_t kLocalCoopCharacterChunkVersion = 1;

struct LocalCoopCharacterSlotState {
    uint8_t locked;
    uint8_t archetype;
    uint8_t gender;
    uint8_t reserved;
    char controllerGuid[64];
};

struct LocalCoopCharacterState {
    LocalCoopCharacterSlotState slots[kLocalCoopCharacterSlotCount];
};

static_assert(sizeof(LocalCoopCharacterSlotState) == 68,
    "Co-op character slot payload must remain byte-stable");
static_assert(sizeof(LocalCoopCharacterState) == 272,
    "Co-op character roster payload must remain byte-stable");

inline LocalCoopCharacterState gLocalCoopCharacterState {};
inline LocalCoopCharacterState gLocalCoopPendingCharacterState {};
inline bool gLocalCoopCharacterStateInitialized = false;
inline bool gLocalCoopPendingCharacterStateValid = false;
inline uint32_t gLocalCoopCharacterStateRevision = 0;

inline void localCoopCharacterStateReset(LocalCoopCharacterState& state)
{
    memset(&state, 0, sizeof(state));
    state.slots[0].locked = 1;
}

inline void localCoopCharacterStateEnsureInitialized()
{
    if (!gLocalCoopCharacterStateInitialized) {
        localCoopCharacterStateReset(gLocalCoopCharacterState);
        gLocalCoopCharacterStateInitialized = true;
    }
}

inline LocalCoopCharacterState& localCoopCharacterStateGet()
{
    localCoopCharacterStateEnsureInitialized();
    return gLocalCoopCharacterState;
}

inline const LocalCoopCharacterState& localCoopCharacterStateGetConst()
{
    localCoopCharacterStateEnsureInitialized();
    return gLocalCoopCharacterState;
}

inline void localCoopCharacterStateResetCurrent()
{
    localCoopCharacterStateReset(gLocalCoopCharacterState);
    gLocalCoopCharacterStateInitialized = true;
    gLocalCoopCharacterStateRevision++;
}

inline void localCoopCharacterStateClearPending()
{
    gLocalCoopPendingCharacterState = LocalCoopCharacterState {};
    gLocalCoopPendingCharacterStateValid = false;
}

inline void localCoopCharacterStateStage(const LocalCoopCharacterState& state)
{
    gLocalCoopPendingCharacterState = state;
    gLocalCoopPendingCharacterStateValid = true;
}

inline bool localCoopCharacterStateApplyPending()
{
    if (!gLocalCoopPendingCharacterStateValid) {
        return false;
    }

    gLocalCoopCharacterState = gLocalCoopPendingCharacterState;
    gLocalCoopCharacterStateInitialized = true;
    localCoopCharacterStateClearPending();
    gLocalCoopCharacterStateRevision++;
    return true;
}

inline UnifiedCampaignMetaChunkHeader localCoopCharacterStateMakeChunkHeader()
{
    UnifiedCampaignMetaChunkHeader header {};
    header.magic = kLocalCoopCharacterChunkMagic;
    header.version = kLocalCoopCharacterChunkVersion;
    header.payloadSize = static_cast<uint32_t>(sizeof(LocalCoopCharacterState));
    return header;
}

inline bool localCoopCharacterStateChunkIsSupported(const UnifiedCampaignMetaChunkHeader& header)
{
    return header.magic == kLocalCoopCharacterChunkMagic
        && header.version == kLocalCoopCharacterChunkVersion
        && header.payloadSize == sizeof(LocalCoopCharacterState);
}

} // namespace fallout

#endif /* LOCAL_COOP_CHARACTER_STATE_H */
