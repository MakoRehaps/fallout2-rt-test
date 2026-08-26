from pathlib import Path


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    if new in text:
        print(f"{path}: already patched")
        return
    if old not in text:
        raise SystemExit(f"{path}: expected text not found")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")
    print(f"{path}: patched")


# Public hidden-checkpoint API. Use int gameId here to keep loadsave.h independent
# from unified_campaign.h's enum definition.
replace_once(
    Path("src/loadsave.h"),
    "int lsgAutosaveGame();\nint lsgLoadLastGame();",
    "int lsgAutosaveGame();\nint lsgLoadLastGame();\nint lsgSaveUnifiedCampaignCheckpoint(int gameId);\nint lsgLoadUnifiedCampaignCheckpoint(int gameId);\nbool lsgUnifiedCampaignCheckpointExists(int gameId);",
)

# The checkpoint is a complete copy of Fallout's normal save slot, not a custom
# quest summary. This deliberately preserves globals, scripts, maps, local vars,
# NPC state, queues, automap, party data and COOPMETA.SAV together.
replace_once(
    Path("src/loadsave.cc"),
    "#include <algorithm>\n",
    "#include <algorithm>\n#include <filesystem>\n",
)

checkpoint_impl = r'''

static std::filesystem::path lsgUnifiedCampaignCheckpointPath(int gameId)
{
    return std::filesystem::path("SAVEGAME")
        / (gameId == 1 ? "UNIFIED_F1_RESUME" : "UNIFIED_F2_RESUME");
}

bool lsgUnifiedCampaignCheckpointExists(int gameId)
{
    if (gameId != 1 && gameId != 2) {
        return false;
    }

    std::error_code ec;
    std::filesystem::path root = lsgUnifiedCampaignCheckpointPath(gameId);
    return std::filesystem::exists(root / "SAVE.DAT", ec)
        && std::filesystem::exists(root / "COOPMETA.SAV", ec);
}

static bool lsgCopyUnifiedCampaignCheckpointDirectory(
    const std::filesystem::path& source,
    const std::filesystem::path& destination)
{
    std::error_code ec;
    if (!std::filesystem::exists(source / "SAVE.DAT", ec)) {
        return false;
    }

    std::filesystem::remove_all(destination, ec);
    ec.clear();
    std::filesystem::create_directories(destination.parent_path(), ec);
    if (ec) {
        return false;
    }

    std::filesystem::copy(
        source,
        destination,
        std::filesystem::copy_options::recursive
            | std::filesystem::copy_options::overwrite_existing,
        ec);
    return !ec;
}

int lsgSaveUnifiedCampaignCheckpoint(int gameId)
{
    if (gameId != 1 && gameId != 2) {
        return -1;
    }

    // Slot 10 is already the engine-owned autosave slot. Save through the normal
    // handler table first so every stock quest/script/map subsystem serializes
    // itself, then archive that whole slot under the campaign-specific name.
    if (lsgAutosaveGame() != 1) {
        debugPrint("[CAMPAIGN RESUME] checkpoint save failed game=%d\n", gameId);
        return -1;
    }

    std::filesystem::path source = std::filesystem::path("SAVEGAME") / "SLOT10";
    std::filesystem::path destination = lsgUnifiedCampaignCheckpointPath(gameId);
    if (!lsgCopyUnifiedCampaignCheckpointDirectory(source, destination)) {
        debugPrint("[CAMPAIGN RESUME] checkpoint archive failed game=%d\n", gameId);
        return -1;
    }

    debugPrint("[CAMPAIGN RESUME] checkpoint saved game=%d\n", gameId);
    return 1;
}

int lsgLoadUnifiedCampaignCheckpoint(int gameId)
{
    if (gameId != 1 && gameId != 2 || !lsgUnifiedCampaignCheckpointExists(gameId)) {
        return -1;
    }

    std::filesystem::path source = lsgUnifiedCampaignCheckpointPath(gameId);
    std::filesystem::path destination = std::filesystem::path("SAVEGAME") / "SLOT10";
    if (!lsgCopyUnifiedCampaignCheckpointDirectory(source, destination)) {
        debugPrint("[CAMPAIGN RESUME] checkpoint restore failed game=%d\n", gameId);
        return -1;
    }

    // Force the normal loader to use the restored campaign slot. COOPMETA.SAV
    // belongs to this same checkpoint and therefore stages the matching profile
    // plus unified/F1 world state before the stock SAVE.DAT handler table runs.
    gLastLoadableSlot = 9;
    int rc = lsgLoadLastGame();
    debugPrint("[CAMPAIGN RESUME] checkpoint load game=%d rc=%d\n", gameId, rc);
    return rc;
}
'''

replace_once(
    Path("src/loadsave.cc"),
    "\n// 0x47C5B4\nstatic int _QuickSnapShot()",
    checkpoint_impl + "\n// 0x47C5B4\nstatic int _QuickSnapShot()",
)

# Save the outgoing world before either the one-way F1->F2 transition or a
# postgame world switch. Initial Act II still starts F2 as a new campaign; only
# postgame switches use the resume loader.
replace_once(
    Path("src/unified_campaign_transition.h"),
    '#include "unified_campaign.h"',
    '#include "loadsave.h"\n#include "unified_campaign.h"',
)
replace_once(
    Path("src/unified_campaign_transition.h"),
    "inline bool unifiedCampaignAdvanceToFallout2AndAutoStart()\n{\n    if (!unifiedCampaignAdvanceToFallout2()) {",
    "inline bool unifiedCampaignAdvanceToFallout2AndAutoStart()\n{\n    if (lsgSaveUnifiedCampaignCheckpoint(static_cast<int>(UnifiedGameId::Fallout1)) != 1) {\n        return false;\n    }\n\n    if (!unifiedCampaignAdvanceToFallout2()) {",
)
replace_once(
    Path("src/unified_campaign_transition.h"),
    "inline bool unifiedCampaignRequestPostgameWorldSwitchAndResume()\n{\n    if (!unifiedCampaignRequestOtherPostgameWorld()) {\n        return false;\n    }\n\n    gUnifiedCampaignAutoStartNewGame = true;\n    gUnifiedCampaignPostgameResumePending = true;\n    return true;\n}",
    "inline bool unifiedCampaignRequestPostgameWorldSwitchAndResume()\n{\n    UnifiedGameId source = unifiedCampaignGetActiveGame();\n    UnifiedGameId destination = source == UnifiedGameId::Fallout1\n        ? UnifiedGameId::Fallout2\n        : UnifiedGameId::Fallout1;\n\n    if (lsgSaveUnifiedCampaignCheckpoint(static_cast<int>(source)) != 1) {\n        return false;\n    }\n\n    // A completed world must already have a checkpoint: F1 gets one when Act I\n    // advances to F2, and F2 gets one on its first postgame departure. Refuse to\n    // silently create a fresh campaign if that persistent state is missing.\n    if (!lsgUnifiedCampaignCheckpointExists(static_cast<int>(destination))) {\n        return false;\n    }\n\n    if (!unifiedCampaignRequestOtherPostgameWorld()) {\n        return false;\n    }\n\n    gUnifiedCampaignAutoStartNewGame = false;\n    gUnifiedCampaignPostgameResumePending = true;\n    return true;\n}",
)

# Add a private main-menu result for seamless checkpoint restoration.
replace_once(
    Path("src/mainmenu.h"),
    "    MAIN_MENU_OPTIONS,\n} MainMenuOption;",
    "    MAIN_MENU_OPTIONS,\n    MAIN_MENU_RESUME_CAMPAIGN,\n} MainMenuOption;",
)
replace_once(
    Path("src/mainmenu.h"),
    "inline int unifiedCampaignMainMenuWindowHandleEvents()\n{\n    if (unifiedCampaignConsumeAutoStartNewGame()) {",
    "inline int unifiedCampaignMainMenuWindowHandleEvents()\n{\n    if (gUnifiedCampaignPostgameResumePending\n        && unifiedCampaignBothGamesCompleted()) {\n        return MAIN_MENU_RESUME_CAMPAIGN;\n    }\n\n    if (unifiedCampaignConsumeAutoStartNewGame()) {",
)

# Mirror the stock load-game setup, but skip the slot picker and load the hidden
# checkpoint for the already-rebooted active profile.
resume_case = r'''            case MAIN_MENU_RESUME_CAMPAIGN:
                if (1) {
                    int win = windowCreate(0, 0, screenGetWidth(), screenGetHeight(), _colorTable[0], WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
                    mainMenuWindowHide(true);
                    mainMenuWindowFree();

                    main_loadgame_new();

                    colorPaletteLoad("color.pal");
                    paletteFadeTo(_cmap);
                    int gameId = static_cast<int>(unifiedCampaignGetActiveGame());
                    int loadGameRc = lsgLoadUnifiedCampaignCheckpoint(gameId);
                    if (loadGameRc == -1) {
                        debugPrint("\n ** Error restoring unified campaign checkpoint! **\n");
                    } else if (loadGameRc != 0) {
                        unifiedCampaignConsumePostgameResume();
                        windowDestroy(win);
                        win = -1;
                        mainLoop();
                    }
                    paletteFadeTo(gPaletteWhite);
                    if (win != -1) {
                        windowDestroy(win);
                    }

                    main_unload_new();
                    main_reset_system();
                    mainMenuWindowInit();
                }
                break;
'''
replace_once(
    Path("src/main.cc"),
    "            case MAIN_MENU_TIMEOUT:\n",
    resume_case + "            case MAIN_MENU_TIMEOUT:\n",
)

print("Postgame campaign resume patch complete.")
