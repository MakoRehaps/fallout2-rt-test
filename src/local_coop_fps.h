#ifndef LOCAL_COOP_FPS_H
#define LOCAL_COOP_FPS_H

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "art.h"
#include "color.h"
#include "draw.h"
#include "local_coop.h"
#include "object.h"
#include "svga.h"
#include "tile.h"
#include "window_manager.h"

namespace fallout {

// COOP_NATIVE_BILLBOARD_FPS_V1
// A second renderer over the SAME live Fallout map/simulation. Nothing is
// teleported or converted into another game mode. World objects keep their
// original FIDs and are projected as distance-scaled billboards.
enum class LocalCoopCameraMode {
    Isometric,
    FirstPerson,
};

struct LocalCoopFpsBillboard {
    Object* object = nullptr;
    float depth = 0.0f;
    float lateral = 0.0f;
    int distance = 0;
};

inline LocalCoopCameraMode gLocalCoopCameraMode = LocalCoopCameraMode::Isometric;
inline int gLocalCoopFpsWindow = -1;
inline Uint32 gLocalCoopFpsNextTurnTick = 0;

inline bool localCoopFpsActive()
{
    return gLocalCoopCameraMode == LocalCoopCameraMode::FirstPerson;
}

inline const char* localCoopCameraModeLabel()
{
    return localCoopFpsActive() ? "FIRST PERSON" : "ISOMETRIC";
}

inline void localCoopFpsDestroyWindow()
{
    if (gLocalCoopFpsWindow != -1) {
        windowDestroy(gLocalCoopFpsWindow);
        gLocalCoopFpsWindow = -1;
    }
}

inline void localCoopFpsSetMode(LocalCoopCameraMode mode)
{
    gLocalCoopCameraMode = mode;
    if (mode == LocalCoopCameraMode::Isometric) {
        localCoopFpsDestroyWindow();
    }
    debugPrint("[COOP CAMERA] mode=%s\n", localCoopCameraModeLabel());
}

inline void localCoopFpsToggle()
{
    localCoopFpsSetMode(localCoopFpsActive()
        ? LocalCoopCameraMode::Isometric
        : LocalCoopCameraMode::FirstPerson);
}

inline bool localCoopFpsProject(Object* camera, Object* object, float& depth, float& lateral)
{
    if (camera == nullptr || object == nullptr || object->tile < 0 || camera->tile < 0) return false;

    int cx = 0;
    int cy = 0;
    int ox = 0;
    int oy = 0;
    if (tileToScreenXY(camera->tile, &cx, &cy, camera->elevation) != 0
        || tileToScreenXY(object->tile, &ox, &oy, object->elevation) != 0) {
        return false;
    }

    int forwardTile = tileGetTileInDirection(camera->tile, camera->rotation, 1);
    int fx = cx;
    int fy = cy - 1;
    if (tileIsValid(forwardTile)) tileToScreenXY(forwardTile, &fx, &fy, camera->elevation);

    float fdx = static_cast<float>(fx - cx);
    float fdy = static_cast<float>(fy - cy);
    float flen = std::sqrt(fdx * fdx + fdy * fdy);
    if (flen < 0.5f) return false;
    fdx /= flen;
    fdy /= flen;

    float rdx = static_cast<float>(ox - cx);
    float rdy = static_cast<float>(oy - cy);
    depth = rdx * fdx + rdy * fdy;
    lateral = rdx * (-fdy) + rdy * fdx;

    // Roughly a 100 degree horizontal field of view. The map remains authoritative;
    // this merely decides which original sprites are visible in the FPS projection.
    return depth > 3.0f && std::fabs(lateral) <= depth * 1.18f;
}

inline void localCoopFpsCollect(std::vector<LocalCoopFpsBillboard>& out)
{
    out.clear();
    Object* camera = gLocalCoopPlayers[0].actor;
    if (camera == nullptr) return;

    for (Object* object = objectFindFirst(); object != nullptr; object = objectFindNext()) {
        if (object == camera
            || object->elevation != camera->elevation
            || object->tile < 0
            || (object->flags & OBJECT_HIDDEN) != 0) {
            continue;
        }

        int type = FID_TYPE(object->fid);
        if (type == OBJ_TYPE_TILE || type == OBJ_TYPE_INTERFACE || type == OBJ_TYPE_INVENTORY
            || type == OBJ_TYPE_HEAD || type == OBJ_TYPE_BACKGROUND || type == OBJ_TYPE_SKILLDEX) {
            continue;
        }

        int distance = tileDistanceBetween(camera->tile, object->tile);
        if (distance <= 0 || distance > 32) continue;

        float depth = 0.0f;
        float lateral = 0.0f;
        if (!localCoopFpsProject(camera, object, depth, lateral)) continue;

        out.push_back({ object, depth, lateral, distance });
    }

    std::sort(out.begin(), out.end(), [](const LocalCoopFpsBillboard& a, const LocalCoopFpsBillboard& b) {
        return a.depth > b.depth;
    });
}

inline void localCoopFpsDrawBillboard(const LocalCoopFpsBillboard& billboard, unsigned char* dest, int width, int height)
{
    Object* object = billboard.object;
    if (object == nullptr || dest == nullptr) return;

    CacheEntry* handle = nullptr;
    Art* art = artLock(object->fid, &handle);
    if (art == nullptr) return;

    int direction = std::clamp(object->rotation, 0, ROTATION_COUNT - 1);
    int frameCount = std::max(1, artGetFrameCount(art));
    int frame = std::clamp(object->frame, 0, frameCount - 1);
    int srcWidth = artGetWidth(art, frame, direction);
    int srcHeight = artGetHeight(art, frame, direction);
    unsigned char* src = artGetFrameData(art, frame, direction);

    if (src != nullptr && srcWidth > 0 && srcHeight > 0) {
        float distanceScale = 7.5f / static_cast<float>(std::max(1, billboard.distance));
        int drawHeight = std::clamp(static_cast<int>(srcHeight * distanceScale), 10, height * 3 / 4);
        int drawWidth = std::max(4, srcWidth * drawHeight / std::max(1, srcHeight));
        int centerX = width / 2 + static_cast<int>((billboard.lateral / billboard.depth) * width * 0.44f);
        int x = centerX - drawWidth / 2;
        int y = height / 2 + height / 5 - drawHeight;

        if (x >= 0 && y >= 0 && x + drawWidth < width && y + drawHeight < height) {
            blitBufferToBufferStretchTrans(src, srcWidth, srcHeight, srcWidth,
                dest + y * width + x, drawWidth, drawHeight, width);
        }
    }

    artUnlock(handle);
}

inline void localCoopFpsProcessLook()
{
    LocalCoopPlayer& p1 = gLocalCoopPlayers[0];
    if (!localCoopFpsActive() || p1.actor == nullptr) return;

    int turn = 0;
    if (p1.controller != nullptr) {
        int rx = SDL_GameControllerGetAxis(p1.controller, SDL_CONTROLLER_AXIS_RIGHTX);
        if (rx > 15000) turn = 1;
        else if (rx < -15000) turn = -1;
    }

    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    if (keys != nullptr) {
        if (keys[SDL_SCANCODE_E]) turn = 1;
        if (keys[SDL_SCANCODE_Q]) turn = -1;
    }

    Uint32 now = SDL_GetTicks();
    if (turn != 0 && static_cast<Sint32>(now - gLocalCoopFpsNextTurnTick) >= 0) {
        int rotation = (p1.actor->rotation + (turn > 0 ? 1 : ROTATION_COUNT - 1)) % ROTATION_COUNT;
        objectSetRotation(p1.actor, rotation, nullptr);
        gLocalCoopFpsNextTurnTick = now + 95;
    }
}

inline void localCoopFpsTick()
{
    if (!localCoopFpsActive()) {
        localCoopFpsDestroyWindow();
        return;
    }

    Object* camera = gLocalCoopPlayers[0].actor;
    if (camera == nullptr || gLocalCoopPlayers[0].uiMode != LocalCoopUiMode::World) return;

    localCoopFpsProcessLook();

    int width = screenGetWidth();
    int height = screenGetVisibleHeight();
    if (width <= 0 || height <= 0) return;

    if (gLocalCoopFpsWindow == -1) {
        gLocalCoopFpsWindow = windowCreate(0, 0, width, height, _colorTable[0], WINDOW_MOVE_ON_TOP);
        if (gLocalCoopFpsWindow == -1) return;
    }

    unsigned char* buffer = windowGetBuffer(gLocalCoopFpsWindow);
    if (buffer == nullptr) return;

    // Sky/ceiling and floor. Every map object above this backdrop is drawn from
    // the original FRM currently owned by the live object.
    windowFill(gLocalCoopFpsWindow, 0, 0, width, height / 2, _colorTable[21140]);
    windowFill(gLocalCoopFpsWindow, 0, height / 2, width, height - height / 2, _colorTable[4228]);

    std::vector<LocalCoopFpsBillboard> billboards;
    localCoopFpsCollect(billboards);
    for (const auto& billboard : billboards) localCoopFpsDrawBillboard(billboard, buffer, width, height);

    int cx = width / 2;
    int cy = height / 2;
    windowDrawLine(gLocalCoopFpsWindow, cx - 8, cy, cx + 8, cy, _colorTable[32747]);
    windowDrawLine(gLocalCoopFpsWindow, cx, cy - 8, cx, cy + 8, _colorTable[32747]);
    windowDrawText(gLocalCoopFpsWindow, "FPS VIEW  Q/E OR RIGHT STICK: TURN", width - 20, 10, height - 24, _colorTable[992]);
    windowRefresh(gLocalCoopFpsWindow);
}

} // namespace fallout

#endif
