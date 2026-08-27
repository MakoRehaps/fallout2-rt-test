#ifndef LOCAL_COOP_STAGING_ROOM_H
#define LOCAL_COOP_STAGING_ROOM_H

#include <cstring>

#include "platform_compat.h"

namespace fallout {

// COOP_PREOPENING_STAGING_ROOM_V1
inline bool gLocalCoopStagingRoomActive = false;
inline bool gLocalCoopStagingRoomConsumed = false;
inline char gLocalCoopStagingDestinationMap[COMPAT_MAX_PATH] = {};

inline void localCoopBeginStagingRoom(const char* destinationMap)
{
    gLocalCoopStagingRoomActive = true;
    gLocalCoopStagingRoomConsumed = false;
    std::strncpy(gLocalCoopStagingDestinationMap,
        destinationMap != nullptr ? destinationMap : "",
        sizeof(gLocalCoopStagingDestinationMap) - 1);
    gLocalCoopStagingDestinationMap[sizeof(gLocalCoopStagingDestinationMap) - 1] = '\0';
}

inline bool localCoopConsumeStagingRoomExit()
{
    if (!gLocalCoopStagingRoomActive || gLocalCoopStagingRoomConsumed) return false;
    gLocalCoopStagingRoomConsumed = true;
    gLocalCoopStagingRoomActive = false;
    return true;
}

} // namespace fallout

#endif
