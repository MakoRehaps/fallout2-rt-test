#ifndef UNIFIED_FOMAP_ENCOUNTER_LAYOUT_H
#define UNIFIED_FOMAP_ENCOUNTER_LAYOUT_H

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "art.h"
#include "debug.h"
#include "map.h"
#include "obj_types.h"
#include "random.h"
#include "tile.h"

namespace fallout {

struct UnifiedFomapTilePlacement {
    bool roof = false;
    int x = 0;
    int y = 0;
    std::string frm;
};

inline std::unordered_map<std::string, int> gUnifiedFomapTileIds;
inline bool gUnifiedFomapTileIdsReady = false;
inline bool gUnifiedFomapMissingRootReported = false;

inline std::string unifiedFomapLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

inline std::string unifiedFomapBaseName(const std::string& value)
{
    std::string normalized = value;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    std::size_t slash = normalized.find_last_of('/');
    if (slash != std::string::npos) {
        normalized.erase(0, slash + 1);
    }
    return unifiedFomapLower(normalized);
}

inline void unifiedFomapBuildTileIndex()
{
    if (gUnifiedFomapTileIdsReady) {
        return;
    }

    gUnifiedFomapTileIdsReady = true;
    char name[32];
    for (int id = 0; id < 4096; id++) {
        if (artCopyFileName(OBJ_TYPE_TILE, id, name) != 0) {
            break;
        }
        gUnifiedFomapTileIds[unifiedFomapBaseName(name)] = id;
    }
}

inline int unifiedFomapResolveTile(const std::string& frm)
{
    unifiedFomapBuildTileIndex();
    auto it = gUnifiedFomapTileIds.find(unifiedFomapBaseName(frm));
    return it != gUnifiedFomapTileIds.end() ? it->second : -1;
}

inline const char* unifiedFomapCategoryForLoadedMap()
{
    std::string name = unifiedFomapLower(gMapHeader.name);
    if (name.rfind("desert", 0) == 0 || name.rfind("mountn", 0) == 0) {
        return "desert";
    }
    if (name.rfind("city", 0) == 0) {
        return "city";
    }
    if (name.rfind("coast", 0) == 0) {
        return "coast";
    }
    return nullptr;
}

inline int unifiedFomapLayoutCount(const char* category)
{
    if (category == nullptr) return 0;
    if (std::strcmp(category, "desert") == 0) return 12;
    if (std::strcmp(category, "city") == 0) return 9;
    if (std::strcmp(category, "coast") == 0) return 11;
    return 0;
}

inline int unifiedFomapLayoutNumber(const char* category, int ordinal)
{
    if (std::strcmp(category, "coast") == 0 && ordinal >= 4) {
        // coast5 is a two-level authored map and is deliberately excluded.
        return ordinal + 2;
    }
    return ordinal + 1;
}

inline bool unifiedFomapOpenLayout(const char* category, int ordinal, std::ifstream& stream, std::string& path)
{
    int number = unifiedFomapLayoutNumber(category, ordinal);
    char relative[192];
    std::snprintf(relative,
        sizeof(relative),
        "GameData/EncounterLayouts/Fallout2/RE/%s%d.fomap",
        category,
        number);
    path = relative;
    stream.open(path);
    if (stream.is_open()) {
        return true;
    }

    // Also support launching from a build/bin directory one level below the
    // installed game root during developer testing.
    std::snprintf(relative,
        sizeof(relative),
        "../GameData/EncounterLayouts/Fallout2/RE/%s%d.fomap",
        category,
        number);
    path = relative;
    stream.clear();
    stream.open(path);
    return stream.is_open();
}

inline bool unifiedFomapParseTiles(std::istream& stream, std::vector<UnifiedFomapTilePlacement>& placements)
{
    placements.clear();
    bool inTiles = false;
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line == "[Tiles]") {
            inTiles = true;
            continue;
        }
        if (inTiles && !line.empty() && line.front() == '[') {
            break;
        }
        if (!inTiles || line.empty()) {
            continue;
        }

        char kind[16] = {};
        char frm[320] = {};
        int x = 0;
        int y = 0;
        if (std::sscanf(line.c_str(), "%15s %d %d %319s", kind, &x, &y, frm) != 4) {
            continue;
        }
        bool roof = std::strcmp(kind, "roof") == 0;
        if (!roof && std::strcmp(kind, "tile") != 0) {
            continue;
        }
        // MAP2FOMAP emits original 100x100 square tiles at even FOnline hex
        // coordinates: X=(square%100)*2, Y=(square/100)*2.
        if ((x & 1) != 0 || (y & 1) != 0 || x < 0 || y < 0 || x >= 200 || y >= 200) {
            continue;
        }
        placements.push_back({ roof, x / 2, y / 2, frm });
    }
    return !placements.empty();
}

inline bool unifiedFomapApplyTiles(const std::vector<UnifiedFomapTilePlacement>& placements,
    const std::string& sourcePath)
{
    if (_square[0] == nullptr || placements.empty()) {
        return false;
    }

    struct ResolvedPlacement {
        bool roof;
        int square;
        int tileId;
    };
    std::vector<ResolvedPlacement> resolved;
    resolved.reserve(placements.size());
    int missing = 0;

    for (const auto& placement : placements) {
        int tileId = unifiedFomapResolveTile(placement.frm);
        if (tileId < 0 || tileId > 0x0FFF) {
            missing++;
            continue;
        }
        resolved.push_back({
            placement.roof,
            placement.y * SQUARE_GRID_WIDTH + placement.x,
            tileId,
        });
    }

    // Do not partially paint a layout from a different asset set. This is what
    // makes the F2 reference archive safe to use while the active profile may
    // be Fallout 1: either nearly all referenced tile art exists, or the stock
    // CE map is left completely untouched.
    if (resolved.empty() || missing * 10 > static_cast<int>(placements.size())) {
        debugPrint("[FOMAP LAYOUT] skipped incompatible path=%s resolved=%d missing=%d\n",
            sourcePath.c_str(),
            static_cast<int>(resolved.size()),
            missing);
        return false;
    }

    constexpr int kEmptyTile = 1;
    constexpr int kEmptyPacked = kEmptyTile | (kEmptyTile << 16);
    for (int square = 0; square < SQUARE_GRID_SIZE; square++) {
        _square[0]->field_0[square] = kEmptyPacked;
    }

    for (const auto& placement : resolved) {
        int old = _square[0]->field_0[placement.square];
        if (placement.roof) {
            _square[0]->field_0[placement.square] =
                (old & 0x0000FFFF) | (placement.tileId << 16);
        } else {
            _square[0]->field_0[placement.square] =
                (old & static_cast<int>(0xFFFF0000)) | placement.tileId;
        }
    }

    tileWindowRefresh();
    debugPrint("[FOMAP LAYOUT] applied path=%s placements=%d missing=%d map=%d\n",
        sourcePath.c_str(),
        static_cast<int>(resolved.size()),
        missing,
        gMapHeader.field_34);
    return true;
}

inline bool unifiedFomapEncounterApplyForLoadedMap()
{
    const char* category = unifiedFomapCategoryForLoadedMap();
    int count = unifiedFomapLayoutCount(category);
    if (count <= 0) {
        return false;
    }

    int start = randomBetween(0, count - 1);
    for (int attempt = 0; attempt < count; attempt++) {
        int ordinal = (start + attempt) % count;
        std::ifstream stream;
        std::string path;
        if (!unifiedFomapOpenLayout(category, ordinal, stream, path)) {
            continue;
        }

        std::vector<UnifiedFomapTilePlacement> placements;
        if (unifiedFomapParseTiles(stream, placements)
            && unifiedFomapApplyTiles(placements, path)) {
            return true;
        }
    }

    if (!gUnifiedFomapMissingRootReported) {
        gUnifiedFomapMissingRootReported = true;
        debugPrint("[FOMAP LAYOUT] no compatible prepared %s layouts; stock map retained\n",
            category != nullptr ? category : "encounter");
    }
    return false;
}

} // namespace fallout

#endif /* UNIFIED_FOMAP_ENCOUNTER_LAYOUT_H */
