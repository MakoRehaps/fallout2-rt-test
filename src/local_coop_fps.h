#ifndef LOCAL_COOP_FPS_H
#define LOCAL_COOP_FPS_H

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "art.h"
#include "color.h"
#include "draw.h"
#include "kb.h"
#include "local_coop.h"
#include "local_coop_fps_raycast.h"
#include "local_coop_fps_weapon.h"
#include "object.h"
#include "svga.h"
#include "tile.h"
#include "window_manager.h"

namespace fallout {

// COOP_NATIVE_BILLBOARD_FPS_V1
// COOP_FOUR_INDEPENDENT_FPS_CAMERAS_V2
// COOP_REAL_RAYCAST_FREEDOOM_RUNTIME_V1
// A second renderer over the SAME live Fallout map/simulation. Nothing is
// teleported or converted into another game mode. In first-person mode every
// joined player owns one camera viewport following that player's actor.
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

struct LocalCoopFpsViewport {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

inline LocalCoopCameraMode gLocalCoopCameraMode = LocalCoopCameraMode::Isometric;
inline int gLocalCoopFpsWindow = -1;
inline bool gLocalCoopFpsToggleWasDown = false;
inline std::array<Uint32, kLocalCoopMaxPlayers> gLocalCoopFpsNextTurnTick {};

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

inline LocalCoopFpsViewport localCoopFpsViewportForSlot(int slot, int width, int height)
{
    int halfW = width / 2;
    int halfH = height / 2;
    LocalCoopFpsViewport view;
    view.x = (slot & 1) != 0 ? halfW : 0;
    view.y = slot >= 2 ? halfH : 0;
    view.width = (slot & 1) != 0 ? width - halfW : halfW;
    view.height = slot >= 2 ? height - halfH : halfH;
    return view;
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
    return depth > 3.0f && std::fabs(lateral) <= depth * 1.18f;
}

inline void localCoopFpsCollect(Object* camera, std::vector<LocalCoopFpsBillboard>& out)
{
    out.clear();
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

inline void localCoopFpsDrawBillboard(const LocalCoopFpsBillboard& billboard,
    unsigned char* dest,
    int pitch,
    const LocalCoopFpsViewport& view,
    const std::vector<float>& wallDepth)
{
    Object* object = billboard.object;
    if (object == nullptr || dest == nullptr || view.width <= 0 || view.height <= 0) return;

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
        float distanceScale = 6.0f / static_cast<float>(std::max(1, billboard.distance));
        int drawHeight = std::clamp(static_cast<int>(srcHeight * distanceScale), 8, view.height * 3 / 4);
        int drawWidth = std::max(3, srcWidth * drawHeight / std::max(1, srcHeight));
        int centerX = view.x + view.width / 2
            + static_cast<int>((billboard.lateral / billboard.depth) * view.width * 0.44f);
        int localColumn = centerX - view.x;
        if (localColumn >= 0 && localColumn < static_cast<int>(wallDepth.size())
            && billboard.depth > wallDepth[static_cast<size_t>(localColumn)] + 6.0f) {
            artUnlock(handle);
            return;
        }
        int x = centerX - drawWidth / 2;
        int y = view.y + view.height / 2 + view.height / 5 - drawHeight;

        if (x >= view.x && y >= view.y
            && x + drawWidth < view.x + view.width
            && y + drawHeight < view.y + view.height) {
            blitBufferToBufferStretchTrans(src, srcWidth, srcHeight, srcWidth,
                dest + y * pitch + x, drawWidth, drawHeight, pitch);
        }
    }

    artUnlock(handle);
}

inline void localCoopFpsProcessLook(int slot)
{
    if (slot < 0 || slot >= kLocalCoopMaxPlayers) return;
    LocalCoopPlayer& player = gLocalCoopPlayers[slot];
    if (!localCoopFpsActive() || player.actor == nullptr || player.uiMode != LocalCoopUiMode::World) return;

    int turn = 0;
    if (player.controller != nullptr) {
        int rx = SDL_GameControllerGetAxis(player.controller, SDL_CONTROLLER_AXIS_RIGHTX);
        if (rx > 15000) turn = 1;
        else if (rx < -15000) turn = -1;
    }

    // Keyboard look belongs to P1; every controller has its own right-stick camera.
    if (slot == 0) {
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        if (keys != nullptr) {
            if (keys[SDL_SCANCODE_E]) turn = 1;
            if (keys[SDL_SCANCODE_Q]) turn = -1;
        }
    }

    Uint32 now = SDL_GetTicks();
    if (turn != 0 && static_cast<Sint32>(now - gLocalCoopFpsNextTurnTick[slot]) >= 0) {
        int rotation = (player.actor->rotation + (turn > 0 ? 1 : ROTATION_COUNT - 1)) % ROTATION_COUNT;
        objectSetRotation(player.actor, rotation, nullptr);
        gLocalCoopFpsNextTurnTick[slot] = now + 95;
    }
}

inline void localCoopFpsDrawViewport(int slot, unsigned char* buffer, int pitch, int screenWidth, int screenHeight)
{
    LocalCoopFpsViewport view = localCoopFpsViewportForSlot(slot, screenWidth, screenHeight);
    LocalCoopPlayer& player = gLocalCoopPlayers[slot];

    windowFill(gLocalCoopFpsWindow, view.x, view.y, view.width, view.height, _colorTable[0]);
    if (!player.connected || !player.humanOwned || player.actor == nullptr) {
        char label[64];
        snprintf(label, sizeof(label), "P%d - WAITING FOR PLAYER", slot + 1);
        windowDrawText(gLocalCoopFpsWindow, label, view.width - 20, view.x + 10, view.y + 14, _colorTable[992]);
        return;
    }

    // Each quadrant has its own sky/floor and projects from its own actor.
    windowFill(gLocalCoopFpsWindow, view.x, view.y, view.width, view.height / 2, _colorTable[21140]);
    windowFill(gLocalCoopFpsWindow, view.x, view.y + view.height / 2, view.width,
        view.height - view.height / 2, _colorTable[4228]);

    // Cast one collision ray per viewport column through the real Fallout map.
    // Walls/scenery stop the ray; Freedoom supplies a BSD-licensed texture.
    std::vector<float> wallDepth = localCoopFpsRaycastWalls(
        player.actor, buffer, pitch, view.x, view.y, view.width, view.height);

    std::vector<LocalCoopFpsBillboard> billboards;
    localCoopFpsCollect(player.actor, billboards);
    for (const auto& billboard : billboards) {
        localCoopFpsDrawBillboard(billboard, buffer, pitch, view, wallDepth);
    }

    // COOP_FREEDOOM_FIRST_PERSON_WEAPONS_RUNTIME_V1
    // Draw the equipped player's first-person weapon after world geometry and
    // billboards so it behaves like a classic FPS view model.
    localCoopFpsDrawWeapon(slot, buffer, pitch, view.x, view.y, view.width, view.height);

    int cx = view.x + view.width / 2;
    int cy = view.y + view.height / 2;
    windowDrawLine(gLocalCoopFpsWindow, cx - 6, cy, cx + 6, cy, _colorTable[32747]);
    windowDrawLine(gLocalCoopFpsWindow, cx, cy - 6, cx, cy + 6, _colorTable[32747]);

    char label[96];
    snprintf(label, sizeof(label), "P%d FPS  %s", slot + 1, slot == 0 ? "MAP LEADER" : "FOLLOWER");
    windowDrawText(gLocalCoopFpsWindow, label, view.width - 20, view.x + 10, view.y + 10, _colorTable[992]);
}

inline void localCoopFpsTick()
{
    // COOP_FPS_F9_TOGGLE_V2
    // Use the engine-maintained physical key state. SDL_GetKeyboardState can
    // remain stale in this runtime because Fallout owns/pumps the input loop.
    // Keep SDL as a fallback for backends where it is current.
    bool toggleDown = gPressedPhysicalKeys[SDL_SCANCODE_F9] != 0;
    if (!toggleDown) {
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        toggleDown = keys != nullptr && keys[SDL_SCANCODE_F9] != 0;
    }
    if (toggleDown && !gLocalCoopFpsToggleWasDown) {
        debugPrint("[COOP CAMERA] F9 pressed\n");
        localCoopFpsToggle();
    }
    gLocalCoopFpsToggleWasDown = toggleDown;

    if (!localCoopFpsActive()) {
        localCoopFpsDestroyWindow();
        return;
    }

    int width = screenGetWidth();
    int height = screenGetVisibleHeight();
    if (width <= 0 || height <= 0) return;

    if (gLocalCoopFpsWindow == -1) {
        gLocalCoopFpsWindow = windowCreate(0, 0, width, height, _colorTable[0], WINDOW_MOVE_ON_TOP);
        if (gLocalCoopFpsWindow == -1) return;
    }

    unsigned char* buffer = windowGetBuffer(gLocalCoopFpsWindow);
    if (buffer == nullptr) return;

    for (int slot = 0; slot < kLocalCoopMaxPlayers; slot++) {
        localCoopFpsProcessLook(slot);
        localCoopFpsDrawViewport(slot, buffer, width, width, height);
    }

    int halfW = width / 2;
    int halfH = height / 2;
    windowDrawLine(gLocalCoopFpsWindow, halfW, 0, halfW, height - 1, _colorTable[992]);
    windowDrawLine(gLocalCoopFpsWindow, 0, halfH, width - 1, halfH, _colorTable[992]);
    windowRefresh(gLocalCoopFpsWindow);
}

} // namespace fallout

#endif
