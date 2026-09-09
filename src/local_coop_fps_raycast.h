#ifndef LOCAL_COOP_FPS_RAYCAST_H
#define LOCAL_COOP_FPS_RAYCAST_H

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "color.h"
#include "object.h"
#include "tile.h"

namespace fallout {

// COOP_REAL_RAYCAST_FREEDOOM_V1
// Real per-column collision ray pass over the authoritative Fallout map.
// Geometry still comes from Fallout's live blocking/sight objects. Freedoom is
// only a redistributable FPS texture fallback; actors/items stay Fallout FRM
// billboards and all simulation remains Fallout's.
inline bool gLocalCoopFreedoomTriedLoad = false;
inline bool gLocalCoopFreedoomTextureReady = false;
inline std::array<unsigned char, 64 * 64> gLocalCoopFreedoomFlat {};
// COOP_FPS_RAYCAST_DIAGNOSTICS_V2
inline int gLocalCoopFpsRayHitColumns = 0;
inline int gLocalCoopFpsRayLastBlockerFid = -1;

struct LocalCoopWadDirEntry {
    int32_t filePos;
    int32_t size;
    char name[8];
};

inline bool localCoopWadNameEquals(const char name[8], const char* wanted)
{
    char temp[9] = {};
    memcpy(temp, name, 8);
    return std::strncmp(temp, wanted, 8) == 0;
}

inline unsigned char localCoopNearestFalloutPalette(unsigned char r, unsigned char g, unsigned char b)
{
    int best = 0;
    int bestError = 0x7FFFFFFF;
    for (int index = 0; index < 256; index++) {
        int fr = static_cast<int>(_cmap[index * 3 + 0]) * 4;
        int fg = static_cast<int>(_cmap[index * 3 + 1]) * 4;
        int fb = static_cast<int>(_cmap[index * 3 + 2]) * 4;
        int dr = fr - static_cast<int>(r);
        int dg = fg - static_cast<int>(g);
        int db = fb - static_cast<int>(b);
        int error = dr * dr + dg * dg + db * db;
        if (error < bestError) {
            bestError = error;
            best = index;
        }
    }
    return static_cast<unsigned char>(best);
}

inline bool localCoopLoadFreedoomFlatFromWad(const char* path)
{
    FILE* stream = std::fopen(path, "rb");
    if (stream == nullptr) return false;

    char magic[4] = {};
    int32_t lumpCount = 0;
    int32_t directoryOffset = 0;
    if (std::fread(magic, 1, 4, stream) != 4
        || std::fread(&lumpCount, sizeof(lumpCount), 1, stream) != 1
        || std::fread(&directoryOffset, sizeof(directoryOffset), 1, stream) != 1
        || (std::memcmp(magic, "IWAD", 4) != 0 && std::memcmp(magic, "PWAD", 4) != 0)
        || lumpCount <= 0 || lumpCount > 100000 || directoryOffset <= 0) {
        std::fclose(stream);
        return false;
    }

    if (std::fseek(stream, directoryOffset, SEEK_SET) != 0) {
        std::fclose(stream);
        return false;
    }

    std::vector<LocalCoopWadDirEntry> entries(static_cast<size_t>(lumpCount));
    if (std::fread(entries.data(), sizeof(LocalCoopWadDirEntry), entries.size(), stream) != entries.size()) {
        std::fclose(stream);
        return false;
    }

    int playpalIndex = -1;
    int flatIndex = -1;
    bool insideFlats = false;
    for (int index = 0; index < lumpCount; index++) {
        const LocalCoopWadDirEntry& entry = entries[index];
        if (localCoopWadNameEquals(entry.name, "PLAYPAL")) playpalIndex = index;
        if (localCoopWadNameEquals(entry.name, "F_START") || localCoopWadNameEquals(entry.name, "FF_START")) {
            insideFlats = true;
            continue;
        }
        if (localCoopWadNameEquals(entry.name, "F_END") || localCoopWadNameEquals(entry.name, "FF_END")) {
            insideFlats = false;
            continue;
        }
        if (insideFlats && entry.size == 4096 && flatIndex == -1) flatIndex = index;
    }

    if (playpalIndex < 0 || flatIndex < 0 || entries[playpalIndex].size < 768) {
        std::fclose(stream);
        return false;
    }

    std::array<unsigned char, 768> doomPalette {};
    std::array<unsigned char, 4096> doomFlat {};
    if (std::fseek(stream, entries[playpalIndex].filePos, SEEK_SET) != 0
        || std::fread(doomPalette.data(), 1, doomPalette.size(), stream) != doomPalette.size()
        || std::fseek(stream, entries[flatIndex].filePos, SEEK_SET) != 0
        || std::fread(doomFlat.data(), 1, doomFlat.size(), stream) != doomFlat.size()) {
        std::fclose(stream);
        return false;
    }
    std::fclose(stream);

    std::array<unsigned char, 256> remap {};
    for (int index = 0; index < 256; index++) {
        remap[index] = localCoopNearestFalloutPalette(
            doomPalette[index * 3 + 0],
            doomPalette[index * 3 + 1],
            doomPalette[index * 3 + 2]);
    }
    for (size_t index = 0; index < doomFlat.size(); index++) {
        gLocalCoopFreedoomFlat[index] = remap[doomFlat[index]];
    }

    debugPrint("[COOP FPS] loaded Freedoom raycast texture from %s\n", path);
    return true;
}

inline void localCoopEnsureFreedoomTexture()
{
    if (gLocalCoopFreedoomTriedLoad) return;
    gLocalCoopFreedoomTriedLoad = true;

    const char* candidates[] = {
        "freedoom\\freedoom2.wad",
        "freedoom/freedoom2.wad",
        "..\\..\\freedoom\\freedoom2.wad",
        "../../freedoom/freedoom2.wad",
        "freedoom\\freedoom1.wad",
        "../../freedoom/freedoom1.wad",
    };
    for (const char* candidate : candidates) {
        if (localCoopLoadFreedoomFlatFromWad(candidate)) {
            gLocalCoopFreedoomTextureReady = true;
            return;
        }
    }
    debugPrint("[COOP FPS] Freedoom WAD not found; raycaster using fallback wall shade\n");
}

inline bool localCoopFpsRayBlocker(Object* camera, int tile, int elevation, Object*& blocker)
{
    blocker = _obj_sight_blocking_at(camera, tile, elevation);
    if (blocker == nullptr) return false;
    int type = FID_TYPE(blocker->fid);
    return type == OBJ_TYPE_WALL || type == OBJ_TYPE_SCENERY;
}

inline std::vector<float> localCoopFpsRaycastWalls(Object* camera,
    unsigned char* dest,
    int pitch,
    int viewX,
    int viewY,
    int viewWidth,
    int viewHeight)
{
    std::vector<float> depth(static_cast<size_t>(std::max(0, viewWidth)), 1000000.0f);
    gLocalCoopFpsRayHitColumns = 0;
    gLocalCoopFpsRayLastBlockerFid = -1;
    if (camera == nullptr || dest == nullptr || viewWidth <= 0 || viewHeight <= 0 || !tileIsValid(camera->tile)) {
        return depth;
    }

    localCoopEnsureFreedoomTexture();

    int cx = 0;
    int cy = 0;
    if (tileToScreenXY(camera->tile, &cx, &cy, camera->elevation) != 0) return depth;
    int forwardTile = tileGetTileInDirection(camera->tile, camera->rotation, 1);
    int fx = cx;
    int fy = cy - 16;
    if (tileIsValid(forwardTile)) tileToScreenXY(forwardTile, &fx, &fy, camera->elevation);

    float forwardX = static_cast<float>(fx - cx);
    float forwardY = static_cast<float>(fy - cy);
    float forwardLength = std::sqrt(forwardX * forwardX + forwardY * forwardY);
    if (forwardLength < 0.5f) return depth;
    forwardX /= forwardLength;
    forwardY /= forwardLength;
    float rightX = -forwardY;
    float rightY = forwardX;

    for (int column = 0; column < viewWidth; column++) {
        float cameraPlane = ((static_cast<float>(column) + 0.5f) / static_cast<float>(viewWidth) - 0.5f) * 1.55f;
        float rayX = forwardX + rightX * cameraPlane;
        float rayY = forwardY + rightY * cameraPlane;
        float rayLength = std::sqrt(rayX * rayX + rayY * rayY);
        if (rayLength < 0.001f) continue;
        rayX /= rayLength;
        rayY /= rayLength;

        int lastTile = camera->tile;
        Object* hitObject = nullptr;
        float hitDistance = 0.0f;
        for (float travel = 8.0f; travel <= 1050.0f; travel += 4.0f) {
            int sx = static_cast<int>(std::lround(static_cast<float>(cx) + rayX * travel));
            int sy = static_cast<int>(std::lround(static_cast<float>(cy) + rayY * travel));
            int tile = tileFromScreenXY(sx, sy, camera->elevation, true);
            if (!tileIsValid(tile) || tile == lastTile) continue;
            lastTile = tile;
            if (localCoopFpsRayBlocker(camera, tile, camera->elevation, hitObject)) {
                float fishEye = std::max(0.25f, rayX * forwardX + rayY * forwardY);
                hitDistance = travel * fishEye;
                break;
            }
        }

        if (hitObject == nullptr || hitDistance <= 0.0f) continue;
        gLocalCoopFpsRayHitColumns++;
        gLocalCoopFpsRayLastBlockerFid = hitObject->fid;
        depth[static_cast<size_t>(column)] = hitDistance;

        float tileDistance = std::max(0.5f, hitDistance / std::max(8.0f, forwardLength));
        int wallHeight = std::clamp(static_cast<int>(viewHeight * 1.55f / (tileDistance + 0.45f)), 6, viewHeight);
        int top = viewY + (viewHeight - wallHeight) / 2;
        int bottom = top + wallHeight;
        int screenX = viewX + column;
        int textureX = (column * 64 / std::max(1, viewWidth) + hitObject->tile * 13) & 63;

        for (int y = top; y < bottom; y++) {
            int textureY = ((y - top) * 64 / std::max(1, wallHeight)) & 63;
            unsigned char color = gLocalCoopFreedoomTextureReady
                ? gLocalCoopFreedoomFlat[textureY * 64 + textureX]
                : _colorTable[10570];
            dest[y * pitch + screenX] = color;
        }
    }

    return depth;
}

} // namespace fallout

#endif
