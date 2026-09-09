#ifndef FALLOUT_SETTINGS_H_
#define FALLOUT_SETTINGS_H_

#include <string>

#include "game_config.h"
#include "unified_campaign.h"

namespace fallout {

struct SystemSettings {
    std::string executable = "game";
    std::string master_dat_path = "master.dat";
    std::string master_patches_path = "data";
    std::string critter_dat_path = "critter.dat";
    std::string critter_patches_path = "data";
    std::string language = ENGLISH;
    int scroll_lock = 0;
    bool interrupt_walk = true;
    int art_cache_size = 8;
    bool color_cycling = true;
    int cycle_speed_factor = 1;
    bool hashing = true;
    int splash = 0;
    int free_space = 20480;
    int times_run = 0;
};

struct PreferencesSettings {
    int game_difficulty = GAME_DIFFICULTY_NORMAL;
    int combat_difficulty = COMBAT_DIFFICULTY_NORMAL;
    int violence_level = VIOLENCE_LEVEL_MAXIMUM_BLOOD;
    int target_highlight = TARGET_HIGHLIGHT_TARGETING_ONLY;
    bool item_highlight = true;
    bool combat_looks = false;
    bool combat_messages = true;
    bool combat_taunts = true;
    bool language_filter = false;
    bool running = false;
    bool subtitles = false;
    int combat_speed = 0;
    bool player_speedup = false;
    double text_base_delay = 3.5;
    double text_line_delay = 1.399994;
    double brightness = 1.0;
    double mouse_sensitivity = 1.0;
    bool running_burning_guy = true;
};

struct SoundSettings {
    bool initialize = true;
    bool debug = false;
    bool debug_sfxc = true;
    int device = -1;
    int port = -1;
    int irq = -1;
    int dma = -1;
    bool sounds = true;
    bool music = true;
    bool speech = true;
    int master_volume = 22281;
    int music_volume = 22281;
    int sndfx_volume = 22281;
    int speech_volume = 22281;
    int cache_size = 448;
    std::string music_path1 = "sound\\music\\";
    std::string music_path2 = "sound\\music\\";
};

struct DebugSettings {
    std::string mode = "environment";
    bool show_tile_num = false;
    bool show_script_messages = false;
    bool show_load_info = false;
    bool output_map_data_info = false;
};

struct MapperSettings {
    bool override_librarian = false;
    bool librarian = false;
    bool user_art_not_protos = false;
    bool rebuild_protos = false;
    bool fix_map_objects = false;
    bool fix_map_inventory = false;
    bool ignore_rebuild_errors = false;
    bool show_pid_numbers = false;
    bool save_text_maps = false;
    bool run_mapper_as_game = false;
    bool default_f8_as_game = true;
    bool sort_script_list = false;
};

struct Settings {
    SystemSettings system;
    PreferencesSettings preferences;
    SoundSettings sound;
    DebugSettings debug;
    MapperSettings mapper;
};

extern Settings settings;

bool settingsInit(bool isMapper, int argc, char** argv);
bool settingsSave();
bool settingsExit(bool shouldSave);

// game.cc includes game.h before settings.h. Restrict this wrapper to that
// consumer so settings.cc still defines the stock settingsInit symbol normally.
// The selected content origin determines which dataset owns unqualified legacy
// IDs for the current map, but it no longer determines which game is mounted.
#ifdef GAME_H
inline bool unifiedCampaignSettingsInit(bool isMapper, int argc, char** argv)
{
    if (!settingsInit(isMapper, argc, argv)) {
        return false;
    }

    if (!unifiedCampaignIsEnabled()) {
        return true;
    }

    const std::string& root = unifiedCampaignGetActiveRoot();
    if (root.empty()) {
        return true;
    }

#if defined(_WIN32)
    constexpr char kPathSeparator = '\\';
#else
    constexpr char kPathSeparator = '/';
#endif

    std::string prefix = root;
    if (!prefix.empty() && prefix.back() != '/' && prefix.back() != '\\') {
        prefix.push_back(kPathSeparator);
    }

    settings.system.master_dat_path = prefix + "master.dat";
    settings.system.critter_dat_path = prefix + "critter.dat";
    settings.system.master_patches_path = prefix + "data";
    settings.system.critter_patches_path = prefix + "data";

    return true;
}

inline std::string unifiedCampaignDatasetPath(UnifiedGameId game, const char* relative)
{
    const std::string& root = unifiedCampaignGetRoot(game);
    if (root.empty()) {
        return std::string();
    }

#if defined(_WIN32)
    constexpr char kDatasetPathSeparator = '\\';
#else
    constexpr char kDatasetPathSeparator = '/';
#endif

    std::string path = root;
    if (!path.empty() && path.back() != '/' && path.back() != '\\') {
        path.push_back(kDatasetPathSeparator);
    }
    path.append(relative != nullptr ? relative : "");
    return path;
}

// A unified process always exposes both original data sets. The current map's
// origin is still mounted last so old unqualified lookups preserve that game's
// semantics, while explicit origin-aware systems can address either set. The
// DAT reader itself now dispatches by archive origin (DAT1 for F1, DAT2 for F2)
// rather than by active campaign.
inline int unifiedCampaignDbOpen(const char* filePath1, int a2, const char* filePath2, int a4)
{
    if (unifiedCampaignIsEnabled()
        && filePath1 != nullptr
        && settings.system.master_dat_path == filePath1) {
        UnifiedGameId active = unifiedCampaignGetActiveGame();
        UnifiedGameId other = active == UnifiedGameId::Fallout1
            ? UnifiedGameId::Fallout2
            : UnifiedGameId::Fallout1;

        std::string otherMaster = unifiedCampaignDatasetPath(other, "master.dat");
        std::string otherCritter = unifiedCampaignDatasetPath(other, "critter.dat");
        std::string otherData = unifiedCampaignDatasetPath(other, "data");

        if (!otherMaster.empty() && !otherCritter.empty() && !otherData.empty()) {
            // Mount the other game's master first, then critter/data. dbOpen /
            // xbaseOpen place newer bases at the head; the stock active dataset
            // is opened immediately after this wrapper returns and therefore
            // remains the legacy default without hiding the other game.
            dbOpen(otherMaster.c_str(), 0, otherData.c_str(), 1);
            dbOpen(otherCritter.c_str(), 0, otherData.c_str(), 1);
        }
    }

    return dbOpen(filePath1, a2, filePath2, a4);
}

#define settingsInit unifiedCampaignSettingsInit
#define dbOpen unifiedCampaignDbOpen
#endif

} // namespace fallout

#endif /* FALLOUT_SETTINGS_H_ */
