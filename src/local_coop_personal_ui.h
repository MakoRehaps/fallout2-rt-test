#ifndef LOCAL_COOP_PERSONAL_UI_H
#define LOCAL_COOP_PERSONAL_UI_H

#include <array>
#include "item.h"
#include "local_coop.h"
#include "window_manager.h"

namespace fallout {

// COOP_FOUR_PERSONAL_HUD_SHARED_BAG_V1
struct LocalCoopPersonalUiState {
    int hudWindow = -1;
    int inventoryWindow = -1;
    int selectedItem = 0;
    int scroll = 0;
    int equipHand = HAND_RIGHT;
    bool backWasDown = false;
    bool upWasDown = false;
    bool downWasDown = false;
    bool leftWasDown = false;
    bool rightWasDown = false;
    bool aWasDown = false;
    bool bWasDown = false;
    bool xWasDown = false;
    Uint32 nextRefreshTick = 0;
};

inline std::array<LocalCoopPersonalUiState, kLocalCoopMaxPlayers> gLocalCoopPersonalUi;

inline void localCoopPersonalUiCloseInventory(int slot)
{
    auto& ui = gLocalCoopPersonalUi[slot];
    if (ui.inventoryWindow != -1) windowDestroy(ui.inventoryWindow);
    ui.inventoryWindow = -1;
    if (gLocalCoopPlayers[slot].uiMode == LocalCoopUiMode::Inventory) {
        gLocalCoopPlayers[slot].uiMode = LocalCoopUiMode::World;
    }
}

inline void localCoopPersonalUiShutdown()
{
    for (int slot = 0; slot < kLocalCoopMaxPlayers; slot++) {
        auto& ui = gLocalCoopPersonalUi[slot];
        if (ui.hudWindow != -1) windowDestroy(ui.hudWindow);
        if (ui.inventoryWindow != -1) windowDestroy(ui.inventoryWindow);
        ui = LocalCoopPersonalUiState {};
    }
}

} // namespace fallout

#endif
