#ifndef FO3_RUNTIME_LAYOUT_H
#define FO3_RUNTIME_LAYOUT_H

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "db.h"
#include "debug.h"
#include "map.h"
#include "map_defs.h"
#include "obj_types.h"
#include "object.h"
#include "proto.h"
#include "proto_types.h"

namespace fallout {

struct Fo3RuntimeWallRun {
    int x;
    int y;
    int length;
    int rotation;
    bool horizontal;
};

struct Fo3RuntimeDoorPlacement {
    int x;
    int y;
    int rotation;
};

struct Fo3RuntimeAnchorPlacement {
    int x;
    int y;
    std::string role;
};

struct Fo3RuntimeExitPlacement {
    int x;
    int y;
    int targetMap;
    int targetTile;
    int targetElevation;
    int targetRotation;
};

static inline int fo3RuntimeMakePid(int type, int id)
{
    return (type << 24) | (id & 0x00FFFFFF);
}

static inline int fo3RuntimeSquareToHexTile(int x, int y)
{
    int hx = std::max(0, std::min(HEX_GRID_WIDTH - 1, x * 2 + 1));
    int hy = std::max(0, std::min(HEX_GRID_HEIGHT - 1, y * 2 + 1));
    return hy * HEX_GRID_WIDTH + hx;
}

static inline int fo3RuntimeSideRotation(const char* side)
{
    if (strcmp(side, "east") == 0) {
        return ROTATION_E;
    }

    if (strcmp(side, "south") == 0) {
        return ROTATION_SE;
    }

    if (strcmp(side, "west") == 0) {
        return ROTATION_W;
    }

    return ROTATION_NE;
}

static inline std::vector<int> fo3RuntimePreferredMaterials(const std::string& category)
{
    if (category == "vault") {
        return { MATERIAL_TYPE_METAL, MATERIAL_TYPE_CEMENT, MATERIAL_TYPE_STONE };
    }

    if (category == "industrial") {
        return { MATERIAL_TYPE_METAL, MATERIAL_TYPE_CEMENT, MATERIAL_TYPE_STONE };
    }

    if (category == "urban") {
        return {
            MATERIAL_TYPE_CEMENT,
            MATERIAL_TYPE_STONE,
            MATERIAL_TYPE_METAL,
            MATERIAL_TYPE_WOOD,
        };
    }

    if (category == "cave") {
        return { MATERIAL_TYPE_STONE, MATERIAL_TYPE_DIRT, MATERIAL_TYPE_CEMENT };
    }

    return {
        MATERIAL_TYPE_WOOD,
        MATERIAL_TYPE_METAL,
        MATERIAL_TYPE_DIRT,
        MATERIAL_TYPE_STONE,
        MATERIAL_TYPE_CEMENT,
    };
}

static inline int fo3RuntimeMaterialRank(int material, const std::vector<int>& preferred)
{
    for (int index = 0; index < static_cast<int>(preferred.size()); index++) {
        if (preferred[index] == material) {
            return index;
        }
    }

    return 100;
}

static inline int fo3RuntimeFindWallPid(const std::string& category)
{
    std::vector<int> preferred = fo3RuntimePreferredMaterials(category);
    int bestPid = -1;
    int bestRank = 999;
    int maxId = proto_max_id(OBJ_TYPE_WALL);

    for (int id = 1; id <= maxId; id++) {
        int pid = fo3RuntimeMakePid(OBJ_TYPE_WALL, id);
        Proto* proto = nullptr;
        if (protoGetProto(pid, &proto) != 0 || proto == nullptr) {
            continue;
        }

        int rank = fo3RuntimeMaterialRank(proto->wall.material, preferred);
        if (rank < bestRank) {
            bestRank = rank;
            bestPid = pid;
            if (rank == 0) {
                break;
            }
        }
    }

    return bestPid;
}

static inline int fo3RuntimeFindDoorPid(const std::string& category)
{
    std::vector<int> preferred = fo3RuntimePreferredMaterials(category);
    int bestPid = -1;
    int bestRank = 999;
    int maxId = proto_max_id(OBJ_TYPE_SCENERY);

    for (int id = 1; id <= maxId; id++) {
        int pid = fo3RuntimeMakePid(OBJ_TYPE_SCENERY, id);
        Proto* proto = nullptr;
        if (protoGetProto(pid, &proto) != 0
            || proto == nullptr
            || proto->scenery.type != SCENERY_TYPE_DOOR) {
            continue;
        }

        int rank = fo3RuntimeMaterialRank(proto->scenery.field_2C, preferred);
        if (rank < bestRank) {
            bestRank = rank;
            bestPid = pid;
            if (rank == 0) {
                break;
            }
        }
    }

    return bestPid;
}

static inline int fo3RuntimeFindContainerPid(const std::string& category)
{
    std::vector<int> preferred = fo3RuntimePreferredMaterials(category);
    int bestPid = -1;
    int bestRank = 999;
    int maxId = proto_max_id(OBJ_TYPE_ITEM);

    for (int id = 1; id <= maxId; id++) {
        int pid = fo3RuntimeMakePid(OBJ_TYPE_ITEM, id);
        Proto* proto = nullptr;
        if (protoGetProto(pid, &proto) != 0
            || proto == nullptr
            || proto->item.type != ITEM_TYPE_CONTAINER) {
            continue;
        }

        int rank = fo3RuntimeMaterialRank(proto->item.material, preferred);
        if (rank < bestRank) {
            bestRank = rank;
            bestPid = pid;
            if (rank == 0) {
                break;
            }
        }
    }

    return bestPid;
}

static inline bool fo3RuntimeSpawnObject(
    int pid,
    int x,
    int y,
    int rotation,
    Object** outObject = nullptr)
{
    if (pid == -1) {
        return false;
    }

    Object* object = nullptr;
    if (objectCreateWithPid(&object, pid) != 0 || object == nullptr) {
        return false;
    }

    object->flags |= OBJECT_NO_SAVE;

    int tile = fo3RuntimeSquareToHexTile(x, y);
    if (objectSetLocation(object, tile, 0, nullptr) != 0) {
        objectDestroy(object, nullptr);
        return false;
    }

    objectSetRotation(object, rotation, nullptr);
    if (outObject != nullptr) {
        *outObject = object;
    }

    return true;
}

static inline bool fo3RuntimeSpawnExit(const Fo3RuntimeExitPlacement& exit)
{
    Object* object = nullptr;
    if (!fo3RuntimeSpawnObject(
            FIRST_EXIT_GRID_PID,
            exit.x,
            exit.y,
            ROTATION_NE,
            &object)
        || object == nullptr) {
        return false;
    }

    object->data.misc.map = exit.targetMap;
    object->data.misc.tile = exit.targetTile;
    object->data.misc.elevation = exit.targetElevation;
    object->data.misc.rotation = exit.targetRotation;
    return true;
}

static inline void fo3RuntimeLoadCurrentMapLayout()
{
    char base[16];
    strncpy(base, gMapHeader.name, sizeof(base) - 1);
    base[sizeof(base) - 1] = '\0';

    char* dot = strchr(base, '.');
    if (dot != nullptr) {
        *dot = '\0';
    }

    // F3M = generated surface, F3U = generated underground/metro.
    if (strncmp(base, "F3", 2) != 0) {
        return;
    }

    char path[64];
    snprintf(path, sizeof(path), "MAPS\\%s.F3O", base);
    File* stream = fileOpen(path, "rt");
    if (stream == nullptr) {
        debugPrint("\nFO3 runtime: no sidecar %s", path);
        return;
    }

    std::string category = "wasteland";
    std::vector<Fo3RuntimeWallRun> walls;
    std::vector<Fo3RuntimeDoorPlacement> doors;
    std::vector<Fo3RuntimeAnchorPlacement> anchors;
    std::vector<Fo3RuntimeExitPlacement> exits;
    std::set<int> doorSquares;

    char line[512];
    while (fileReadString(line, sizeof(line), stream) != nullptr) {
        char sid[96];
        char side[32];
        char word[96];
        int x;
        int y;
        int length;

        if (sscanf(line, "CATEGORY %95s", word) == 1) {
            category = word;
            continue;
        }

        if (sscanf(
                line,
                "WALL_RUN %95s %31s %d %d %d",
                sid,
                side,
                &x,
                &y,
                &length)
            == 5) {
            Fo3RuntimeWallRun run;
            run.x = x;
            run.y = y;
            run.length = std::max(0, length);
            run.rotation = fo3RuntimeSideRotation(side);
            run.horizontal = strcmp(side, "north") == 0 || strcmp(side, "south") == 0;
            walls.push_back(run);
            continue;
        }

        if (sscanf(line, "DOOR %95s %d %d %31s", sid, &x, &y, side) == 4) {
            Fo3RuntimeDoorPlacement door;
            door.x = x;
            door.y = y;
            door.rotation = fo3RuntimeSideRotation(side);
            doors.push_back(door);
            doorSquares.insert(y * SQUARE_GRID_WIDTH + x);
            continue;
        }

        if (sscanf(line, "ANCHOR %95s %95s %d %d", sid, word, &x, &y) == 4) {
            Fo3RuntimeAnchorPlacement anchor;
            anchor.x = x;
            anchor.y = y;
            anchor.role = word;
            anchors.push_back(anchor);
            continue;
        }

        int targetMap;
        int targetTile;
        int targetElevation;
        int targetRotation;
        if (sscanf(
                line,
                "EXIT %31s %d %d %d %d %d %d",
                side,
                &x,
                &y,
                &targetMap,
                &targetTile,
                &targetElevation,
                &targetRotation)
            == 7) {
            if (targetMap >= 0
                && hexGridTileIsValid(targetTile)
                && elevationIsValid(targetElevation)) {
                Fo3RuntimeExitPlacement exit;
                exit.x = x;
                exit.y = y;
                exit.targetMap = targetMap;
                exit.targetTile = targetTile;
                exit.targetElevation = targetElevation;
                exit.targetRotation = std::max(
                    0,
                    std::min(ROTATION_COUNT - 1, targetRotation));
                exits.push_back(exit);
            }
        }
    }

    fileClose(stream);

    int wallPid = fo3RuntimeFindWallPid(category);
    int doorPid = fo3RuntimeFindDoorPid(category);
    int containerPid = fo3RuntimeFindContainerPid(category);
    int spawnedWalls = 0;
    int spawnedDoors = 0;
    int spawnedContainers = 0;
    int spawnedExits = 0;

    for (const Fo3RuntimeWallRun& run : walls) {
        for (int index = 0; index < run.length; index++) {
            int x = run.x + (run.horizontal ? index : 0);
            int y = run.y + (run.horizontal ? 0 : index);
            if (x < 0 || x >= SQUARE_GRID_WIDTH
                || y < 0 || y >= SQUARE_GRID_HEIGHT) {
                continue;
            }

            if (doorSquares.count(y * SQUARE_GRID_WIDTH + x) != 0) {
                continue;
            }

            if (fo3RuntimeSpawnObject(wallPid, x, y, run.rotation)) {
                spawnedWalls++;
            }
        }
    }

    for (const Fo3RuntimeDoorPlacement& door : doors) {
        if (fo3RuntimeSpawnObject(doorPid, door.x, door.y, door.rotation)) {
            spawnedDoors++;
        }
    }

    for (const Fo3RuntimeAnchorPlacement& anchor : anchors) {
        if (anchor.role == "loot"
            && fo3RuntimeSpawnObject(containerPid, anchor.x, anchor.y, ROTATION_NE)) {
            spawnedContainers++;
        }
    }

    for (const Fo3RuntimeExitPlacement& exit : exits) {
        if (fo3RuntimeSpawnExit(exit)) {
            spawnedExits++;
        }
    }

    debugPrint(
        "\nFO3 runtime: %s category=%s walls=%d doors=%d containers=%d exits=%d "
        "wallPid=%08X doorPid=%08X containerPid=%08X",
        base,
        category.c_str(),
        spawnedWalls,
        spawnedDoors,
        spawnedContainers,
        spawnedExits,
        wallPid,
        doorPid,
        containerPid);
}

static inline void fo3RuntimeLayoutTick()
{
    static std::string lastMap;
    std::string current = gMapHeader.name;
    if (current == lastMap) {
        return;
    }

    lastMap = current;
    if (!current.empty()) {
        fo3RuntimeLoadCurrentMapLayout();
    }
}

} // namespace fallout

#endif // FO3_RUNTIME_LAYOUT_H
