#ifndef LOCAL_COOP_FPS_WEAPON_H
#define LOCAL_COOP_FPS_WEAPON_H

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "color.h"
#include "combat_defs.h"
#include "debug.h"
#include "item.h"
#include "skill_defs.h"

namespace fallout {

// COOP_FREEDOOM_FIRST_PERSON_WEAPONS_V1
// Reads the BSD-licensed Freedoom WAD installed beside the game and uses only
// its first-person weapon patches as a view-model layer. Fallout remains the
// simulation: the equipped Fallout item chooses the visual family and all
// firing/reloading/damage still go through the existing Fallout combat code.
struct LocalCoopWadLump {
    int offset = 0;
    int size = 0;
};

struct LocalCoopDoomPatch {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> pixels;
    std::vector<unsigned char> opaque;
};

inline bool gLocalCoopFreedoomTriedLoad = false;
inline bool gLocalCoopFreedoomLoaded = false;
inline std::vector<unsigned char> gLocalCoopFreedoomWad;
inline std::unordered_map<std::string, LocalCoopWadLump> gLocalCoopFreedoomLumps;
inline std::unordered_map<std::string, LocalCoopDoomPatch> gLocalCoopFreedoomPatchCache;
inline std::array<unsigned char, 256> gLocalCoopFreedoomPaletteMap {};

inline uint16_t localCoopReadLe16(const unsigned char* p)
{
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

inline uint32_t localCoopReadLe32(const unsigned char* p)
{
    return static_cast<uint32_t>(p[0])
        | (static_cast<uint32_t>(p[1]) << 8)
        | (static_cast<uint32_t>(p[2]) << 16)
        | (static_cast<uint32_t>(p[3]) << 24);
}

inline std::string localCoopWadName(const unsigned char* p)
{
    char name[9] {};
    memcpy(name, p, 8);
    for (int i = 0; i < 8; i++) {
        if (name[i] == '\0') break;
        if (name[i] >= 'a' && name[i] <= 'z') name[i] = static_cast<char>(name[i] - 'a' + 'A');
    }
    return std::string(name);
}

inline bool localCoopReadBinaryFile(const char* path, std::vector<unsigned char>& out)
{
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) return false;
    std::streamsize size = stream.tellg();
    if (size <= 0) return false;
    stream.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(size));
    return stream.read(reinterpret_cast<char*>(out.data()), size).good();
}

inline void localCoopBuildDoomPaletteMap(const unsigned char* doomPalette)
{
    int falloutMax = 0;
    for (int i = 0; i < 768; i++) falloutMax = std::max(falloutMax, static_cast<int>(_systemCmap[i]));
    int falloutScale = falloutMax <= 63 ? 4 : 1;

    for (int src = 0; src < 256; src++) {
        int sr = doomPalette[src * 3 + 0];
        int sg = doomPalette[src * 3 + 1];
        int sb = doomPalette[src * 3 + 2];
        int best = 0;
        int bestDist = 0x7FFFFFFF;
        for (int dst = 0; dst < 256; dst++) {
            int dr = static_cast<int>(_systemCmap[dst * 3 + 0]) * falloutScale - sr;
            int dg = static_cast<int>(_systemCmap[dst * 3 + 1]) * falloutScale - sg;
            int db = static_cast<int>(_systemCmap[dst * 3 + 2]) * falloutScale - sb;
            int dist = dr * dr + dg * dg + db * db;
            if (dist < bestDist) {
                bestDist = dist;
                best = dst;
            }
        }
        gLocalCoopFreedoomPaletteMap[static_cast<size_t>(src)] = static_cast<unsigned char>(best);
    }
}

inline bool localCoopFreedoomLoad()
{
    if (gLocalCoopFreedoomTriedLoad) return gLocalCoopFreedoomLoaded;
    gLocalCoopFreedoomTriedLoad = true;

    const char* candidates[] = {
        "freedoom\\freedoom1.wad",
        "freedoom/freedoom1.wad",
        "freedoom1.wad",
    };
    for (const char* path : candidates) {
        if (localCoopReadBinaryFile(path, gLocalCoopFreedoomWad)) {
            debugPrint("[COOP FPS WEAPON] loaded %s (%d bytes)\n", path, static_cast<int>(gLocalCoopFreedoomWad.size()));
            break;
        }
    }

    const auto& wad = gLocalCoopFreedoomWad;
    if (wad.size() < 12 || (memcmp(wad.data(), "IWAD", 4) != 0 && memcmp(wad.data(), "PWAD", 4) != 0)) {
        debugPrint("[COOP FPS WEAPON] Freedoom WAD missing/invalid; weapon overlay disabled\n");
        return false;
    }

    uint32_t count = localCoopReadLe32(wad.data() + 4);
    uint32_t directory = localCoopReadLe32(wad.data() + 8);
    if (directory > wad.size() || static_cast<uint64_t>(directory) + static_cast<uint64_t>(count) * 16ULL > wad.size()) {
        return false;
    }

    for (uint32_t i = 0; i < count; i++) {
        const unsigned char* entry = wad.data() + directory + i * 16;
        uint32_t offset = localCoopReadLe32(entry);
        uint32_t size = localCoopReadLe32(entry + 4);
        std::string name = localCoopWadName(entry + 8);
        if (static_cast<uint64_t>(offset) + size <= wad.size()) {
            gLocalCoopFreedoomLumps[name] = { static_cast<int>(offset), static_cast<int>(size) };
        }
    }

    auto pal = gLocalCoopFreedoomLumps.find("PLAYPAL");
    if (pal == gLocalCoopFreedoomLumps.end() || pal->second.size < 768) return false;
    localCoopBuildDoomPaletteMap(wad.data() + pal->second.offset);

    // These are the specific view-model families used by the mapping below.
    const char* required[] = { "PUNGA0", "PISGA0", "SHTGA0", "CHGGA0", "MISGA0", "PLSGA0", "BFGGA0", "SAWGA0" };
    bool any = false;
    for (const char* name : required) any = any || gLocalCoopFreedoomLumps.find(name) != gLocalCoopFreedoomLumps.end();
    gLocalCoopFreedoomLoaded = any;
    return gLocalCoopFreedoomLoaded;
}

inline const LocalCoopDoomPatch* localCoopFreedoomPatch(const std::string& name)
{
    auto cached = gLocalCoopFreedoomPatchCache.find(name);
    if (cached != gLocalCoopFreedoomPatchCache.end()) return &cached->second;
    auto lumpIt = gLocalCoopFreedoomLumps.find(name);
    if (lumpIt == gLocalCoopFreedoomLumps.end()) return nullptr;

    const LocalCoopWadLump lump = lumpIt->second;
    if (lump.size < 8) return nullptr;
    const unsigned char* src = gLocalCoopFreedoomWad.data() + lump.offset;
    int width = static_cast<int16_t>(localCoopReadLe16(src));
    int height = static_cast<int16_t>(localCoopReadLe16(src + 2));
    if (width <= 0 || height <= 0 || width > 1024 || height > 1024 || lump.size < 8 + width * 4) return nullptr;

    LocalCoopDoomPatch patch;
    patch.width = width;
    patch.height = height;
    patch.pixels.assign(static_cast<size_t>(width) * height, 0);
    patch.opaque.assign(static_cast<size_t>(width) * height, 0);

    for (int x = 0; x < width; x++) {
        uint32_t columnOffset = localCoopReadLe32(src + 8 + x * 4);
        if (columnOffset >= static_cast<uint32_t>(lump.size)) continue;
        size_t pos = columnOffset;
        while (pos < static_cast<size_t>(lump.size)) {
            unsigned char top = src[pos++];
            if (top == 255) break;
            if (pos + 2 > static_cast<size_t>(lump.size)) break;
            unsigned char length = src[pos++];
            pos++; // unused byte
            if (pos + length + 1 > static_cast<size_t>(lump.size)) break;
            for (int y = 0; y < length; y++) {
                int dy = static_cast<int>(top) + y;
                if (dy >= 0 && dy < height) {
                    size_t di = static_cast<size_t>(dy) * width + x;
                    patch.pixels[di] = gLocalCoopFreedoomPaletteMap[src[pos + y]];
                    patch.opaque[di] = 1;
                }
            }
            pos += length + 1; // pixels + trailing unused byte
        }
    }

    auto result = gLocalCoopFreedoomPatchCache.emplace(name, std::move(patch));
    return &result.first->second;
}

inline const char* localCoopFpsWeaponFamily(Object* actor, int hitMode)
{
    Object* weapon = actor != nullptr ? critterGetWeaponForHitMode(actor, hitMode) : nullptr;
    if (weapon == nullptr) return "PUNG";

    int attackType = weaponGetAttackTypeForHitMode(weapon, hitMode);
    if (attackType == ATTACK_TYPE_UNARMED) return "PUNG";
    if (attackType == ATTACK_TYPE_MELEE) return "SAWG";

    int skill = weaponGetSkillForHitMode(weapon, hitMode);
    if (skill == SKILL_ENERGY_WEAPONS) return weaponIsTwoHanded(weapon) ? "BFGG" : "PLSG";
    if (skill == SKILL_BIG_GUNS) {
        if (weaponGetBurstRounds(weapon) > 1) return "CHGG";
        return "MISG";
    }
    if (skill == SKILL_SMALL_GUNS) {
        if (weaponGetBurstRounds(weapon) > 1) return "CHGG";
        return weaponIsTwoHanded(weapon) ? "SHTG" : "PISG";
    }
    return "PISG";
}

inline int localCoopFpsWeaponFrameCount(const char* family)
{
    if (strcmp(family, "PISG") == 0) return 5;
    if (strcmp(family, "SHTG") == 0) return 4;
    if (strcmp(family, "CHGG") == 0) return 2;
    if (strcmp(family, "MISG") == 0) return 2;
    if (strcmp(family, "PLSG") == 0) return 2;
    if (strcmp(family, "BFGG") == 0) return 3;
    if (strcmp(family, "PUNG") == 0) return 4;
    if (strcmp(family, "SAWG") == 0) return 4;
    return 1;
}

inline bool localCoopFpsWeaponFireHeld(int slot)
{
    if (slot < 0 || slot >= kLocalCoopMaxPlayers) return false;
    const LocalCoopPlayer& player = gLocalCoopPlayers[slot];
    if (player.controller != nullptr
        && SDL_GameControllerGetAxis(player.controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 12000) {
        return true;
    }
    if (slot == 0) {
        Uint32 buttons = SDL_GetMouseState(nullptr, nullptr);
        if ((buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0) return true;
    }
    return false;
}

inline void localCoopFpsDrawWeapon(int slot, unsigned char* dest, int pitch,
    int viewX, int viewY, int viewWidth, int viewHeight)
{
    if (dest == nullptr || viewWidth <= 0 || viewHeight <= 0 || !localCoopFreedoomLoad()) return;
    if (slot < 0 || slot >= kLocalCoopMaxPlayers) return;
    LocalCoopPlayer& player = gLocalCoopPlayers[slot];
    if (!player.connected || !player.humanOwned || player.actor == nullptr) return;

    int hitMode = player.activeHand == HAND_LEFT ? HIT_MODE_LEFT_WEAPON_PRIMARY : HIT_MODE_RIGHT_WEAPON_PRIMARY;
    const char* family = localCoopFpsWeaponFamily(player.actor, hitMode);
    int frameCount = localCoopFpsWeaponFrameCount(family);
    int frameIndex = 0;
    if (localCoopFpsWeaponFireHeld(slot) && frameCount > 1) {
        frameIndex = 1 + static_cast<int>((SDL_GetTicks() / 70) % static_cast<Uint32>(frameCount - 1));
    }

    char lumpName[9] {};
    snprintf(lumpName, sizeof(lumpName), "%s%c0", family, static_cast<char>('A' + frameIndex));
    const LocalCoopDoomPatch* patch = localCoopFreedoomPatch(lumpName);
    if (patch == nullptr && frameIndex != 0) {
        snprintf(lumpName, sizeof(lumpName), "%sA0", family);
        patch = localCoopFreedoomPatch(lumpName);
    }
    if (patch == nullptr) return;

    int drawHeight = std::clamp(viewHeight * 55 / 100, 48, viewHeight * 4 / 5);
    int drawWidth = std::max(1, patch->width * drawHeight / std::max(1, patch->height));
    if (drawWidth > viewWidth * 9 / 10) {
        drawWidth = viewWidth * 9 / 10;
        drawHeight = std::max(1, patch->height * drawWidth / std::max(1, patch->width));
    }
    int startX = viewX + (viewWidth - drawWidth) / 2;
    int startY = viewY + viewHeight - drawHeight;

    for (int dy = 0; dy < drawHeight; dy++) {
        int sy = dy * patch->height / drawHeight;
        int yy = startY + dy;
        if (yy < viewY || yy >= viewY + viewHeight) continue;
        for (int dx = 0; dx < drawWidth; dx++) {
            int sx = dx * patch->width / drawWidth;
            size_t si = static_cast<size_t>(sy) * patch->width + sx;
            if (patch->opaque[si] == 0) continue;
            int xx = startX + dx;
            if (xx < viewX || xx >= viewX + viewWidth) continue;
            dest[yy * pitch + xx] = patch->pixels[si];
        }
    }
}

} // namespace fallout

#endif
