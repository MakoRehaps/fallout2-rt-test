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
#include "local_coop_autosave.h"
#include "local_coop_mobile.h"
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

// The combined game has one realtime world state. This flag is only a P1
// weapon-ready/aim presentation state for mouse play; it is deliberately NOT
// Fallout's gCombatState and never starts the legacy turn scheduler.
inline bool gLocalCoopPlayerOneWeaponReady = false;

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
    localCoopAutosaveReset();

    inputEventQueueReset();
    localCoopSetRealtimeCombatActive(false);
    localCoopRealtimeAiReset();
    gLocalCoopPlayerOneWeaponReady = false;

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
        player.actionMode = LocalCoopActionMode::Interact;
        player.hexAimHeld = false;
        player.hexAimTile = -1;
        player.controllerInputActive = false;
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

inline bool localCoopPlayerOneHybridWorldActive()
{
    if (!gLocalCoopInitialized) {
        return false;
    }

    LocalCoopPlayer& player = gLocalCoopPlayers[0];
    return player.humanOwned
        && player.actor != nullptr
        && player.actor == gDude
        && player.uiMode == LocalCoopUiMode::World
        && (player.actor->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) == 0;
}

inline void localCoopSyncPlayerOneWeaponReadyCursor()
{
    if (!localCoopPlayerOneHybridWorldActive()) {
        return;
    }

    LocalCoopPlayer& player = gLocalCoopPlayers[0];

    // The last-used device owns cursor presentation. Controller play is fully
    // cursorless; the first keyboard or mouse event restores the pointer.
    if (player.controllerInputActive) {
        gameMouseObjectsHide();
        if (!cursorIsHidden()) {
            mouseHideCursor();
        }
        return;
    }

    gameMouseObjectsShow();
    if (cursorIsHidden()) {
        mouseShowCursor();
    }

    bool aiming = player.actionMode == LocalCoopActionMode::Aim;
    gLocalCoopPlayerOneWeaponReady = aiming;

    int mode = gameMouseGetMode();
    if (aiming) {
        if (mode != GAME_MOUSE_MODE_CROSSHAIR) {
            gameMouseSetCursor(MOUSE_CURSOR_CROSSHAIR);
            gameMouseSetMode(GAME_MOUSE_MODE_CROSSHAIR);
        }
    } else if (mode != GAME_MOUSE_MODE_MOVE) {
        gameMouseSetMode(GAME_MOUSE_MODE_MOVE);
    }
}

inline void localCoopTogglePlayerOneActionMode()
{
    if (!localCoopPlayerOneHybridWorldActive()) {
        return;
    }

    LocalCoopPlayer& player = gLocalCoopPlayers[0];
    player.actionMode = player.actionMode == LocalCoopActionMode::Aim
        ? LocalCoopActionMode::Interact
        : LocalCoopActionMode::Aim;
    gLocalCoopPlayerOneWeaponReady = player.actionMode == LocalCoopActionMode::Aim;

    debugPrint("[COOP INPUT] P1 action-mode=%s\n",
        gLocalCoopPlayerOneWeaponReady ? "aim" : "interact");
    localCoopSyncPlayerOneWeaponReadyCursor();
}

// Fallout's main HUD weapon/action button produces event -20. In the stock
// game this eventually calls _intface_use_item(), which calls _combat(nullptr)
// for weapons and starts the turn scheduler. Consume that event here instead.
// The button is now only a realtime ready/holster toggle (or reload action).
inline bool localCoopHandlePlayerOneHybridHudAction(int keyCode)
{
    if (keyCode != -20 || !localCoopPlayerOneHybridWorldActive()) {
        return false;
    }

    LocalCoopPlayer& player = gLocalCoopPlayers[0];
    int hand = localCoopGetActiveHand(player);
    Object* item = localCoopGetActiveItem(player);

    int leftAction = INTERFACE_ITEM_ACTION_DEFAULT;
    int rightAction = INTERFACE_ITEM_ACTION_DEFAULT;
    interfaceGetItemActions(&leftAction, &rightAction);
    int action = hand == HAND_LEFT ? leftAction : rightAction;

    // Preserve the HUD's reload selection, but make reload realtime: no AP or
    // turn-mode gate and no transition into _combat().
    if (action == INTERFACE_ITEM_ACTION_RELOAD) {
        bool reloaded = localCoopReloadFromSharedPool(player);
        debugPrint("[COOP HYBRID] hud reload hand=%d itemPid=%d result=%d\n",
            hand,
            item != nullptr ? item->pid : -1,
            reloaded ? 1 : 0);
        if (gInterfaceBarWindow != -1) {
            interfaceUpdateItems(false, leftAction, rightAction);
        }
        return true;
    }

    // Non-weapon usable items retain the stock HUD action path. Empty hands are
    // combat-capable (punch/kick), so nullptr intentionally toggles ready state.
    if (item != nullptr && itemGetType(item) != ITEM_TYPE_WEAPON) {
        return false;
    }

    gLocalCoopPlayerOneWeaponReady = !gLocalCoopPlayerOneWeaponReady;
    player.actionMode = gLocalCoopPlayerOneWeaponReady
        ? LocalCoopActionMode::Aim
        : LocalCoopActionMode::Interact;
    debugPrint("[COOP HYBRID] weapon-ready=%d hand=%d itemPid=%d\n",
        gLocalCoopPlayerOneWeaponReady ? 1 : 0,
        hand,
        item != nullptr ? item->pid : -1);
    localCoopSyncPlayerOneWeaponReadyCursor();
    return true;
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

    // Phone browsers are surfaced as normal SDL gamepads before the runtime
    // polls controllers, so every existing combat and menu path sees them.
    localCoopMobileTick();

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
    localCoopSyncPlayerOneWeaponReadyCursor();
    localCoopAutosaveTick();

    // Legacy proto/script/message tables are still reinitialized when a save or
    // campaign transition changes their origin. Both physical data sets remain
    // mounted throughout; this flag no longer implies a cwd/data-root swap.
    if (gUnifiedCampaignRuntime.loadedSaveRequiresContentReload
        && gUnifiedCampaignRuntime.requestedContentGame != gUnifiedCampaignRuntime.activeGame) {
        _game_user_wants_to_quit = 2;
    }

    int keyCode = inputGetInput();

    // Keyboard or mouse immediately reclaims P1 input from an attached
    // controller and restores the normal pointer.
    if (gLocalCoopInitialized && keyCode != -1) {
        LocalCoopPlayer& playerOne = gLocalCoopPlayers[0];
        if (playerOne.controllerInputActive) {
            playerOne.controllerInputActive = false;
            localCoopSyncPlayerOneWeaponReadyCursor();
        }
    }

    // The HUD action button is no longer allowed to enter Fallout's combat
    // scheduler. Consume its -20 event before gameHandleKey/_intface_use_item.
    if (localCoopHandlePlayerOneHybridHudAction(keyCode)) {
        return -1;
    }

    // P1 is hybrid-input. Keyboard remains available, but the world view now
    // treats WASD as held realtime locomotion instead of passing those letters
    // to Fallout's legacy hotkeys (notably A = enter turn-based combat).
    if (gLocalCoopInitialized) {
        LocalCoopPlayer& playerOne = gLocalCoopPlayers[0];
        if (playerOne.humanOwned
            && playerOne.actor == gDude
            && playerOne.uiMode == LocalCoopUiMode::World) {
            if (keyCode == KEY_LOWERCASE_W || keyCode == KEY_UPPERCASE_W
                || keyCode == KEY_LOWERCASE_A || keyCode == KEY_UPPERCASE_A
                || keyCode == KEY_LOWERCASE_S || keyCode == KEY_UPPERCASE_S
                || keyCode == KEY_LOWERCASE_D || keyCode == KEY_UPPERCASE_D) {
                return -1;
            }

            // Space is the explicit keyboard mode switch. In Interact mode,
            // left click talks/uses/loots; in Aim mode it attacks. Right click
            // remains movement in both modes.
            if (keyCode == KEY_SPACE) {
                localCoopTogglePlayerOneActionMode();
                return -1;
            }

            // E remains a direct interaction shortcut regardless of the active
            // mouse mode, matching controller A.
            if (keyCode == KEY_LOWERCASE_E || keyCode == KEY_UPPERCASE_E) {
                localCoopPlayerOneInteract();
                return -1;
            }
        }
    }

    // ISO-world mouse clicks use the co-op Diablo mapping: right click moves;
    // left click performs the context action or attacks. Returning -1 prevents
    // the stock world mouse handler from also cycling modes or moving the dude.
    if (localCoopHandlePlayerOneMouseInput(keyCode)) {
        return -1;
    }

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

    if (rc == 0) {
        // Install the realtime combat dispatchers before a map script or hostile
        // can request combat. Waiting for the first input frame leaves a startup
        // window where stock combat AI can seize a legacy turn and loop attacks.
        localCoopInit();
        localCoopRealtimeAiInstall();
        localCoopRuntimeEnsureTicker();

        if (unifiedCampaignGetActiveGame() == UnifiedGameId::Fallout1) {
            gameTimeSetTime(kUnifiedFallout1InitialGameTime);
        }
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
    localCoopInit();
    localCoopRealtimeAiInstall();
    localCoopRuntimeEnsureTicker();
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