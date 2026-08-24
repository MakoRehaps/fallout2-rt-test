#ifndef MAIN_H
#define MAIN_H

// Finish the normal dialogue/interpreter/object header chain first. inventory.h
// installs a lightweight dialogue-side dispatcher, not the heavy barter UI.
#include "game_dialog.h"

// Compile the controller barter implementation only after the stock engine
// types are complete. Its internal stock fallback must name the renamed stock
// implementation directly rather than re-entering the dialogue dispatcher.
#ifdef inventoryOpenTrade
#undef inventoryOpenTrade
#endif
#define inventoryOpenTrade inventoryOpenTradeStock
#include "local_coop_barter_ui.h"
#undef inventoryOpenTrade

#include "local_coop_controller_bridge.h"
#include "local_coop_dialog_controller.h"
#include "local_coop_generic_ui_controller.h"
#include "local_coop_interaction.h"
#include "local_coop_inventory_ui.h"
#include "local_coop_modal_controller.h"
#include "local_coop_mode_sync.h"
#include "local_coop_runtime.h"
#include "local_coop_beta_hotfix.h"
#include "unified_campaign.h"

namespace fallout {

inline constexpr unsigned int kUnifiedFallout1InitialGameTime = (7 * 60 * 60 + 21 * 60) * 10;

int inputGetInput();
int mainMenuWindowInit();

inline int gUnifiedCampaignStartupArgc = 0;
inline char** gUnifiedCampaignStartupArgv = nullptr;
inline bool gUnifiedCampaignStartupIsMapper = false;
inline int gUnifiedCampaignStartupFont = 0;
inline int gUnifiedCampaignStartupA4 = 0;
inline std::string gUnifiedCampaignStartupWindowTitle = "FALLOUT II";

inline SDL_GameController* localCoopResolveAssignedController(int slot)
{
    if (slot < 0 || slot >= kLocalCoopMaxPlayers) {
        return nullptr;
    }

    LocalCoopPlayer& player = gLocalCoopPlayers[slot];
    if (!player.connected || player.controller == nullptr) {
        return nullptr;
    }

    return player.controller;
}

inline void localCoopResetTransientStateForLoad()
{
    for (int slot = 0; slot < kLocalCoopMaxPlayers; slot++) {
        localCoopFocusReleaseOutline(slot);
    }

    localCoopPipboyRestoreMarker();
    localCoopGenericUiRestoreMarker();
    localCoopInventoryUiDestroyWindow();
    localCoopLiveLootResetForLoad();

    inputEventQueueReset();
    localCoopSetRealtimeCombatActive(false);
    localCoopRealtimeAiReset();

    gLocalCoopRuntimeSlots = {};
    gLocalCoopFocusSlots = {};
    gLocalCoopInventoryUiSlots = {};
    gLocalCoopDialogControllerState = {};
    gLocalCoopModalControllerState = {};
    gLocalCoopGenericUiControllerState = {};
    gLocalCoopInteractionStates = {};

    gLocalCoopLegacyYieldQueued = false;
    gLocalCoopNextLegacyYieldTick = 0;
    gLocalCoopRuntimeInsideTick = false;
    gLocalCoopModeSyncOwnsPlayerOne = false;

    for (LocalCoopPlayer& player : gLocalCoopPlayers) {
        player.uiMode = LocalCoopUiMode::World;
        player.wantsRun = false;
        player.moveX = 0;
        player.moveY = 0;
        player.aimX = 0;
        player.aimY = 0;
    }
}

inline void localCoopResetTickerRegistrationAfterEngineExit()
{
    gLocalCoopRuntimeTickerInstalled = false;
    gLocalCoopInventoryTickerInstalled = false;
    gLocalCoopModeSyncTickerInstalled = false;
    gLocalCoopDialogControllerTickerInstalled = false;
    gLocalCoopModalControllerTickerInstalled = false;
    gLocalCoopGenericUiControllerTickerInstalled = false;
    gLocalCoopLiveLootState.tickerInstalled = false;
}

inline int localCoopMainInputGetInput()
{
    gLocalCoopControllerLookup = localCoopResolveAssignedController;
    gInventoryOpenTradeControllerHandler = localCoopInventoryOpenTrade;

    localCoopModeSyncEnsureTicker();
    localCoopDialogControllerEnsureTicker();
    localCoopModalControllerEnsureTicker();
    localCoopGenericUiControllerEnsureTicker();
    localCoopLiveLootEnsureTicker();
    localCoopSyncLegacyModes();

    // Preserve the pre-runtime camera center so the beta correction can apply a
    // shared-screen dead-zone after the old runtime has processed movement.
    localCoopBetaHotfixBeginFrame();
    localCoopRuntimeTick();
    localCoopBetaHotfixAfterRuntime();

    localCoopFocusTick();
    localCoopInventoryUiEnsureTicker();
    localCoopInventoryUiTick();
    localCoopInteractionTick();
    localCoopLiveLootTick();
    localCoopGenericUiControllerTick();

    // Legacy proto/script/message tables are still reinitialized when a save or
    // campaign transition changes their origin. Both physical data sets remain
    // mounted throughout; this flag no longer implies a cwd/data-root swap.
    if (gUnifiedCampaignRuntime.loadedSaveRequiresContentReload
        && gUnifiedCampaignRuntime.requestedContentGame != gUnifiedCampaignRuntime.activeGame) {
        _game_user_wants_to_quit = 2;
    }

    int keyCode = inputGetInput();

    if ((keyCode == KEY_UPPERCASE_I || keyCode == KEY_LOWERCASE_I)
        && gLocalCoopInitialized) {
        LocalCoopPlayer& playerOne = gLocalCoopPlayers[0];
        if (playerOne.uiMode == LocalCoopUiMode::Inventory) {
            playerOne.uiMode = LocalCoopUiMode::World;
            return -1;
        }

        if (playerOne.uiMode == LocalCoopUiMode::World) {
            playerOne.uiMode = LocalCoopUiMode::Inventory;
            return -1;
        }
    }

    return keyCode;
}

inline int unifiedCampaignGameInitWithOptions(const char* windowTitle,
    bool isMapper,
    int font,
    int a4,
    int argc,
    char** argv)
{
    gUnifiedCampaignStartupArgc = argc;
    gUnifiedCampaignStartupArgv = argv;
    gUnifiedCampaignStartupIsMapper = isMapper;
    gUnifiedCampaignStartupFont = font;
    gUnifiedCampaignStartupA4 = a4;
    gUnifiedCampaignStartupWindowTitle = windowTitle != nullptr ? windowTitle : "FALLOUT II";

    unifiedCampaignConfigureFromArgs(argc, argv);

    // A fresh combined-campaign process begins on the Fallout 1 side of the
    // fused world. This chooses the origin for legacy unqualified map/script/
    // proto IDs; it does NOT unmount Fallout 2 or turn the process into an F1
    // working directory.
    if (unifiedCampaignIsEnabled()) {
        unifiedCampaignSetActiveGame(UnifiedGameId::Fallout1);
        gUnifiedCampaignRuntime.requestedContentGame = UnifiedGameId::Fallout1;
        gUnifiedCampaignRuntime.loadedSaveRequiresContentReload = false;
    }

    unifiedCampaignSetBeforeGameResetHook(localCoopResetTransientStateForLoad);

    // Non-unified compatibility launches retain the old root activation. The
    // fused runtime deliberately stays in the unified install directory and
    // addresses both original games through absolute dataset roots/xbase mounts.
    if (!unifiedCampaignIsEnabled() && !unifiedCampaignActivateContentRoot()) {
        return -1;
    }

    int rc = gameInitWithOptions(
        unifiedCampaignGetWindowTitle(gUnifiedCampaignStartupWindowTitle.c_str()),
        isMapper,
        font,
        a4,
        argc,
        argv);

    if (rc == 0 && unifiedCampaignGetActiveGame() == UnifiedGameId::Fallout1) {
        gameTimeSetTime(kUnifiedFallout1InitialGameTime);
    }

    return rc;
}

inline bool unifiedCampaignRebootstrapRequestedContent()
{
    if (!gUnifiedCampaignRuntime.loadedSaveRequiresContentReload) {
        return true;
    }

    UnifiedGameId previousGame = gUnifiedCampaignRuntime.activeGame;
    UnifiedGameId requestedGame = gUnifiedCampaignRuntime.requestedContentGame;
    if (requestedGame == previousGame) {
        gUnifiedCampaignRuntime.loadedSaveRequiresContentReload = false;
        _game_user_wants_to_quit = 0;
        return true;
    }

    if (unifiedCampaignGetRoot(requestedGame).empty()) {
        return false;
    }

    localCoopResetTransientStateForLoad();
    localCoopShutdown();
    gameExit();
    localCoopResetTickerRegistrationAfterEngineExit();

    unifiedCampaignSetActiveGame(requestedGame);
    gUnifiedCampaignRuntime.loadedSaveRequiresContentReload = false;

    // Both data sets are already part of the same fused install. Rebootstrap is
    // currently only for the still-singleton legacy proto/script/message tables;
    // never chdir into the requested original game.
    if (!unifiedCampaignIsEnabled() && !unifiedCampaignActivateContentRoot()) {
        return false;
    }

    int rc = gameInitWithOptions(
        unifiedCampaignGetWindowTitle(gUnifiedCampaignStartupWindowTitle.c_str()),
        gUnifiedCampaignStartupIsMapper,
        gUnifiedCampaignStartupFont,
        gUnifiedCampaignStartupA4,
        gUnifiedCampaignStartupArgc,
        gUnifiedCampaignStartupArgv);
    if (rc != 0) {
        return false;
    }

    unifiedCampaignSetBeforeGameResetHook(localCoopResetTransientStateForLoad);
    if (requestedGame == UnifiedGameId::Fallout1) {
        gameTimeSetTime(kUnifiedFallout1InitialGameTime);
    }

    _game_user_wants_to_quit = 0;
    return true;
}

inline int localCoopMainMenuWindowInit()
{
    if (gUnifiedCampaignRuntime.loadedSaveRequiresContentReload
        && !unifiedCampaignRebootstrapRequestedContent()) {
        return -1;
    }

    return mainMenuWindowInit();
}

int falloutMain(int argc, char** argv);

} // namespace fallout

#define inputGetInput localCoopMainInputGetInput
#define gameInitWithOptions unifiedCampaignGameInitWithOptions
#define mainMenuWindowInit localCoopMainMenuWindowInit

#endif /* MAIN_H */
