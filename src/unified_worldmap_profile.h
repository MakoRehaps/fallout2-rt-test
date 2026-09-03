#ifndef UNIFIED_WORLDMAP_PROFILE_H
#define UNIFIED_WORLDMAP_PROFILE_H

#include <cstddef>
#include <cstring>
#include <cstdio>

#include "unified_campaign.h"

namespace fallout {

// Stock Fallout 2 CE world-map query symbols. Wrappers are defined before the
// call-site remap macros below, so fallback calls continue to target the stock
// Fallout 2 world-map implementation.
int wmMapMaxCount();
int wmMapIdxToName(int mapIdx, char* dest, size_t size);
int wmMapMatchNameToIdx(char* name);
bool wmMapIdxIsSaveable(int mapIdx);
int wmMatchAreaContainingMapIdx(int mapIdx, int* areaIdxPtr);

inline constexpr int kUnifiedFallout1MapCount = 66;
inline constexpr int kUnifiedFallout1TownCount = 12;

// Order is the original Fallout 1 MAP_* enum order from fallout1-ce's
// worldmap.h. Names are the actual map basenames used by the original engine.
inline constexpr const char* kUnifiedFallout1MapNames[kUnifiedFallout1MapCount] = {
    "desert1", "desert2", "desert3", "hallded", "hotel", "watrshd",
    "vault13", "vaultent", "vaultbur", "vaultnec", "junkent", "junkcsno",
    "junkkill", "brohdent", "brohd12", "brohd34", "caves", "childrn1",
    "childrn2", "city1", "coast1", "coast2", "colatruk", "fsauser",
    "raiders", "shadye", "shadyw", "glowent", "laadytum", "lafollwr",
    "mbent", "mbstrg12", "mbvats12", "mstrlr12", "mstrlr34", "v13ent",
    "hubent", "dethclaw", "hubdwntn", "hubheigt", "huboldtn", "hubwater",
    "glow1", "glow2", "lablades", "laripper", "lagunrun", "childead",
    "mbdead", "mountn1", "mountn2", "foot", "tardis", "talkcow",
    "usedcar", "brodead", "descrvn1", "descrvn2", "mntcrvn1", "mntcrvn2",
    "vipers", "descrvn3", "mntcrvn3", "descrvn4", "mntcrvn4", "hubmis1",
};

inline char unifiedWorldmapAsciiLower(char ch)
{
    if (ch >= 'A' && ch <= 'Z') {
        return static_cast<char>(ch - 'A' + 'a');
    }
    return ch;
}

inline bool unifiedWorldmapNameEquals(const char* lhs, const char* rhs)
{
    if (lhs == nullptr || rhs == nullptr) {
        return false;
    }

    while (*lhs != '\0' && *rhs != '\0') {
        if (unifiedWorldmapAsciiLower(*lhs) != unifiedWorldmapAsciiLower(*rhs)) {
            return false;
        }
        lhs++;
        rhs++;
    }

    // Accept the common "name.map" form as equivalent to the stock basename.
    if (*lhs == '\0' && *rhs == '.') {
        return unifiedWorldmapAsciiLower(rhs[1]) == 'm'
            && unifiedWorldmapAsciiLower(rhs[2]) == 'a'
            && unifiedWorldmapAsciiLower(rhs[3]) == 'p'
            && rhs[4] == '\0';
    }
    if (*rhs == '\0' && *lhs == '.') {
        return unifiedWorldmapAsciiLower(lhs[1]) == 'm'
            && unifiedWorldmapAsciiLower(lhs[2]) == 'a'
            && unifiedWorldmapAsciiLower(lhs[3]) == 'p'
            && lhs[4] == '\0';
    }

    return *lhs == '\0' && *rhs == '\0';
}

inline int unifiedFallout1MapTown(int mapIdx)
{
    switch (mapIdx) {
    // Vault 13.
    case 6:  // VAULT13
    case 35: // V13ENT
        return 0;

    // Vault 15.
    case 7: // VAULTENT
    case 8: // VAULTBUR
        return 1;

    // Shady Sands, including the radscorpion caves.
    case 16: // CAVES
    case 25: // SHADYE
    case 26: // SHADYW
        return 2;

    // Junktown.
    case 10:
    case 11:
    case 12:
        return 3;

    // Raiders.
    case 24:
        return 4;

    // Necropolis.
    case 3:
    case 4:
    case 5:
    case 9:
        return 5;

    // The Hub.
    case 36:
    case 37:
    case 38:
    case 39:
    case 40:
    case 41:
    case 65:
        return 6;

    // Brotherhood of Steel.
    case 13:
    case 14:
    case 15:
    case 55:
        return 7;

    // Military Base.
    case 30:
    case 31:
    case 32:
    case 48:
        return 8;

    // The Glow.
    case 27:
    case 42:
    case 43:
        return 9;

    // Boneyard.
    case 28:
    case 29:
    case 44:
    case 45:
    case 46:
        return 10;

    // Cathedral / Master complex.
    case 17:
    case 18:
    case 33:
    case 34:
    case 47:
        return 11;

    default:
        return -1;
    }
}

inline int unifiedWmMapMaxCount()
{
    if (unifiedCampaignGetActiveGame() == UnifiedGameId::Fallout1) {
        return kUnifiedFallout1MapCount;
    }
    return wmMapMaxCount();
}

inline int unifiedWmMapIdxToName(int mapIdx, char* dest, size_t size)
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmMapIdxToName(mapIdx, dest, size);
    }

    if (dest == nullptr || size == 0 || mapIdx < 0 || mapIdx >= kUnifiedFallout1MapCount) {
        if (dest != nullptr && size != 0) {
            dest[0] = '\0';
        }
        return -1;
    }

    // mapLoadByName opens the exact path returned here. Fallout 2's table
    // includes the .MAP suffix, so preserve that contract for Fallout 1 too.
    const char* name = kUnifiedFallout1MapNames[mapIdx];
    int written = std::snprintf(dest, size, "%s.MAP", name);
    if (written < 0 || static_cast<size_t>(written) >= size) {
        dest[0] = '\0';
        return -1;
    }
    return 0;
}

inline int unifiedWmMapMatchNameToIdx(char* name)
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmMapMatchNameToIdx(name);
    }

    if (name == nullptr) {
        return -1;
    }

    for (int index = 0; index < kUnifiedFallout1MapCount; index++) {
        if (unifiedWorldmapNameEquals(kUnifiedFallout1MapNames[index], name)) {
            return index;
        }
    }

    return -1;
}

inline bool unifiedWmMapIdxIsSaveable(int mapIdx)
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmMapIdxIsSaveable(mapIdx);
    }

    // Fallout 1 persists its fixed town/location maps. Wilderness, caravan and
    // special/random encounter maps are temporary and should not participate in
    // F2CE's same-city saved-map machinery.
    return mapIdx >= 0
        && mapIdx < kUnifiedFallout1MapCount
        && unifiedFallout1MapTown(mapIdx) != -1;
}

inline int unifiedWmMatchAreaContainingMapIdx(int mapIdx, int* areaIdxPtr)
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmMatchAreaContainingMapIdx(mapIdx, areaIdxPtr);
    }

    if (areaIdxPtr == nullptr) {
        return -1;
    }

    int town = unifiedFallout1MapTown(mapIdx);
    if (town == -1) {
        *areaIdxPtr = -1;
        return -1;
    }

    *areaIdxPtr = town;
    return 0;
}

} // namespace fallout

// worldmap.cc includes worldmap.h before any transitive map.h include. In that
// translation unit WORLD_MAP_H is therefore already defined: keep the stock F2
// function definitions unrenamed. Loaded-map/gameplay callers that enter through
// map.h first receive the profile remaps below.
#ifndef WORLD_MAP_H
#define wmMapMaxCount unifiedWmMapMaxCount
#define wmMapIdxToName unifiedWmMapIdxToName
#define wmMapMatchNameToIdx unifiedWmMapMatchNameToIdx
#define wmMapIdxIsSaveable unifiedWmMapIdxIsSaveable
#define wmMatchAreaContainingMapIdx unifiedWmMatchAreaContainingMapIdx
#endif

#endif /* UNIFIED_WORLDMAP_PROFILE_H */
