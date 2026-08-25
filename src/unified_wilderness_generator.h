#ifndef UNIFIED_WILDERNESS_GENERATOR_H
#define UNIFIED_WILDERNESS_GENERATOR_H

#include <algorithm>
#include <vector>

#include "debug.h"
#include "map.h"
#include "map_entry_utils.h"
#include "object.h"
#include "proto.h"
#include "proto_types.h"
#include "tile.h"
#include "unified_campaign.h"
#include "unified_vehicle_system.h"
#include "unified_world_system.h"

namespace fallout {

inline bool unifiedWildernessIsOpenMountainMap(UnifiedGameId game, int mapIdx)
{
    return game == UnifiedGameId::Fallout1 && (mapIdx == 49 || mapIdx == 50);
}

inline bool unifiedWildernessObjectIsRemovableMountainBlocker(Object* object)
{
    if (object == nullptr || object == gDude || object->sid != -1 || isExitGridPid(object->pid))
        return false;

    int type = PID_TYPE(object->pid);
    if (type == OBJ_TYPE_WALL) return true;
    if (type != OBJ_TYPE_SCENERY) return false;

    // Preserve any loaded wreck/car art so it remains repairable. Everything
    // else without a script is set dressing on these two wilderness templates
    // and can be regenerated safely.
    return unifiedVehicleTypeForObject(object) == UnifiedVehicleType::None;
}

inline uint32_t unifiedWildernessCurrentSeed(UnifiedGameId game, int mapIdx)
{
    const UnifiedWorldSystemTravelState& travel = unifiedWorldSystemGetStateConst().travel;
    int gi = unifiedWorldSystemGameIndex(game);
    const UnifiedWorldSystemCellState* cell = unifiedWorldSystemGetCell(
        game, travel.currentCellX[gi], travel.currentCellY[gi]);
    return cell != nullptr ? cell->seed : unifiedWorldSystemMixSeed(static_cast<uint32_t>(mapIdx));
}

inline int unifiedWildernessCollectOpenFloorTiles(int elevation, int* floorIds, int capacity)
{
    if (!elevationIsValid(elevation) || _square[elevation] == nullptr || capacity <= 0)
        return 0;

    int counts[4096] {};
    // Sample only the central open/playable portion of the authored template.
    // The edge squares are the mountain artwork we specifically do not want to
    // copy back over the generated wilderness.
    for (int y = 30; y < 70; y++) {
        for (int x = 30; x < 70; x++) {
            int id = _square[elevation]->field_0[y * SQUARE_GRID_WIDTH + x] & 0x0FFF;
            if (id > 1 && id < 4096) counts[id]++;
        }
    }

    int count = 0;
    for (int slot = 0; slot < capacity; slot++) {
        int bestId = -1;
        int bestCount = 0;
        for (int id = 2; id < 4096; id++) {
            if (counts[id] > bestCount) {
                bestId = id;
                bestCount = counts[id];
            }
        }
        if (bestId == -1) break;
        floorIds[count++] = bestId;
        counts[bestId] = 0;
    }
    return count;
}

inline int unifiedWildernessRebuildFloor(int elevation, uint32_t seed)
{
    if ((gMapHeader.flags & (2 << elevation)) != 0 || _square[elevation] == nullptr)
        return 0;

    int floorIds[4] {};
    int floorCount = unifiedWildernessCollectOpenFloorTiles(elevation, floorIds, 4);
    if (floorCount == 0) return 0;

    for (int square = 0; square < SQUARE_GRID_SIZE; square++) {
        uint32_t mixed = unifiedWorldSystemMixSeed(
            seed ^ static_cast<uint32_t>(elevation * 0x45D9F3B)
            ^ static_cast<uint32_t>(square * 0x9E3779B9));
        int floorId = floorIds[mixed % floorCount];
        int old = _square[elevation]->field_0[square];
        _square[elevation]->field_0[square] =
            (old & static_cast<int>(0xFFFF0000))
            | (old & 0xF000)
            | floorId;
    }
    return floorCount;
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

inline void unifiedWildernessInstallFourExitGrids(uint32_t seed, int elevation)
{
    std::vector<Object*> exits;
    for (Object* object = objectFindFirstAtElevation(elevation); object != nullptr; object = objectFindNextAtElevation())
        if (isExitGridPid(object->pid)) exits.push_back(object);

    for (int index = 0; index < 4; index++) {
        Object* exit = index < static_cast<int>(exits.size()) ? exits[index] : nullptr;
        if (exit == nullptr && objectCreateWithPid(&exit, FIRST_EXIT_GRID_PID + index) != 0) {
            debugPrint("[WILDERNESS GEN] missing exit-grid prototype direction=%d elevation=%d\n", index, elevation);
            continue;
        }
        int anchor = unifiedWildernessExitAnchor(static_cast<UnifiedWorldSystemRoadDirection>(index), seed);
        int tile = mapEntryFindNearestSafeHex(gDude, anchor, elevation, 32, false);
        if (tile == -1) tile = anchor;
        exit->sid = -1;
        exit->data.misc.map = -2;
        exit->data.misc.tile = -1;
        exit->data.misc.elevation = 0;
        exit->data.misc.rotation = 0;
        objectShow(exit, nullptr);
        objectSetLocation(exit, tile, elevation, nullptr);
    }
}

inline void unifiedWildernessMaybeGenerateVehicle(uint32_t seed, int elevation)
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
        // Keep the story Highwayman rare; do not use its guaranteed PID as the
        // fallback art for every missing bike/buggy/scout prototype.
        if (actual == UnifiedVehicleType::Highwayman && requested != UnifiedVehicleType::Highwayman)
            continue;
        pid = unifiedVehicleFindPrototype(actual);
        if (pid != -1) break;
    }
    if (pid == -1) return;

    int anchor = 20100 + static_cast<int>((seed >> 16) % 31) - 15;
    int tile = mapEntryFindNearestSafeHex(gDude, anchor, elevation, 40, false);
    if (tile == -1) return;
    Object* vehicle = nullptr;
    if (objectCreateWithPid(&vehicle, pid) == 0 && vehicle != nullptr) {
        vehicle->sid = -1;
        objectSetLocation(vehicle, tile, elevation, nullptr);
        debugPrint("[WILDERNESS GEN] spawned repairable %s pid=%08X tile=%d elevation=%d\n",
            unifiedVehicleName(actual), pid, tile, elevation);
    }
}

inline void unifiedWildernessGenerateLoadedMapForGame(UnifiedGameId game, int mapIdx)
{
    unifiedVehicleResetEncounterSalvage();
    unifiedVehicleIndexLoadedMapPrototypes();

    if (!unifiedWildernessIsOpenMountainMap(game, mapIdx)) return;

    std::vector<Object*> remove;
    for (Object* object = objectFindFirst(); object != nullptr; object = objectFindNext())
        if (unifiedWildernessObjectIsRemovableMountainBlocker(object)) remove.push_back(object);
    for (Object* object : remove) objectDestroy(object, nullptr);

    uint32_t seed = unifiedWildernessCurrentSeed(game, mapIdx);
    int rebuiltElevations = 0;
    int floorVariants = 0;
    for (int elevation = 0; elevation < ELEVATION_COUNT; elevation++) {
        int variants = unifiedWildernessRebuildFloor(elevation, seed);
        if (variants != 0) {
            rebuiltElevations++;
            floorVariants = std::max(floorVariants, variants);
        }
    }

    int entranceElevation = elevationIsValid(gElevation) ? gElevation : 0;
    unifiedWildernessInstallFourExitGrids(seed, entranceElevation);
    unifiedWildernessMaybeGenerateVehicle(seed, entranceElevation);
    tileWindowRefresh();

    debugPrint("[WILDERNESS GEN] game=%d map=%d seed=%08X removed=%d rebuiltElevations=%d floorVariants=%d exits=4 elevation=%d\n",
        static_cast<int>(static_cast<uint32_t>(game)),
        mapIdx,
        seed,
        static_cast<int>(remove.size()),
        rebuiltElevations,
        floorVariants,
        entranceElevation);
}

inline void unifiedWildernessGenerateLoadedMap(int mapIdx)
{
    unifiedWildernessGenerateLoadedMapForGame(unifiedCampaignGetActiveGame(), mapIdx);
}

} // namespace fallout
#endif
