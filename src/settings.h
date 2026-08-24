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
// Unified mode must never inherit stale Fallout 2 asset paths from fallout2.cfg:
// the active campaign root is authoritative immediately before gameDbInit opens
// MASTER.DAT, CRITTER.DAT and the loose patch directory.
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

// The runtime is the Fallout 2 engine for both campaigns. When Fallout 1 is
// active, keep Fallout 1 authoritative but mount the Fallout 2 installation
// underneath it as an engine/UI compatibility fallback. xbaseOpen moves each
// newly opened base to the front, so mounting F2 first and then letting the
// stock gameDbInit open active F1 data preserves this lookup order:
//   F1 loose data -> F1 critter/master -> F2 loose data -> F2 critter/master.
// Fallout 2 mode remains the stock/native F2 database chain.
inline int unifiedCampaignDbOpen(const char* filePath1, int a2, const char* filePath2, int a4)
{
    if (unifiedCampaignIsEnabled()
        && unifiedCampaignGetActiveGame() == UnifiedGameId::Fallout1
        && filePath1 != nullptr
        && settings.system.master_dat_path == filePath1) {
        const std::string& fallout2Root = unifiedCampaignGetRoot(UnifiedGameId::Fallout2);
        if (!fallout2Root.empty()) {
#if defined(_WIN32)
            constexpr char kFallbackPathSeparator = '\\';
#else
            constexpr char kFallbackPathSeparator = '/';
#endif
            std::string prefix = fallout2Root;
            if (!prefix.empty() && prefix.back() != '/' && prefix.back() != '\\') {
                prefix.push_back(kFallbackPathSeparator);
            }

            std::string fallout2Master = prefix + "master.dat";
            std::string fallout2Critter = prefix + "critter.dat";
            std::string fallout2Data = prefix + "data";

            // Best-effort fallback. Missing F2 compatibility data must not
            // replace the normal active-profile error handling below.
            dbOpen(fallout2Master.c_str(), 0, fallout2Data.c_str(), 1);
            dbOpen(fallout2Critter.c_str(), 0, fallout2Data.c_str(), 1);
        }
    }

    return dbOpen(filePath1, a2, filePath2, a4);
}

#define settingsInit unifiedCampaignSettingsInit
#define dbOpen unifiedCampaignDbOpen
#endif

} // namespace fallout

#endif /* FALLOUT_SETTINGS_H_ */