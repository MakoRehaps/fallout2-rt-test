#ifndef UNIFIED_RESOURCE_RESOLVER_H
#define UNIFIED_RESOURCE_RESOLVER_H

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "platform_compat.h"
#include "unified_resource_origin.h"
#include "xfile.h"

namespace fallout {

struct UnifiedResourceDatasetMount {
    std::string rootSnapshot;
    std::vector<std::string> orderedPaths;
};

inline std::array<UnifiedResourceDatasetMount, 2> gUnifiedResourceDatasetMounts;

inline size_t unifiedResourceDatasetIndex(UnifiedGameId game)
{
    return game == UnifiedGameId::Fallout1 ? 0u : 1u;
}

inline std::string unifiedResourceJoinPath(const std::string& root, const char* relative)
{
    if (root.empty()) {
        return std::string();
    }

    std::string path = root;
    if (path.back() != '/' && path.back() != '\\') {
#if defined(_WIN32)
        path.push_back('\\');
#else
        path.push_back('/');
#endif
    }

    if (relative != nullptr) {
        path.append(relative);
    }
    return path;
}

inline void unifiedResourceAppendExistingPath(std::vector<std::string>& paths, const std::string& path)
{
    if (!path.empty() && compat_access(path.c_str(), 0) == 0) {
        paths.push_back(path);
    }
}

inline void unifiedResourceRebuildDatasetMount(UnifiedGameId game)
{
    UnifiedResourceDatasetMount& mount = gUnifiedResourceDatasetMounts[unifiedResourceDatasetIndex(game)];
    const std::string& root = unifiedCampaignGetRoot(game);

    if (mount.rootSnapshot == root && !mount.orderedPaths.empty()) {
        return;
    }

    mount.rootSnapshot = root;
    mount.orderedPaths.clear();
    if (root.empty()) {
        return;
    }

    // xbaseOpen moves an existing base to the head. These are deliberately
    // stored bottom-up: promoting in this order yields the final precedence
    //
    //   loose data > newest patch DAT > older patch DAT > f2_res > critter >
    //   master > root directory.
    //
    // The root directory is also mounted so original root-level sound/music and
    // other engine support files from both games remain addressable.
    unifiedResourceAppendExistingPath(mount.orderedPaths, root);
    unifiedResourceAppendExistingPath(mount.orderedPaths, unifiedResourceJoinPath(root, "master.dat"));
    unifiedResourceAppendExistingPath(mount.orderedPaths, unifiedResourceJoinPath(root, "critter.dat"));
    unifiedResourceAppendExistingPath(mount.orderedPaths, unifiedResourceJoinPath(root, "f2_res.dat"));

    for (int patchIndex = 0; patchIndex < 1000; patchIndex++) {
        char patchName[32];
        std::snprintf(patchName, sizeof(patchName), "patch%03d.dat", patchIndex);
        unifiedResourceAppendExistingPath(mount.orderedPaths, unifiedResourceJoinPath(root, patchName));
    }

    unifiedResourceAppendExistingPath(mount.orderedPaths, unifiedResourceJoinPath(root, "data"));
}

inline void unifiedResourcePromoteDataset(UnifiedGameId game)
{
    unifiedResourceRebuildDatasetMount(game);
    UnifiedResourceDatasetMount& mount = gUnifiedResourceDatasetMounts[unifiedResourceDatasetIndex(game)];

    for (const std::string& path : mount.orderedPaths) {
        // Opening an already-mounted xbase simply moves it to the head. Opening
        // a not-yet-mounted one adds it. The DAT guard selects DAT1 vs DAT2 from
        // the archive path/root, so both container formats coexist safely.
        xbaseOpen(path.c_str());
    }
}

inline bool unifiedResourceIsRelativePath(const char* filePath)
{
    if (filePath == nullptr || *filePath == '\0') {
        return false;
    }

    char drive[COMPAT_MAX_DRIVE];
    char dir[COMPAT_MAX_DIR];
    compat_splitpath(filePath, drive, dir, nullptr, nullptr);
    return drive[0] == '\0'
        && dir[0] != '\\'
        && dir[0] != '/'
        && dir[0] != '.';
}

inline bool unifiedResourceIsReadOnlyMode(const char* mode)
{
    if (mode == nullptr || std::strchr(mode, 'r') == nullptr) {
        return false;
    }

    return std::strchr(mode, 'w') == nullptr
        && std::strchr(mode, 'a') == nullptr
        && std::strchr(mode, '+') == nullptr;
}

inline std::string unifiedResourceNormalizeLookupPath(const char* filePath)
{
    std::string normalized = filePath != nullptr ? filePath : "";
    std::replace(normalized.begin(), normalized.end(), '/', '\\');
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    while (normalized.size() >= 2 && normalized[0] == '.' && normalized[1] == '\\') {
        normalized.erase(0, 2);
    }
    return normalized;
}

inline bool unifiedResourceEndsWith(const std::string& value, const char* suffix)
{
    if (suffix == nullptr) {
        return false;
    }

    size_t suffixLength = std::strlen(suffix);
    return value.size() >= suffixLength
        && value.compare(value.size() - suffixLength, suffixLength, suffix) == 0;
}

inline bool unifiedResourceUsesFallout2EngineLayer(const char* filePath)
{
    std::string path = unifiedResourceNormalizeLookupPath(filePath);

    // The fused executable is the Fallout 2 engine, so shared engine-facing UI
    // is deliberately F2-owned. World/campaign art remains origin-aware. This
    // also means artInit obtains the F2 interface/inventory/skilldex LSTs, so
    // hard-coded F2 UI FIDs (notably the character editor) are indexed against
    // the table they were authored for instead of an F1 list with colliding IDs.
    if (path.rfind("art\\intrface\\", 0) == 0
        || path.rfind("art\\inven\\", 0) == 0
        || path.rfind("art\\skilldex\\", 0) == 0) {
        return true;
    }

    // These tables belong to the Fallout 2-derived engine framework. They are
    // not Fallout 1 campaign content. Keeping them F2-owned removes the old
    // installer stubs and prevents the F2 parsers from consuming F1-era or
    // incomplete compatibility files. F1 companions/ending content will be
    // represented in the unified systems rather than by swapping this framework
    // table out underneath the engine.
    if (path == "data\\ai.txt"
        || path == "data\\party.txt"
        || path == "data\\enddeath.txt") {
        return true;
    }

    // Engine modal text follows the same rule. Campaign dialogue, maps, scripts,
    // protos and world-map messages are intentionally NOT listed here and keep
    // using their originating game's namespace.
    return unifiedResourceEndsWith(path, "game\\editor.msg")
        || unifiedResourceEndsWith(path, "game\\options.msg")
        || unifiedResourceEndsWith(path, "game\\inventry.msg")
        || unifiedResourceEndsWith(path, "game\\skilldex.msg");
}

inline XFile* unifiedResourceXfileOpen(const char* filePath, const char* mode)
{
    if (unifiedCampaignIsEnabled()
        && unifiedResourceIsRelativePath(filePath)
        && unifiedResourceIsReadOnlyMode(mode)) {
        UnifiedGameId preferred = unifiedResourceUsesFallout2EngineLayer(filePath)
            ? UnifiedGameId::Fallout2
            : unifiedResourceGetPreferredGame();
        UnifiedGameId fallback = unifiedResourceGetOtherGame(preferred);

        // Both data sets remain mounted. Promote the fallback first and the
        // requested origin last so legacy unqualified paths resolve in the
        // intended namespace without ever tearing the other game down.
        unifiedResourcePromoteDataset(fallback);
        unifiedResourcePromoteDataset(preferred);
    }

    return xfileOpen(filePath, mode);
}

} // namespace fallout

// db.cc includes db.h, while xfile.cc includes xfile.h directly. Redirect only
// the DB/file layer so the stock xfile implementation keeps its original symbol
// and this wrapper can safely call it.
#define xfileOpen unifiedResourceXfileOpen

#endif /* UNIFIED_RESOURCE_RESOLVER_H */
