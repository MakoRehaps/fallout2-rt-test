#ifndef MAIN_H
#define MAIN_H

#include "local_coop_dialog_controller.h"
#include "local_coop_generic_ui_controller.h"
#include "local_coop_interaction.h"
#include "local_coop_inventory_ui.h"
#include "local_coop_modal_controller.h"
#include "local_coop_mode_sync.h"
#include "local_coop_runtime.h"
#include "unified_campaign.h"

namespace fallout {

inline constexpr unsigned int kUnifiedFallout1InitialGameTime = (7 * 60 * 60 + 21 * 60) * 10;

int inputGetInput();
int mainMenuWindowInit();

inline int gUnifiedCampaignStartupArgc = 0;
inline char** gUnifiedCampaignStartupArgv = nullptr;

inline void localCoopResetTransientStateForLoad()
{
    for (int slot = 0; slot < kLocalCoopMaxPlayers; slot++) {
        localCoopFocusReleaseOutline(slot);
    }

    localCoopPipboyRestoreMarker();
    localCoopGenericUiRestoreMarker();
    localCoopInventoryUiDestroyWindow();

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
}

inline int localCoopMainInputGetInput()
{
    localCoopModeSyncEnsureTicker();
    localCoopDialogControllerEnsureTicker();
    localCoopModalControllerEnsureTicker();
    localCoopGenericUiControllerEnsureTicker();
    localCoopSyncLegacyModes();
    localCoopRuntimeTick();
    localCoopFocusTick();
    localCoopInventoryUiEnsureTicker();
    localCoopInventoryUiTick();
    localCoopInteractionTick();
    localCoopGenericUiControllerTick();

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

    unifiedCampaignConfigureFromArgs(argc, argv);
    unifiedCampaignSetBeforeGameResetHook(localCoopResetTransientStateForLoad);

    if (!unifiedCampaignActivateContentRoot()) {
        return -1;
    }

    int rc = gameInitWithOptions(
        unifiedCampaignGetWindowTitle(windowTitle),
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

    if (!unifiedCampaignActivateContentRoot()) {
        return false;
    }

    int rc = gameInitWithOptions(
        unifiedCampaignGetWindowTitle("FALLOUT II"),
        false,
        0,
        0,
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
