#ifndef LOCAL_COOP_STAGING_ROOM_H
#define LOCAL_COOP_STAGING_ROOM_H

#include <cstring>

#include "object.h"
#include "platform_compat.h"
#include "proto_types.h"

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

// COOP_PREOPENING_STAGING_SAFE_V1
inline void localCoopSanitizeStagingRoom()
{
    if (!gLocalCoopStagingRoomActive) return;

    // The original V13ENT map is reused only as a temporary preparation room.
    // Hide its stock critters so players can join, inspect kits, equip and learn
    // controls without the opening being interrupted by cave combat. The real
    // start map is reloaded from disk after the opening, so its normal actors are
    // not permanently changed.
    for (Object* object = objectFindFirst(); object != nullptr; object = objectFindNext()) {
        if (object == gDude || object->pid == -1 || PID_TYPE(object->pid) != OBJ_TYPE_CRITTER) {
            continue;
        }
        object->flags |= OBJECT_HIDDEN | OBJECT_NO_BLOCK;
    }
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
