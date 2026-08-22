#ifndef LOCAL_COOP_INVENTORY_UI_H
#define LOCAL_COOP_INVENTORY_UI_H

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cstdio>

#include "color.h"
#include "critter.h"
#include "input.h"
#include "inventory.h"
#include "item.h"
#include "local_coop.h"
#include "object.h"
#include "proto_instance.h"
#include "svga.h"
#include "window_manager.h"

namespace fallout {

inline constexpr int kLocalCoopHandLeft = 0;
inline constexpr int kLocalCoopHandRight = 1;

struct LocalCoopInventoryUiSlot {
    int selectedSharedIndex = 0;
    bool backWasDown = false;
    bool upWasDown = false;
    bool downWasDown = false;
    bool confirmWasDown = false;
    bool rightUnequipWasDown = false;
    bool leftUnequipWasDown = false;
    bool useWasDown = false;
    bool dropWasDown = false;
};

inline std::array<LocalCoopInventoryUiSlot, kLocalCoopMaxPlayers> gLocalCoopInventoryUiSlots;
inline int gLocalCoopInventoryWindow = -1;
inline int gLocalCoopInventoryWindowWidth = 0;
inline int gLocalCoopInventoryWindowHeight = 0;
inline bool gLocalCoopInventoryTickerInstalled = false;
inline bool gLocalCoopInventoryInsideTick = false;

inline bool localCoopAnyInventoryUiOpen()
{
    for (const LocalCoopPlayer& player : gLocalCoopPlayers) {
        if (player.uiMode == LocalCoopUiMode::Inventory) {
            return true;
        }
    }

    return false;
}

inline int localCoopSharedInventoryVisibleCount()
{
    Object* sharedOwner = localCoopGetSharedInventoryOwner();
    if (sharedOwner == nullptr) {
        return 0;
    }

    int count = 0;
    Inventory& inventory = sharedOwner->data.inventory;
    for (int index = 0; index < inventory.length; index++) {
        Object* item = inventory.items[index].item;
        if (item == nullptr || (item->flags & OBJECT_EQUIPPED) != 0) {
            continue;
        }
        count++;
    }

    return count;
}

inline Object* localCoopSharedInventoryVisibleItem(int visibleIndex, int* quantityPtr = nullptr)
{
    if (quantityPtr != nullptr) {
        *quantityPtr = 0;
    }

    Object* sharedOwner = localCoopGetSharedInventoryOwner();
    if (sharedOwner == nullptr || visibleIndex < 0) {
        return nullptr;
    }

    int current = 0;
    Inventory& inventory = sharedOwner->data.inventory;
    for (int index = 0; index < inventory.length; index++) {
        InventoryItem& entry = inventory.items[index];
        Object* item = entry.item;
        if (item == nullptr || (item->flags & OBJECT_EQUIPPED) != 0) {
            continue;
        }

        if (current == visibleIndex) {
            if (quantityPtr != nullptr) {
                *quantityPtr = entry.quantity;
            }
            return item;
        }

        current++;
    }

    return nullptr;
}

inline const char* localCoopUiItemName(Object* item)
{
    return item != nullptr ? itemGetName(item) : "-";
}

inline bool localCoopEquipSelectedSharedItem(int slot)
{
    if (slot < 0 || slot >= kLocalCoopMaxPlayers) {
        return false;
    }

    LocalCoopPlayer& player = gLocalCoopPlayers[slot];
    Object* actor = player.actor;
    Object* sharedOwner = localCoopGetSharedInventoryOwner();
    if (actor == nullptr || sharedOwner == nullptr) {
        return false;
    }

    LocalCoopInventoryUiSlot& ui = gLocalCoopInventoryUiSlots[slot];
    Object* item = localCoopSharedInventoryVisibleItem(ui.selectedSharedIndex);
    if (item == nullptr) {
        return false;
    }

    int itemType = itemGetType(item);
    if (itemType == ITEM_TYPE_ARMOR) {
        if (actor != sharedOwner && item->owner != actor) {
            if (itemMoveForce(sharedOwner, actor, item, 1) != 0) {
                return false;
            }
        }

        if (_invenWieldFunc(actor, item, kLocalCoopHandRight, true) != 0) {
            if (actor != sharedOwner && item->owner == actor) {
                itemMoveForce(actor, sharedOwner, item, 1);
            }
            return false;
        }
        return true;
    }

    if (itemType == ITEM_TYPE_WEAPON) {
        return localCoopEquipSharedItem(slot, item, kLocalCoopHandRight);
    }

    return false;
}

inline bool localCoopUseSelectedSharedItem(int slot)
{
    if (slot < 0 || slot >= kLocalCoopMaxPlayers) {
        return false;
    }

    LocalCoopPlayer& player = gLocalCoopPlayers[slot];
    Object* actor = player.actor;
    Object* sharedOwner = localCoopGetSharedInventoryOwner();
    if (actor == nullptr || sharedOwner == nullptr) {
        return false;
    }

    Object* item = localCoopSharedInventoryVisibleItem(
        gLocalCoopInventoryUiSlots[slot].selectedSharedIndex);
    if (item == nullptr) {
        return false;
    }

    int itemType = itemGetType(item);
    if (itemType != ITEM_TYPE_DRUG
        && itemType != ITEM_TYPE_MISC
        && itemType != ITEM_TYPE_WEAPON) {
        return false;
    }

    bool borrowed = actor != sharedOwner;
    if (borrowed && itemMoveForce(sharedOwner, actor, item, 1) != 0) {
        return false;
    }

    bool success = false;
    bool consumed = false;

    if (itemType == ITEM_TYPE_DRUG) {
        if (_item_d_take_drug(actor, item) != 0) {
            _obj_destroy(item);
            success = true;
            consumed = true;
        }
    } else {
        // Match stock inventory semantics: temporarily detach the object before
        // executing its use script/proto hook, then put it back if it was not
        // consumed. This keeps scripted misc/weapon use behavior intact.
        if (itemRemove(actor, item, 1) == 0) {
            int useResult = _protinst_use_item(actor, item);
            if (useResult == 1) {
                _obj_destroy(item);
                consumed = true;
                success = true;
            } else {
                itemAdd(actor, item, 1);
                success = useResult != -1;
            }
        }
    }

    if (borrowed && !consumed && item->owner == actor) {
        itemMoveForce(actor, sharedOwner, item, 1);
    }

    localCoopSweepSharedInventory();
    return success;
}

inline bool localCoopDropSelectedSharedItem(int slot)
{
    if (slot < 0 || slot >= kLocalCoopMaxPlayers) {
        return false;
    }

    LocalCoopPlayer& player = gLocalCoopPlayers[slot];
    Object* actor = player.actor;
    Object* sharedOwner = localCoopGetSharedInventoryOwner();
    if (actor == nullptr || sharedOwner == nullptr || actor->tile == -1) {
        return false;
    }

    Object* item = localCoopSharedInventoryVisibleItem(
        gLocalCoopInventoryUiSlots[slot].selectedSharedIndex);
    if (item == nullptr || item->pid == PROTO_ID_MONEY) {
        // Stock money dropping has special amount bookkeeping. Keep caps in the
        // shared pool until the live quantity selector is implemented.
        return false;
    }

    if (actor != sharedOwner) {
        if (itemMoveForce(sharedOwner, actor, item, 1) != 0) {
            return false;
        }
    }

    int rc = _obj_drop(actor, item);
    if (rc != 0 && actor != sharedOwner && item->owner == actor) {
        itemMoveForce(actor, sharedOwner, item, 1);
    }

    localCoopSweepSharedInventory();
    return rc == 0;
}

inline void localCoopInventoryUiClampSelection(int slot)
{
    int count = localCoopSharedInventoryVisibleCount();
    LocalCoopInventoryUiSlot& ui = gLocalCoopInventoryUiSlots[slot];
    if (count <= 0) {
        ui.selectedSharedIndex = 0;
    } else {
        ui.selectedSharedIndex = std::max(0, std::min(ui.selectedSharedIndex, count - 1));
    }
}

inline void localCoopInventoryUiProcessInput()
{
    for (int slot = 0; slot < kLocalCoopMaxPlayers; slot++) {
        LocalCoopPlayer& player = gLocalCoopPlayers[slot];
        LocalCoopInventoryUiSlot& ui = gLocalCoopInventoryUiSlots[slot];

        if (!player.connected || player.controller == nullptr || !player.humanOwned || player.actor == nullptr) {
            continue;
        }

        bool backDown = SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_BACK) != 0;
        bool upDown = SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_DPAD_UP) != 0;
        bool downDown = SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN) != 0;
        bool confirmDown = SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_A) != 0;
        bool rightUnequipDown = SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_B) != 0;
        bool leftUnequipDown = SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_Y) != 0;
        bool useDown = SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_X) != 0;
        bool dropDown = SDL_GameControllerGetAxis(player.controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 12000;

        if (backDown && !ui.backWasDown) {
            if (player.uiMode == LocalCoopUiMode::Inventory) {
                player.uiMode = LocalCoopUiMode::World;
            } else if (player.uiMode == LocalCoopUiMode::World) {
                player.uiMode = LocalCoopUiMode::Inventory;
            }
        }

        if (player.uiMode == LocalCoopUiMode::Inventory) {
            if (upDown && !ui.upWasDown) {
                ui.selectedSharedIndex--;
                localCoopInventoryUiClampSelection(slot);
            }

            if (downDown && !ui.downWasDown) {
                ui.selectedSharedIndex++;
                localCoopInventoryUiClampSelection(slot);
            }

            if (confirmDown && !ui.confirmWasDown) {
                localCoopEquipSelectedSharedItem(slot);
                localCoopInventoryUiClampSelection(slot);
            }

            if (rightUnequipDown && !ui.rightUnequipWasDown) {
                localCoopUnequipToSharedPool(slot, kLocalCoopHandRight);
                localCoopInventoryUiClampSelection(slot);
            }

            if (leftUnequipDown && !ui.leftUnequipWasDown) {
                localCoopUnequipToSharedPool(slot, kLocalCoopHandLeft);
                localCoopInventoryUiClampSelection(slot);
            }

            if (useDown && !ui.useWasDown) {
                localCoopUseSelectedSharedItem(slot);
                localCoopInventoryUiClampSelection(slot);
            }

            if (dropDown && !ui.dropWasDown) {
                localCoopDropSelectedSharedItem(slot);
                localCoopInventoryUiClampSelection(slot);
            }
        }

        ui.backWasDown = backDown;
        ui.upWasDown = upDown;
        ui.downWasDown = downDown;
        ui.confirmWasDown = confirmDown;
        ui.rightUnequipWasDown = rightUnequipDown;
        ui.leftUnequipWasDown = leftUnequipDown;
        ui.useWasDown = useDown;
        ui.dropWasDown = dropDown;
    }
}

inline void localCoopInventoryUiDestroyWindow()
{
    if (gLocalCoopInventoryWindow != -1) {
        windowDestroy(gLocalCoopInventoryWindow);
        gLocalCoopInventoryWindow = -1;
        gLocalCoopInventoryWindowWidth = 0;
        gLocalCoopInventoryWindowHeight = 0;
    }
}

inline bool localCoopInventoryUiEnsureWindow()
{
    if (!gWindowSystemInitialized) {
        return false;
    }

    int width = screenGetWidth();
    int height = std::min(screenGetHeight(), 300);
    if (width <= 0 || height <= 0) {
        return false;
    }

    if (gLocalCoopInventoryWindow != -1
        && (gLocalCoopInventoryWindowWidth != width || gLocalCoopInventoryWindowHeight != height)) {
        localCoopInventoryUiDestroyWindow();
    }

    if (gLocalCoopInventoryWindow == -1) {
        gLocalCoopInventoryWindow = windowCreate(0, 0, width, height, _colorTable[0], WINDOW_MOVE_ON_TOP);
        if (gLocalCoopInventoryWindow == -1) {
            return false;
        }
        gLocalCoopInventoryWindowWidth = width;
        gLocalCoopInventoryWindowHeight = height;
    }

    return true;
}

inline void localCoopInventoryUiRender()
{
    if (!localCoopAnyInventoryUiOpen()) {
        localCoopInventoryUiDestroyWindow();
        return;
    }

    if (!localCoopInventoryUiEnsureWindow()) {
        return;
    }

    int win = gLocalCoopInventoryWindow;
    int width = gLocalCoopInventoryWindowWidth;
    int height = gLocalCoopInventoryWindowHeight;
    int textColor = _colorTable[992];
    int dimColor = _colorTable[992];

    windowFill(win, 0, 0, width, height, _colorTable[0]);
    windowDrawBorder(win);
    windowDrawText(win, "CO-OP SHARED INVENTORY - LIVE / NON-PAUSING", width - 20, 10, 8, textColor);

    int columnWidth = std::max(120, width / kLocalCoopMaxPlayers);
    for (int slot = 0; slot < kLocalCoopMaxPlayers; slot++) {
        int x = slot * columnWidth + 8;
        LocalCoopPlayer& player = gLocalCoopPlayers[slot];
        Object* actor = player.actor;

        char line[160];
        if (actor != nullptr) {
            std::snprintf(line,
                sizeof(line),
                "P%d %s%s",
                slot + 1,
                critterGetName(actor),
                player.uiMode == LocalCoopUiMode::Inventory ? " [EDIT]" : "");
        } else {
            std::snprintf(line, sizeof(line), "P%d -", slot + 1);
        }
        windowDrawText(win, line, columnWidth - 16, x, 28, textColor);

        if (actor != nullptr) {
            std::snprintf(line, sizeof(line), "ARM: %s", localCoopUiItemName(critterGetArmor(actor)));
            windowDrawText(win, line, columnWidth - 16, x, 46, dimColor);
            std::snprintf(line, sizeof(line), "L: %s", localCoopUiItemName(critterGetItem1(actor)));
            windowDrawText(win, line, columnWidth - 16, x, 62, dimColor);
            std::snprintf(line, sizeof(line), "R: %s", localCoopUiItemName(critterGetItem2(actor)));
            windowDrawText(win, line, columnWidth - 16, x, 78, dimColor);
        }
    }

    int sharedTop = 104;
    windowDrawLine(win, 6, sharedTop - 4, width - 7, sharedTop - 4, textColor);
    windowDrawText(win,
        "SHARED POOL  D-pad select / A equip / X use / RT drop / B right off / Y left off / Back close",
        width - 20,
        10,
        sharedTop,
        textColor);

    int visibleCount = localCoopSharedInventoryVisibleCount();
    int rowHeight = 18;
    int maxRows = std::max(1, (height - sharedTop - 28) / rowHeight);

    int firstRow = 0;
    for (int slot = 0; slot < kLocalCoopMaxPlayers; slot++) {
        if (gLocalCoopPlayers[slot].uiMode == LocalCoopUiMode::Inventory) {
            int selected = gLocalCoopInventoryUiSlots[slot].selectedSharedIndex;
            if (selected >= firstRow + maxRows) {
                firstRow = selected - maxRows + 1;
            }
        }
    }

    for (int row = 0; row < maxRows; row++) {
        int visibleIndex = firstRow + row;
        if (visibleIndex >= visibleCount) {
            break;
        }

        int quantity = 0;
        Object* item = localCoopSharedInventoryVisibleItem(visibleIndex, &quantity);
        if (item == nullptr) {
            continue;
        }

        char selectors[24];
        int selectorOffset = 0;
        for (int slot = 0; slot < kLocalCoopMaxPlayers; slot++) {
            if (gLocalCoopPlayers[slot].uiMode == LocalCoopUiMode::Inventory
                && gLocalCoopInventoryUiSlots[slot].selectedSharedIndex == visibleIndex) {
                selectorOffset += std::snprintf(selectors + selectorOffset,
                    sizeof(selectors) - selectorOffset,
                    "P%d ",
                    slot + 1);
            }
        }
        if (selectorOffset == 0) {
            std::snprintf(selectors, sizeof(selectors), "   ");
        }

        char line[256];
        std::snprintf(line,
            sizeof(line),
            "%s%-36s x%d",
            selectors,
            itemGetName(item),
            quantity);
        windowDrawText(win, line, width - 24, 12, sharedTop + 22 + row * rowHeight, textColor);
    }

    if (visibleCount == 0) {
        windowDrawText(win, "(shared pool empty)", width - 24, 12, sharedTop + 22, textColor);
    }

    windowShow(win);
    windowRefresh(win);
}

inline void localCoopInventoryUiTick()
{
    if (gLocalCoopInventoryInsideTick) {
        return;
    }

    gLocalCoopInventoryInsideTick = true;
    localCoopInventoryUiProcessInput();
    localCoopInventoryUiRender();
    gLocalCoopInventoryInsideTick = false;
}

inline void localCoopInventoryUiTicker()
{
    localCoopInventoryUiTick();
}

inline void localCoopInventoryUiEnsureTicker()
{
    if (!gLocalCoopInventoryTickerInstalled) {
        tickersAdd(localCoopInventoryUiTicker);
        gLocalCoopInventoryTickerInstalled = true;
    }
}

} // namespace fallout

#endif /* LOCAL_COOP_INVENTORY_UI_H */
