#ifndef UNIFIED_CAMPAIGN_H
#define UNIFIED_CAMPAIGN_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#else
#include <unistd.h>
#endif

#include <string>

namespace fallout {

// Engine-only campaign selector for running Fallout 1 and Fallout 2 content
// through one executable. This layer deliberately owns no art, maps, movies,
// dialogue, or other replacement content.
enum class UnifiedGameId : uint32_t {
    Fallout1 = 1,
    Fallout2 = 2,
};

struct UnifiedCampaignRuntime {
    bool initialized = false;
    bool unifiedCampaign = false;
    bool contentRootActivated = false;
    bool loadedSaveRequiresContentReload = false;
    UnifiedGameId activeGame = UnifiedGameId::Fallout2;
    std::string fallout1Root;
    std::string fallout2Root;
};

// Header stored beside the stock SAVE.DAT. The normal Fallout save data remains
// owned by the existing engine; this identifies which content namespace must be
// active when that slot is restored.
struct UnifiedCampaignSaveHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t activeGame;
    uint32_t flags;
};

inline constexpr uint32_t kUnifiedCampaignSaveMagic = 0x4643554E; // "FCUN"
inline constexpr uint32_t kUnifiedCampaignSaveVersion = 1;
inline constexpr uint32_t kUnifiedCampaignSaveFlagUnified = 0x01;

inline UnifiedCampaignRuntime gUnifiedCampaignRuntime;
inline UnifiedCampaignSaveHeader gUnifiedCampaignPendingSaveHeader{};
inline bool gUnifiedCampaignPendingSaveHeaderValid = false;

inline bool unifiedCampaignStartsWith(const char* value, const char* prefix)
{
    if (value == nullptr || prefix == nullptr) {
        return false;
    }

    size_t prefixLength = strlen(prefix);
    return strncmp(value, prefix, prefixLength) == 0;
}

inline void unifiedCampaignSetRoot(UnifiedGameId game, const char* root)
{
    std::string value = root != nullptr ? root : "";
    while (!value.empty() && (value.back() == '/' || value.back() == '\\')) {
        value.pop_back();
    }

    if (game == UnifiedGameId::Fallout1) {
        gUnifiedCampaignRuntime.fallout1Root = value;
    } else {
        gUnifiedCampaignRuntime.fallout2Root = value;
    }

    gUnifiedCampaignRuntime.contentRootActivated = false;
}

inline void unifiedCampaignSetActiveGame(UnifiedGameId game)
{
    if (gUnifiedCampaignRuntime.activeGame != game) {
        gUnifiedCampaignRuntime.activeGame = game;
        gUnifiedCampaignRuntime.contentRootActivated = false;
    }
}

inline UnifiedGameId unifiedCampaignGetActiveGame()
{
    return gUnifiedCampaignRuntime.activeGame;
}

inline bool unifiedCampaignIsEnabled()
{
    return gUnifiedCampaignRuntime.unifiedCampaign;
}

inline const std::string& unifiedCampaignGetRoot(UnifiedGameId game)
{
    return game == UnifiedGameId::Fallout1
        ? gUnifiedCampaignRuntime.fallout1Root
        : gUnifiedCampaignRuntime.fallout2Root;
}

inline const std::string& unifiedCampaignGetActiveRoot()
{
    return unifiedCampaignGetRoot(gUnifiedCampaignRuntime.activeGame);
}

inline std::string unifiedCampaignResolvePath(const char* relativePath)
{
    if (relativePath == nullptr || *relativePath == '\0') {
        return unifiedCampaignGetActiveRoot();
    }

    const std::string& root = unifiedCampaignGetActiveRoot();
    if (root.empty()) {
        return std::string(relativePath);
    }

    std::string path = root;
#if defined(_WIN32)
    path.push_back('\\');
#else
    path.push_back('/');
#endif
    path.append(relativePath);
    return path;
}

inline bool unifiedCampaignActivateContentRoot()
{
    if (gUnifiedCampaignRuntime.contentRootActivated) {
        return true;
    }

    const std::string& root = unifiedCampaignGetActiveRoot();
    if (root.empty()) {
        // Empty root preserves Fallout CE's normal behavior: data is resolved
        // relative to the executable/current working directory.
        gUnifiedCampaignRuntime.contentRootActivated = true;
        return true;
    }

#if defined(_WIN32)
    int rc = _chdir(root.c_str());
#else
    int rc = chdir(root.c_str());
#endif

    if (rc != 0) {
        return false;
    }

    gUnifiedCampaignRuntime.contentRootActivated = true;
    return true;
}

inline const char* unifiedCampaignGetWindowTitle(const char* fallback)
{
    if (gUnifiedCampaignRuntime.unifiedCampaign) {
        return "FALLOUT CE - UNIFIED";
    }

    if (gUnifiedCampaignRuntime.activeGame == UnifiedGameId::Fallout1) {
        return "FALLOUT";
    }

    return fallback != nullptr ? fallback : "FALLOUT II";
}

inline void unifiedCampaignConfigureFromEnvironment()
{
    if (gUnifiedCampaignRuntime.fallout1Root.empty()) {
        const char* value = getenv("FALLOUT1_ROOT");
        if (value != nullptr) {
            unifiedCampaignSetRoot(UnifiedGameId::Fallout1, value);
        }
    }

    if (gUnifiedCampaignRuntime.fallout2Root.empty()) {
        const char* value = getenv("FALLOUT2_ROOT");
        if (value != nullptr) {
            unifiedCampaignSetRoot(UnifiedGameId::Fallout2, value);
        }
    }
}

// Supported engine switches:
//   --unified                 Start the combined campaign in Fallout 1 mode.
//   --fallout1                Run the Fallout 1 content namespace.
//   --fallout2                Run the Fallout 2 content namespace.
//   --fallout1-root=<path>    Root containing the user's Fallout 1 data.
//   --fallout2-root=<path>    Root containing the user's Fallout 2 data.
//
// Content remains user-supplied; this code does not bundle or generate assets.
inline void unifiedCampaignConfigureFromArgs(int argc, char** argv)
{
    if (gUnifiedCampaignRuntime.initialized) {
        return;
    }

    gUnifiedCampaignRuntime.initialized = true;
    unifiedCampaignConfigureFromEnvironment();

    for (int index = 1; index < argc; index++) {
        const char* arg = argv[index];
        if (arg == nullptr) {
            continue;
        }

        if (strcmp(arg, "--unified") == 0) {
            gUnifiedCampaignRuntime.unifiedCampaign = true;
            unifiedCampaignSetActiveGame(UnifiedGameId::Fallout1);
        } else if (strcmp(arg, "--fallout1") == 0) {
            unifiedCampaignSetActiveGame(UnifiedGameId::Fallout1);
        } else if (strcmp(arg, "--fallout2") == 0) {
            unifiedCampaignSetActiveGame(UnifiedGameId::Fallout2);
        } else if (unifiedCampaignStartsWith(arg, "--fallout1-root=")) {
            unifiedCampaignSetRoot(UnifiedGameId::Fallout1, arg + strlen("--fallout1-root="));
        } else if (unifiedCampaignStartsWith(arg, "--fallout2-root=")) {
            unifiedCampaignSetRoot(UnifiedGameId::Fallout2, arg + strlen("--fallout2-root="));
        }
    }
}

inline bool unifiedCampaignAdvanceToFallout2()
{
    if (!gUnifiedCampaignRuntime.unifiedCampaign
        || gUnifiedCampaignRuntime.activeGame != UnifiedGameId::Fallout1) {
        return false;
    }

    unifiedCampaignSetActiveGame(UnifiedGameId::Fallout2);
    return unifiedCampaignActivateContentRoot();
}

inline UnifiedCampaignSaveHeader unifiedCampaignMakeSaveHeader()
{
    UnifiedCampaignSaveHeader header{};
    header.magic = kUnifiedCampaignSaveMagic;
    header.version = kUnifiedCampaignSaveVersion;
    header.activeGame = static_cast<uint32_t>(gUnifiedCampaignRuntime.activeGame);
    header.flags = gUnifiedCampaignRuntime.unifiedCampaign ? kUnifiedCampaignSaveFlagUnified : 0;
    return header;
}

inline bool unifiedCampaignApplySaveHeader(const UnifiedCampaignSaveHeader& header)
{
    if (header.magic != kUnifiedCampaignSaveMagic
        || header.version != kUnifiedCampaignSaveVersion) {
        return false;
    }

    if (header.activeGame != static_cast<uint32_t>(UnifiedGameId::Fallout1)
        && header.activeGame != static_cast<uint32_t>(UnifiedGameId::Fallout2)) {
        return false;
    }

    UnifiedGameId savedGame = static_cast<UnifiedGameId>(header.activeGame);
    gUnifiedCampaignRuntime.loadedSaveRequiresContentReload =
        savedGame != gUnifiedCampaignRuntime.activeGame;

    unifiedCampaignSetActiveGame(savedGame);
    gUnifiedCampaignRuntime.unifiedCampaign = (header.flags & kUnifiedCampaignSaveFlagUnified) != 0;
    return true;
}

inline void unifiedCampaignClearPendingSaveHeader()
{
    gUnifiedCampaignPendingSaveHeader = UnifiedCampaignSaveHeader{};
    gUnifiedCampaignPendingSaveHeaderValid = false;
}

inline bool unifiedCampaignStageSaveHeader(const UnifiedCampaignSaveHeader& header)
{
    if (header.magic != kUnifiedCampaignSaveMagic
        || header.version != kUnifiedCampaignSaveVersion
        || (header.activeGame != static_cast<uint32_t>(UnifiedGameId::Fallout1)
            && header.activeGame != static_cast<uint32_t>(UnifiedGameId::Fallout2))) {
        unifiedCampaignClearPendingSaveHeader();
        return false;
    }

    gUnifiedCampaignPendingSaveHeader = header;
    gUnifiedCampaignPendingSaveHeaderValid = true;
    return true;
}

inline bool unifiedCampaignApplyPendingSaveHeader()
{
    if (!gUnifiedCampaignPendingSaveHeaderValid) {
        return false;
    }

    UnifiedCampaignSaveHeader header = gUnifiedCampaignPendingSaveHeader;
    unifiedCampaignClearPendingSaveHeader();
    return unifiedCampaignApplySaveHeader(header);
}

} // namespace fallout

#endif /* UNIFIED_CAMPAIGN_H */
