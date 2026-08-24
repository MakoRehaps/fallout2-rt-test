#ifndef UNIFIED_RESOURCE_RESOLVER_H
#define UNIFIED_RESOURCE_RESOLVER_H

#include <array>
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

inline XFile* unifiedResourceXfileOpen(const char* filePath, const char* mode)
{
    if (unifiedCampaignIsEnabled()
        && unifiedResourceIsRelativePath(filePath)
        && unifiedResourceIsReadOnlyMode(mode)) {
        UnifiedGameId preferred = unifiedResourceGetPreferredGame();
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
