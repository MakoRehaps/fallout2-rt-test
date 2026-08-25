#ifndef UNIFIED_WILDERNESS_GENERATOR_H
#define UNIFIED_WILDERNESS_GENERATOR_H

#include <vector>
#include "debug.h"
#include "map_entry_utils.h"
#include "object.h"
#include "proto.h"
#include "proto_types.h"
#include "unified_campaign.h"
#include "unified_vehicle_system.h"
#include "unified_world_system.h"

namespace fallout {

inline bool unifiedWildernessIsOpenMountainMap(int mapIdx)
{
    return unifiedCampaignGetActiveGame() == UnifiedGameId::Fallout1 && (mapIdx == 49 || mapIdx == 50);
}
inline bool unifiedWildernessObjectIsRemovableMountainBlocker(Object* object)
{
    if (object == nullptr || object == gDude || object->elevation != 0 || object->sid != -1 || isExitGridPid(object->pid))
        return false;
    int type = PID_TYPE(object->pid);
    if (type == OBJ_TYPE_WALL || object->pid == 0x500000C) return true;
    if (type != OBJ_TYPE_SCENERY || (object->flags & OBJECT_NO_BLOCK) != 0) return false;
    Proto* proto = nullptr;
    return protoGetProto(object->pid, &proto) == 0
        && proto->scenery.type == SCENERY_TYPE_GENERIC
        && unifiedVehicleTypeForObject(object) == UnifiedVehicleType::None;
}
inline uint32_t unifiedWildernessCurrentSeed(int mapIdx)
{
    const UnifiedWorldSystemTravelState& travel = unifiedWorldSystemGetStateConst().travel;
    int gi = unifiedWorldSystemGameIndex(unifiedCampaignGetActiveGame());
    const UnifiedWorldSystemCellState* cell = unifiedWorldSystemGetCellConst(
        unifiedCampaignGetActiveGame(), travel.currentCellX[gi], travel.currentCellY[gi]);
    return cell != nullptr ? cell->seed : unifiedWorldSystemMixSeed(static_cast<uint32_t>(mapIdx));
}
inline int unifiedWildernessExitAnchor(UnifiedWorldSystemRoadDirection direction, uint32_t seed)
{
    int offset = static_cast<int>((seed >> (static_cast<int>(direction) * 5)) % 41) - 20;
    switch (direction) {
    case UnifiedWorldSystemRoadDirection::North: return 8 * 200 + 100 + offset;
    case UnifiedWorldSystemRoadDirection::East: return (100 + offset) * 200 + 191;
    case UnifiedWorldSystemRoadDirection::South: return 191 * 200 + 100 + offset;
    case UnifiedWorldSystemRoadDirection::West: return (100 + offset) * 200 + 8;
    }
    return 20100;
}
inline void unifiedWildernessInstallFourExitGrids(uint32_t seed)
{
    std::vector<Object*> exits;
    for (Object* object = objectFindFirstAtElevation(0); object != nullptr; object = objectFindNextAtElevation())
        if (isExitGridPid(object->pid)) exits.push_back(object);

    for (int index = 0; index < 4; index++) {
        Object* exit = index < static_cast<int>(exits.size()) ? exits[index] : nullptr;
        if (exit == nullptr && objectCreateWithPid(&exit, FIRST_EXIT_GRID_PID + index) != 0) {
            debugPrint("[WILDERNESS GEN] missing exit-grid prototype direction=%d\n", index);
            continue;
        }
        int anchor = unifiedWildernessExitAnchor(static_cast<UnifiedWorldSystemRoadDirection>(index), seed);
        int tile = mapEntryFindNearestSafeHex(gDude, anchor, 0, 24, false);
        if (tile == -1) tile = anchor;
        exit->sid = -1;
        exit->data.misc.map = -2;
        exit->data.misc.tile = -1;
        exit->data.misc.elevation = 0;
        exit->data.misc.rotation = 0;
        objectShow(exit, nullptr);
        objectSetLocation(exit, tile, 0, nullptr);
    }
}
inline void unifiedWildernessMaybeGenerateVehicle(uint32_t seed)
{
    if (seed % 100 >= 18) return;
    for (Object* object = objectFindFirst(); object != nullptr; object = objectFindNext())
        if (unifiedVehicleTypeForObject(object) != UnifiedVehicleType::None) return;

    int roll = static_cast<int>((seed >> 8) % 100);
    UnifiedVehicleType requested = roll < 35 ? UnifiedVehicleType::Motorbike
        : roll < 60 ? UnifiedVehicleType::Scout
        : roll < 82 ? UnifiedVehicleType::Buggy
        : roll < 94 ? UnifiedVehicleType::Highwayman : UnifiedVehicleType::Vertibird;
    int pid = -1;
    UnifiedVehicleType actual = requested;
    for (int attempt = 0; attempt < static_cast<int>(UnifiedVehicleType::Count); attempt++) {
        actual = static_cast<UnifiedVehicleType>((static_cast<int>(requested) + attempt)
            % static_cast<int>(UnifiedVehicleType::Count));
        pid = unifiedVehicleFindPrototype(actual);
        if (pid != -1) break;
    }
    if (pid == -1) return;

    int anchor = 20100 + static_cast<int>((seed >> 16) % 31) - 15;
    int tile = mapEntryFindNearestSafeHex(gDude, anchor, 0, 40, false);
    if (tile == -1) return;
    Object* vehicle = nullptr;
    if (objectCreateWithPid(&vehicle, pid) == 0 && vehicle != nullptr) {
        vehicle->sid = -1;
        objectSetLocation(vehicle, tile, 0, nullptr);
        debugPrint("[WILDERNESS GEN] spawned repairable %s pid=%08X tile=%d\n",
            unifiedVehicleName(actual), pid, tile);
    }
}
inline void unifiedWildernessGenerateLoadedMap(int mapIdx)
{
    unifiedVehicleResetEncounterSalvage();
    if (!unifiedWildernessIsOpenMountainMap(mapIdx)) return;

    std::vector<Object*> remove;
    for (Object* object = objectFindFirst(); object != nullptr; object = objectFindNext())
        if (unifiedWildernessObjectIsRemovableMountainBlocker(object)) remove.push_back(object);
    for (Object* object : remove) objectDestroy(object, nullptr);

    uint32_t seed = unifiedWildernessCurrentSeed(mapIdx);
    unifiedWildernessInstallFourExitGrids(seed);
    unifiedWildernessMaybeGenerateVehicle(seed);
    debugPrint("[WILDERNESS GEN] map=%d seed=%08X removed=%d exits=4\n",
        mapIdx, seed, static_cast<int>(remove.size()));
}

} // namespace fallout
#endif
