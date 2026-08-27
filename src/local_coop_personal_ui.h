#ifndef LOCAL_COOP_PERSONAL_UI_H
#define LOCAL_COOP_PERSONAL_UI_H

#include <algorithm>
#include <array>
#include "critter.h"
#include "item.h"
#include "local_coop.h"
#include "proto.h"
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

inline void localCoopPersonalUiHudRect(int slot, int& x, int& y, int& width, int& height)
{
    int sw = std::max(640, screenGetWidth());
    int sh = std::max(480, screenGetHeight());
    width = std::min(360, std::max(280, sw / 3));
    height = 112;
    x = (slot & 1) ? sw - width : 0;
    y = slot >= 2 ? sh - INTERFACE_BAR_HEIGHT - height : 0;
}

inline void localCoopPersonalUiInventoryRect(int slot, int& x, int& y, int& width, int& height)
{
    int sw = std::max(640, screenGetWidth());
    int sh = std::max(480, screenGetHeight());
    width = std::max(310, sw / 2 - 8);
    height = std::max(230, sh / 2 - 8);
    x = (slot & 1) ? sw - width : 0;
    y = slot >= 2 ? sh - height : 0;
}

inline void localCoopPersonalUiCloseInventory(int slot)
{
    auto& ui = gLocalCoopPersonalUi[slot];
    if (ui.inventoryWindow != -1) windowDestroy(ui.inventoryWindow);
    ui.inventoryWindow = -1;
    if (gLocalCoopPlayers[slot].uiMode == LocalCoopUiMode::Inventory) {
        gLocalCoopPlayers[slot].uiMode = LocalCoopUiMode::World;
    }
}

inline void localCoopPersonalUiOpenInventory(int slot)
{
    auto& player = gLocalCoopPlayers[slot];
    auto& ui = gLocalCoopPersonalUi[slot];
    if (!player.connected || !player.humanOwned || player.actor == nullptr || ui.inventoryWindow != -1) return;
    int x, y, w, h;
    localCoopPersonalUiInventoryRect(slot, x, y, w, h);
    ui.inventoryWindow = windowCreate(x, y, w, h, _colorTable[0], WINDOW_MOVE_ON_TOP);
    if (ui.inventoryWindow == -1) return;
    ui.equipHand = localCoopGetActiveHand(player);
    player.uiMode = LocalCoopUiMode::Inventory;
}

inline void localCoopPersonalUiDrawHud(int slot)
{
    auto& ui = gLocalCoopPersonalUi[slot];
    int x, y, w, h;
    localCoopPersonalUiHudRect(slot, x, y, w, h);
    if (ui.hudWindow == -1) ui.hudWindow = windowCreate(x, y, w, h, _colorTable[0], WINDOW_MOVE_ON_TOP);
    if (ui.hudWindow == -1) return;

    windowFill(ui.hudWindow, 0, 0, w, h, _colorTable[0]);
    windowDrawBorder(ui.hudWindow);
    auto& player = gLocalCoopPlayers[slot];
    char line[192];
    snprintf(line, sizeof(line), "P%d  %s%s", slot + 1,
        player.connected ? "CONNECTED" : (player.slotLocked ? "RESERVED" : "EMPTY"),
        ui.inventoryWindow != -1 ? "  [BAG]" : "");
    windowDrawText(ui.hudWindow, line, w - 16, 8, 7, _colorTable[992]);

    Object* actor = player.actor;
    if (actor == nullptr || !player.slotLocked) {
        windowDrawText(ui.hudWindow, "NO CHARACTER", w - 16, 8, 31, _colorTable[992]);
        windowRefresh(ui.hudWindow);
        return;
    }

    int maxHp = std::max(1, critterGetStat(actor, STAT_MAXIMUM_HIT_POINTS));
    snprintf(line, sizeof(line), "HP %d/%d", actor->data.critter.hp, maxHp);
    windowDrawText(ui.hudWindow, line, w - 16, 8, 29, _colorTable[992]);

    Object* item = localCoopGetActiveItem(player);
    const char* itemName = item != nullptr ? protoGetName(item->pid) : nullptr;
    if (itemName == nullptr || *itemName == '\0') itemName = "UNARMED";
    snprintf(line, sizeof(line), "%s HAND: %s", localCoopGetActiveHand(player) == HAND_LEFT ? "LEFT" : "RIGHT", itemName);
    windowDrawText(ui.hudWindow, line, w - 16, 8, 51, _colorTable[992]);

    snprintf(line, sizeof(line), "%s   BACK: BAG", player.actionMode == LocalCoopActionMode::Aim ? "AIM" : "INTERACT");
    windowDrawText(ui.hudWindow, line, w - 16, 8, 75, _colorTable[992]);
    windowRefresh(ui.hudWindow);
}

inline void localCoopPersonalUiDrawInventory(int slot)
{
    auto& ui = gLocalCoopPersonalUi[slot];
    if (ui.inventoryWindow == -1) return;
    Object* shared = localCoopGetSharedInventoryOwner();
    if (shared == nullptr) return;
    Inventory& inv = shared->data.inventory;
    if (inv.length > 0) ui.selectedItem = std::clamp(ui.selectedItem, 0, inv.length - 1);
    else ui.selectedItem = 0;

    int w = windowGetWidth(ui.inventoryWindow);
    int h = windowGetHeight(ui.inventoryWindow);
    windowFill(ui.inventoryWindow, 0, 0, w, h, _colorTable[0]);
    windowDrawBorder(ui.inventoryWindow);
    char line[256];
    snprintf(line, sizeof(line), "P%d SHARED BAG  -> %s HAND", slot + 1, ui.equipHand == HAND_LEFT ? "LEFT" : "RIGHT");
    windowDrawText(ui.inventoryWindow, line, w - 20, 10, 10, _colorTable[992]);
    windowDrawText(ui.inventoryWindow, "UP/DOWN SELECT  LEFT/RIGHT HAND  A EQUIP  X UNEQUIP  B/BACK CLOSE", w - 20, 10, 31, _colorTable[992]);

    constexpr int visible = 10;
    if (ui.selectedItem < ui.scroll) ui.scroll = ui.selectedItem;
    if (ui.selectedItem >= ui.scroll + visible) ui.scroll = ui.selectedItem - visible + 1;
    int last = std::min(inv.length, ui.scroll + visible);
    int y = 58;
    for (int i = ui.scroll; i < last; i++, y += 18) {
        InventoryItem& entry = inv.items[i];
        const char* name = entry.item != nullptr ? protoGetName(entry.item->pid) : "UNKNOWN";
        snprintf(line, sizeof(line), "%c %s  x%d", i == ui.selectedItem ? '>' : ' ', name != nullptr ? name : "UNKNOWN", entry.quantity);
        windowDrawText(ui.inventoryWindow, line, w - 24, 12, y, _colorTable[992]);
    }
    windowRefresh(ui.inventoryWindow);
}

inline void localCoopPersonalUiTick()
{
    for (int slot = 0; slot < kLocalCoopMaxPlayers; slot++) {
        auto& player = gLocalCoopPlayers[slot];
        auto& ui = gLocalCoopPersonalUi[slot];
        localCoopPersonalUiDrawHud(slot);
        if (!player.connected || !player.humanOwned || player.controller == nullptr) {
            if (ui.inventoryWindow != -1) localCoopPersonalUiCloseInventory(slot);
            continue;
        }

        SDL_GameController* pad = player.controller;
        bool back = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_BACK) != 0;
        bool up = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP) != 0;
        bool down = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN) != 0;
        bool left = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT) != 0;
        bool right = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) != 0;
        bool a = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A) != 0;
        bool b = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B) != 0;
        bool x = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_X) != 0;

        if (back && !ui.backWasDown) {
            if (ui.inventoryWindow == -1 && player.uiMode == LocalCoopUiMode::World) localCoopPersonalUiOpenInventory(slot);
            else if (ui.inventoryWindow != -1) localCoopPersonalUiCloseInventory(slot);
        }

        if (ui.inventoryWindow != -1) {
            Object* shared = localCoopGetSharedInventoryOwner();
            int count = shared != nullptr ? shared->data.inventory.length : 0;
            if (b && !ui.bWasDown) {
                localCoopPersonalUiCloseInventory(slot);
            } else {
                if (count > 0 && up && !ui.upWasDown) ui.selectedItem = (ui.selectedItem + count - 1) % count;
                if (count > 0 && down && !ui.downWasDown) ui.selectedItem = (ui.selectedItem + 1) % count;
                if (left && !ui.leftWasDown) ui.equipHand = HAND_LEFT;
                if (right && !ui.rightWasDown) ui.equipHand = HAND_RIGHT;
                if (count > 0 && a && !ui.aWasDown && shared != nullptr) {
                    Object* selected = shared->data.inventory.items[ui.selectedItem].item;
                    if (selected != nullptr) localCoopEquipSharedItem(slot, selected, ui.equipHand);
                }
                if (x && !ui.xWasDown) localCoopUnequipToSharedPool(slot, ui.equipHand);
                localCoopPersonalUiDrawInventory(slot);
            }
        }

        ui.backWasDown = back;
        ui.upWasDown = up;
        ui.downWasDown = down;
        ui.leftWasDown = left;
        ui.rightWasDown = right;
        ui.aWasDown = a;
        ui.bWasDown = b;
        ui.xWasDown = x;
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
