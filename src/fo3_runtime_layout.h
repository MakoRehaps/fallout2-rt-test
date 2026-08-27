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

// Header-only on purpose: fps_limiter.cc is already compiled on every target,
// so the FO3 runtime hook needs no new CMake source entry.

namespace fo3rt {

struct WallRun {
    int x;
    int y;
    int length;
    int rotation;
    bool horizontal;
};

struct DoorPlacement {
    int x;
    int y;
    int rotation;
};

static inline int makePid(int type, int id)
{
    return (type << 24) | (id & 0x00FFFFFF);
}

static inline int squareToHexTile(int x, int y)
{
    // F3O plans use the classic 100x100 square grid. Runtime objects use the
    // 200x200 hex grid. Put each planned point near the middle of its 2x2 hex
    // region. This keeps generated walls aligned with generated square tiles.
    int hx = std::max(0, std::min(HEX_GRID_WIDTH - 1, x * 2 + 1));
    int hy = std::max(0, std::min(HEX_GRID_HEIGHT - 1, y * 2 + 1));
    return hy * HEX_GRID_WIDTH + hx;
}

static inline int sideRotation(const char* side)
{
    if (strcmp(side, "east") == 0) return ROTATION_E;
    if (strcmp(side, "south") == 0) return ROTATION_SE;
    if (strcmp(side, "west") == 0) return ROTATION_W;
    return ROTATION_NE;
}

static inline int findFirstWallPid()
{
    int maxId = proto_max_id(OBJ_TYPE_WALL);
    for (int id = 1; id <= maxId; id++) {
        int pid = makePid(OBJ_TYPE_WALL, id);
        Proto* proto = nullptr;
        if (protoGetProto(pid, &proto) == 0 && proto != nullptr) {
            return pid;
        }
    }
    return -1;
}

static inline int findFirstDoorPid()
{
    int maxId = proto_max_id(OBJ_TYPE_SCENERY);
    for (int id = 1; id <= maxId; id++) {
        int pid = makePid(OBJ_TYPE_SCENERY, id);
        Proto* proto = nullptr;
        if (protoGetProto(pid, &proto) != 0 || proto == nullptr) {
            continue;
        }
        if (proto->scenery.type == SCENERY_TYPE_DOOR) {
            return pid;
        }
    }
    return -1;
}

static inline void spawnObject(int pid, int x, int y, int rotation)
{
    if (pid == -1) {
        return;
    }

    Object* object = nullptr;
    if (objectCreateWithPid(&object, pid) != 0 || object == nullptr) {
        return;
    }

    // Generated runtime geometry is reconstructed from the F3O every time the
    // map is entered. Do not serialize it into SAV files or it would duplicate.
    object->flags |= OBJECT_NO_SAVE;

    int tile = squareToHexTile(x, y);
    if (objectSetLocation(object, tile, 0, nullptr) != 0) {
        objectDestroy(object, nullptr);
        return;
    }
    objectSetRotation(object, rotation, nullptr);
}

static inline void loadLayoutForCurrentMap()
{
    char base[16];
    strncpy(base, gMapHeader.name, sizeof(base) - 1);
    base[sizeof(base) - 1] = '\0';
    char* dot = strchr(base, '.');
    if (dot != nullptr) {
        *dot = '\0';
    }

    if (strncmp(base, "F3M", 3) != 0) {
        return;
    }

    char path[64];
    snprintf(path, sizeof(path), "MAPS\\%s.F3O", base);
    File* stream = fileOpen(path, "rt");
    if (stream == nullptr) {
        debugPrint("\nFO3 runtime: no sidecar %s", path);
        return;
    }

    std::vector<WallRun> walls;
    std::vector<DoorPlacement> doors;
    std::set<int> doorSquares;

    char line[512];
    while (fileReadString(line, sizeof(line), stream) != nullptr) {
        char sid[96];
        char side[32];
        int x;
        int y;
        int length;

        if (sscanf(line, "WALL_RUN %95s %31s %d %d %d", sid, side, &x, &y, &length) == 5) {
            WallRun run;
            run.x = x;
            run.y = y;
            run.length = std::max(0, length);
            run.rotation = sideRotation(side);
            run.horizontal = strcmp(side, "north") == 0 || strcmp(side, "south") == 0;
            walls.push_back(run);
            continue;
        }

        if (sscanf(line, "DOOR %95s %d %d %31s", sid, &x, &y, side) == 4) {
            DoorPlacement door;
            door.x = x;
            door.y = y;
            door.rotation = sideRotation(side);
            doors.push_back(door);
            doorSquares.insert(y * SQUARE_GRID_WIDTH + x);
        }
    }
    fileClose(stream);

    int wallPid = findFirstWallPid();
    int doorPid = findFirstDoorPid();
    int spawnedWalls = 0;
    int spawnedDoors = 0;

    if (wallPid == -1) {
        debugPrint("\nFO3 runtime: no wall prototype available");
    }
    if (doorPid == -1) {
        debugPrint("\nFO3 runtime: no door prototype available");
    }

    for (const WallRun& run : walls) {
        for (int i = 0; i < run.length; i++) {
            int x = run.x + (run.horizontal ? i : 0);
            int y = run.y + (run.horizontal ? 0 : i);
            if (x < 0 || x >= SQUARE_GRID_WIDTH || y < 0 || y >= SQUARE_GRID_HEIGHT) {
                continue;
            }
            if (doorSquares.count(y * SQUARE_GRID_WIDTH + x) != 0) {
                continue;
            }
            if (wallPid != -1) {
                spawnObject(wallPid, x, y, run.rotation);
                spawnedWalls++;
            }
        }
    }

    for (const DoorPlacement& door : doors) {
        if (doorPid != -1) {
            spawnObject(doorPid, door.x, door.y, door.rotation);
            spawnedDoors++;
        }
    }

    debugPrint("\nFO3 runtime: %s spawned %d walls, %d doors", base, spawnedWalls, spawnedDoors);
}

static inline void tick()
{
    static std::string lastMap;

    std::string current = gMapHeader.name;
    if (current == lastMap) {
        return;
    }

    lastMap = current;
    if (!current.empty()) {
        loadLayoutForCurrentMap();
    }
}

} // namespace fo3rt

static inline void fo3RuntimeLayoutTick()
{
    fo3rt::tick();
}

} // namespace fallout

#endif // FO3_RUNTIME_LAYOUT_H
