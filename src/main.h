#ifndef MAIN_H
#define MAIN_H

#include "local_coop_dialog_controller.h"
#include "local_coop_interaction.h"
#include "local_coop_inventory_ui.h"
#include "local_coop_mode_sync.h"
#include "local_coop_runtime.h"
#include "unified_campaign.h"

namespace fallout {

// main.cc includes this header before input.h. Keep a declaration of the
// original input function here, then redirect only main.cc's calls through the
// cooperative frame wrapper below. input.cc itself is untouched and continues
// to define the original inputGetInput symbol.
int inputGetInput();

inline int localCoopMainInputGetInput()
{
    localCoopModeSyncEnsureTicker();
    localCoopDialogControllerEnsureTicker();
    localCoopSyncLegacyModes();
    localCoopRuntimeTick();
    localCoopFocusTick();
    localCoopInventoryUiEnsureTicker();
    localCoopInventoryUiTick();
    localCoopInteractionTick();

    int keyCode = inputGetInput();

    // Keep the familiar keyboard shortcut, but route it into the live co-op
    // overlay rather than Fallout's blocking inventory modal. Controller Back
    // uses the same LocalCoopUiMode state in local_coop_inventory_ui.h.
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

// main.cc is also the only place that calls gameInitWithOptions. Configure the
// unified campaign before Fallout initializes its database layer, then make the
// selected user-owned Fallout installation the active content root. This lets
// the existing CE database code continue resolving master.dat, critter.dat,
// data/, music, scripts, and other stock assets without bundling any content.
inline int unifiedCampaignGameInitWithOptions(const char* windowTitle,
    bool isMapper,
    int font,
    int a4,
    int argc,
    char** argv)
{
    unifiedCampaignConfigureFromArgs(argc, argv);
    if (!unifiedCampaignActivateContentRoot()) {
        return -1;
    }

    return gameInitWithOptions(
        unifiedCampaignGetWindowTitle(windowTitle),
        isMapper,
        font,
        a4,
        argc,
        argv);
}

int falloutMain(int argc, char** argv);

} // namespace fallout

// main.cc includes input.h/game.h after main.h, but these headers have already
// been pulled in by the cooperative runtime. Redirect only main.cc's call sites
// without altering the low-level implementations used elsewhere.
#define inputGetInput localCoopMainInputGetInput
#define gameInitWithOptions unifiedCampaignGameInitWithOptions

#endif /* MAIN_H */
