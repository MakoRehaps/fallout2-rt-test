#ifndef LOCAL_COOP_LOOT_UI_H
#define LOCAL_COOP_LOOT_UI_H

#include <SDL.h>

#include <algorithm>
#include <cstdio>

#include "animation.h"
#include "color.h"
#include "critter.h"
#include "item.h"
#include "local_coop.h"
#include "object.h"
#include "proto_instance.h"
#include "proto_types.h"
#include "scripts.h"
#include "svga.h"
#include "window_manager.h"

namespace fallout {

struct LocalCoopLiveLootState {
    bool open = false;
    int targetObjectId = -1;
    bool targetPane = true;
    int selectedSharedIndex = 0;
    int selectedTargetIndex = 0;

    bool upWasDown = false;
    bool downWasDown = false;
    bool leftWasDown = false;
    bool rightWasDown = false;
    bool confirmWasDown = false;
    bool stackWasDown = false;
    bool takeAllWasDown = false;
    bool cancelWasDown = false;

    int windowId = -1;
    int windowWidth = 0;
    int windowHeight = 0;
    bool tickerInstalled = false;
    bool insideTick = false;
};

inline LocalCoopLiveLootState gLocalCoopLiveLootState;

inline Object* localCoopLiveLootTarget()
{
    if (gLocalCoopLiveLootState.targetObjectId == -1) {
        return nullptr;
    }

    return objectFindById(gLocalCoopLiveLootState.targetObjectId);
}

inline bool localCoopLiveLootItemVisible(Object* item, bool sharedPane)
{
    if (item == nullptr || itemIsHidden(item)) {
        return false;
    }

    if (sharedPane && (item->flags & OBJECT_EQUIPPED) != 0) {
        return false;
    }

    return true;
}

inline int localCoopLiveLootVisibleCount(Object* owner, bool sharedPane)
{
    if (owner == nullptr) {
        return 0;
    }

    int count = 0;
    Inventory& inventory = owner->data.inventory;
    for (int index = 0; index < inventory.length; index++) {
        if (localCoopLiveLootItemVisible(inventory.items[index].item, sharedPane)) {
            count++;
        }
    }

    return count;
}

inline Object* localCoopLiveLootVisibleItem(Object* owner,
    bool sharedPane,
    int visibleIndex,
    int* quantityPtr = nullptr)
{
    if (quantityPtr != nullptr) {
        *quantityPtr = 0;
    }

    if (owner == nullptr || visibleIndex < 0) {
        return nullptr;
    }

    int current = 0;
    Inventory& inventory = owner->data.inventory;
    for (int index = 0; index < inventory.length; index++) {
        InventoryItem& entry = inventory.items[index];
        if (!localCoopLiveLootItemVisible(entry.item, sharedPane)) {
            continue;
        }

        if (current == visibleIndex) {
            if (quantityPtr != nullptr) {
                *quantityPtr = entry.quantity;
            }
            return entry.item;
        }

        current++;
    }

    return nullptr;
}

inline void localCoopLiveLootClampSelection()
{
    Object* sharedOwner = localCoopGetSharedInventoryOwner();
    Object* target = localCoopLiveLootTarget();

    int sharedCount = localCoopLiveLootVisibleCount(sharedOwner, true);
    int targetCount = localCoopLiveLootVisibleCount(target, false);

    if (sharedCount <= 0) {
        gLocalCoopLiveLootState.selectedSharedIndex = 0;
    } else {
        gLocalCoopLiveLootState.selectedSharedIndex = std::max(0,
            std::min(gLocalCoopLiveLootState.selectedSharedIndex, sharedCount - 1));
    }

    if (targetCount <= 0) {
        gLocalCoopLiveLootState.selectedTargetIndex = 0;
    } else {
        gLocalCoopLiveLootState.selectedTargetIndex = std::max(0,
            std::min(gLocalCoopLiveLootState.selectedTargetIndex, targetCount - 1));
    }
}

inline void localCoopLiveLootDestroyWindow()
{
    if (gLocalCoopLiveLootState.windowId != -1) {
        windowDestroy(gLocalCoopLiveLootState.windowId);
        gLocalCoopLiveLootState.windowId = -1;
        gLocalCoopLiveLootState.windowWidth = 0;
        gLocalCoopLiveLootState.windowHeight = 0;
    }
}

inline void localCoopLiveLootResetEdges()
{
    gLocalCoopLiveLootState.upWasDown = false;
    gLocalCoopLiveLootState.downWasDown = false;
    gLocalCoopLiveLootState.leftWasDown = false;
    gLocalCoopLiveLootState.rightWasDown = false;
    gLocalCoopLiveLootState.confirmWasDown = false;
    gLocalCoopLiveLootState.stackWasDown = false;
    gLocalCoopLiveLootState.takeAllWasDown = false;
    gLocalCoopLiveLootState.cancelWasDown = false;
}

inline void localCoopLiveLootClose()
{
    localCoopLiveLootDestroyWindow();

    gLocalCoopLiveLootState.open = false;
    gLocalCoopLiveLootState.targetObjectId = -1;
    gLocalCoopLiveLootState.targetPane = true;
    gLocalCoopLiveLootState.selectedSharedIndex = 0;
    gLocalCoopLiveLootState.selectedTargetIndex = 0;
    localCoopLiveLootResetEdges();

    if (gLocalCoopInitialized
        && gLocalCoopPlayers[0].uiMode == LocalCoopUiMode::Loot) {
        gLocalCoopPlayers[0].uiMode = LocalCoopUiMode::World;
    }
}

inline void localCoopLiveLootResetForLoad()
{
    localCoopLiveLootDestroyWindow();
    gLocalCoopLiveLootState.open = false;
    gLocalCoopLiveLootState.targetObjectId = -1;
    gLocalCoopLiveLootState.targetPane = true;
    gLocalCoopLiveLootState.selectedSharedIndex = 0;
    gLocalCoopLiveLootState.selectedTargetIndex = 0;
    gLocalCoopLiveLootState.insideTick = false;
    localCoopLiveLootResetEdges();
}

inline bool localCoopLiveLootRunPickupScript(Object* actor, Object* target)
{
    int sid = -1;
    if (_obj_sid(target, &sid) == -1) {
        return true;
    }

    scriptSetObjects(sid, actor, nullptr);
    scriptExecProc(sid, SCRIPT_PROC_PICKUP);

    Script* script = nullptr;
    if (scriptGetScript(sid, &script) == -1 || script == nullptr) {
        return false;
    }

    return !script->scriptOverrides;
}

inline bool localCoopLiveLootCanOpen(Object* actor, Object* target)
{
    if (actor == nullptr || target == nullptr || actor != gDude) {
        return false;
    }

    int type = PID_TYPE(target->pid);
    if (type == OBJ_TYPE_CRITTER) {
        if ((target->data.critter.combat.results & DAM_DEAD) == 0) {
            return false;
        }

        if (_critter_flag_check(target->pid, CRITTER_NO_STEAL)) {
            return false;
        }

        return true;
    }

    if (type == OBJ_TYPE_ITEM && itemGetType(target) == ITEM_TYPE_CONTAINER) {
        return !objectIsLocked(target);
    }

    return false;
}

inline bool localCoopLiveLootOpenNow(Object* actor, Object* target)
{
    if (!localCoopLiveLootCanOpen(actor, target)) {
        return false;
    }

    // Preserve the stock container USE script and open animation, but do not
    // invoke the blocking stock loot window. If already open, leave it open.
    if (PID_TYPE(target->pid) == OBJ_TYPE_ITEM
        && itemGetType(target) == ITEM_TYPE_CONTAINER
        && target->frame == 0) {
        if (_obj_use_container(actor, target) != 0) {
            return false;
        }
    }

    // Stock inventoryOpenLooting executes the target pickup proc before showing
    // inventory. Keep that script contract so quest containers and corpses can
    // override looting exactly as before.
    if (!localCoopLiveLootRunPickupScript(actor, target)) {
        return false;
    }

    gLocalCoopLiveLootState.open = true;
    gLocalCoopLiveLootState.targetObjectId = target->id;
    gLocalCoopLiveLootState.targetPane = true;
    gLocalCoopLiveLootState.selectedSharedIndex = 0;
    gLocalCoopLiveLootState.selectedTargetIndex = 0;
    localCoopLiveLootResetEdges();

    // Consume the opening A press so it cannot immediately transfer item zero.
    if (gLocalCoopPlayers[0].controller != nullptr) {
        gLocalCoopLiveLootState.confirmWasDown =
            SDL_GameControllerGetButton(gLocalCoopPlayers[0].controller, SDL_CONTROLLER_BUTTON_A) != 0;
    }

    gLocalCoopPlayers[0].uiMode = LocalCoopUiMode::Loot;
    localCoopLiveLootClampSelection();
    return true;
}

inline int localCoopLiveLootOpenCallback(void* actorPtr, void* targetPtr)
{
    Object* actor = static_cast<Object*>(actorPtr);
    Object* target = static_cast<Object*>(targetPtr);

    if (actor == nullptr || target == nullptr || objectGetDistanceBetween(actor, target) > 1) {
        return -1;
    }

    return localCoopLiveLootOpenNow(actor, target) ? 0 : -1;
}

inline bool localCoopLiveLootRequest(Object* actor, Object* target)
{
    if (!localCoopLiveLootCanOpen(actor, target)) {
        return false;
    }

    if (gLocalCoopLiveLootState.open) {
        localCoopLiveLootClose();
    }

    if (objectGetDistanceBetween(actor, target) <= 1) {
        return localCoopLiveLootOpenNow(actor, target);
    }

    if (animationIsBusy(actor)) {
        return false;
    }

    if (reg_anim_begin(ANIMATION_REQUEST_RESERVED) != 0) {
        return false;
    }

    int distance = objectGetDistanceBetween(actor, target);
    int rc = distance < 5
        ? animationRegisterMoveToObject(actor, target, -1, 0)
        : animationRegisterRunToObject(actor, target, -1, 0);

    if (rc == 0) {
        animationRegisterCallbackForced(actor,
            target,
            (AnimationCallback*)localCoopLiveLootOpenCallback,
            -1);
    }

    if (reg_anim_end() != 0) {
        return false;
    }

    return rc == 0;
}

inline bool localCoopLiveLootMoveSelected(bool wholeStack)
{
    Object* sharedOwner = localCoopGetSharedInventoryOwner();
    Object* target = localCoopLiveLootTarget();
    if (sharedOwner == nullptr || target == nullptr) {
        return false;
    }

    bool fromTarget = gLocalCoopLiveLootState.targetPane;
    Object* from = fromTarget ? target : sharedOwner;
    Object* to = fromTarget ? sharedOwner : target;
    bool sourceIsShared = !fromTarget;
    int selectedIndex = fromTarget
        ? gLocalCoopLiveLootState.selectedTargetIndex
        : gLocalCoopLiveLootState.selectedSharedIndex;

    int quantity = 0;
    Object* item = localCoopLiveLootVisibleItem(from,
        sourceIsShared,
        selectedIndex,
        &quantity);
    if (item == nullptr || quantity <= 0) {
        return false;
    }

    int quantityToMove = wholeStack ? quantity : 1;
    if (itemMove(from, to, item, quantityToMove) != 0) {
        return false;
    }

    if (fromTarget) {
        item->flags &= ~OBJECT_EQUIPPED;
        localCoopSweepSharedInventory();
    }

    localCoopLiveLootClampSelection();
    return true;
}

inline void localCoopLiveLootTakeAll()
{
    Object* sharedOwner = localCoopGetSharedInventoryOwner();
    Object* target = localCoopLiveLootTarget();
    if (sharedOwner == nullptr || target == nullptr) {
        return;
    }

    // Walk backwards because successful moves shrink the source inventory.
    Inventory& inventory = target->data.inventory;
    for (int index = inventory.length - 1; index >= 0; index--) {
        if (index >= inventory.length) {
            continue;
        }

        InventoryItem entry = inventory.items[index];
        Object* item = entry.item;
        if (!localCoopLiveLootItemVisible(item, false) || entry.quantity <= 0) {
            continue;
        }

        if (itemMove(target, sharedOwner, item, entry.quantity) == 0) {
            item->flags &= ~OBJECT_EQUIPPED;
        }
    }

    localCoopSweepSharedInventory();
    localCoopLiveLootClampSelection();
}

inline bool localCoopLiveLootEnsureWindow()
{
    if (!gWindowSystemInitialized || !gLocalCoopLiveLootState.open) {
        return false;
    }

    int screenWidth = screenGetWidth();
    int screenHeight = screenGetHeight();
    int width = std::min(760, std::max(520, screenWidth - 80));
    int height = std::min(360, std::max(260, screenHeight - 140));

    if (gLocalCoopLiveLootState.windowId != -1
        && (gLocalCoopLiveLootState.windowWidth != width
            || gLocalCoopLiveLootState.windowHeight != height)) {
        localCoopLiveLootDestroyWindow();
    }

    if (gLocalCoopLiveLootState.windowId == -1) {
        int x = std::max(0, (screenWidth - width) / 2);
        int y = std::max(0, (screenHeight - height) / 2);
        gLocalCoopLiveLootState.windowId = windowCreate(x,
            y,
            width,
            height,
            _colorTable[0],
            WINDOW_MOVE_ON_TOP);
        if (gLocalCoopLiveLootState.windowId == -1) {
            return false;
        }

        gLocalCoopLiveLootState.windowWidth = width;
        gLocalCoopLiveLootState.windowHeight = height;
    }

    return true;
}

inline void localCoopLiveLootRenderPane(int win,
    Object* owner,
    bool sharedPane,
    int selectedIndex,
    int x,
    int y,
    int width,
    int height,
    bool active)
{
    int textColor = _colorTable[992];
    const char* title = sharedPane ? "SHARED PARTY" : "TARGET";

    char header[160];
    std::snprintf(header, sizeof(header), "%s%s", active ? "> " : "  ", title);
    windowDrawText(win, header, width - 8, x + 4, y + 2, textColor);

    int rowHeight = 18;
    int maxRows = std::max(1, (height - 28) / rowHeight);
    int count = localCoopLiveLootVisibleCount(owner, sharedPane);
    int first = 0;
    if (selectedIndex >= first + maxRows) {
        first = selectedIndex - maxRows + 1;
    }

    for (int row = 0; row < maxRows; row++) {
        int visibleIndex = first + row;
        if (visibleIndex >= count) {
            break;
        }

        int quantity = 0;
        Object* item = localCoopLiveLootVisibleItem(owner,
            sharedPane,
            visibleIndex,
            &quantity);
        if (item == nullptr) {
            continue;
        }

        char line[256];
        std::snprintf(line,
            sizeof(line),
            "%c %-32s x%d",
            active && visibleIndex == selectedIndex ? '>' : ' ',
            itemGetName(item),
            quantity);
        windowDrawText(win,
            line,
            width - 8,
            x + 4,
            y + 24 + row * rowHeight,
            textColor);
    }

    if (count == 0) {
        windowDrawText(win, "  (empty)", width - 8, x + 4, y + 24, textColor);
    }
}

inline void localCoopLiveLootRender()
{
    if (!gLocalCoopLiveLootState.open) {
        localCoopLiveLootDestroyWindow();
        return;
    }

    Object* target = localCoopLiveLootTarget();
    Object* sharedOwner = localCoopGetSharedInventoryOwner();
    if (target == nullptr || sharedOwner == nullptr) {
        localCoopLiveLootClose();
        return;
    }

    if (!localCoopLiveLootEnsureWindow()) {
        return;
    }

    int win = gLocalCoopLiveLootState.windowId;
    int width = gLocalCoopLiveLootState.windowWidth;
    int height = gLocalCoopLiveLootState.windowHeight;
    int textColor = _colorTable[992];

    windowFill(win, 0, 0, width, height, _colorTable[0]);
    windowDrawBorder(win);

    char title[256];
    std::snprintf(title, sizeof(title), "LIVE LOOT - %s", objectGetName(target));
    windowDrawText(win, title, width - 20, 10, 8, textColor);

    int paneTop = 34;
    int paneBottomMargin = 40;
    int paneHeight = height - paneTop - paneBottomMargin;
    int gap = 12;
    int paneWidth = (width - 30 - gap) / 2;

    localCoopLiveLootRenderPane(win,
        sharedOwner,
        true,
        gLocalCoopLiveLootState.selectedSharedIndex,
        10,
        paneTop,
        paneWidth,
        paneHeight,
        !gLocalCoopLiveLootState.targetPane);

    localCoopLiveLootRenderPane(win,
        target,
        false,
        gLocalCoopLiveLootState.selectedTargetIndex,
        10 + paneWidth + gap,
        paneTop,
        paneWidth,
        paneHeight,
        gLocalCoopLiveLootState.targetPane);

    windowDrawText(win,
        "D-pad: select/pane   A: move 1   X: move stack   Y: take all   B: close",
        width - 20,
        10,
        height - 26,
        textColor);

    windowShow(win);
    windowRefresh(win);
}

inline void localCoopLiveLootProcessInput()
{
    if (!gLocalCoopLiveLootState.open
        || !gLocalCoopInitialized
        || !gLocalCoopPlayers[0].connected
        || gLocalCoopPlayers[0].controller == nullptr) {
        return;
    }

    SDL_GameController* controller = gLocalCoopPlayers[0].controller;
    bool upDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP) != 0;
    bool downDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN) != 0;
    bool leftDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT) != 0;
    bool rightDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) != 0;
    bool confirmDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A) != 0;
    bool stackDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_X) != 0;
    bool takeAllDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_Y) != 0;
    bool cancelDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_B) != 0;

    if (leftDown && !gLocalCoopLiveLootState.leftWasDown) {
        gLocalCoopLiveLootState.targetPane = false;
    }
    if (rightDown && !gLocalCoopLiveLootState.rightWasDown) {
        gLocalCoopLiveLootState.targetPane = true;
    }

    int& selectedIndex = gLocalCoopLiveLootState.targetPane
        ? gLocalCoopLiveLootState.selectedTargetIndex
        : gLocalCoopLiveLootState.selectedSharedIndex;

    if (upDown && !gLocalCoopLiveLootState.upWasDown) {
        selectedIndex--;
    }
    if (downDown && !gLocalCoopLiveLootState.downWasDown) {
        selectedIndex++;
    }
    localCoopLiveLootClampSelection();

    if (confirmDown && !gLocalCoopLiveLootState.confirmWasDown) {
        localCoopLiveLootMoveSelected(false);
    }
    if (stackDown && !gLocalCoopLiveLootState.stackWasDown) {
        localCoopLiveLootMoveSelected(true);
    }
    if (takeAllDown && !gLocalCoopLiveLootState.takeAllWasDown) {
        localCoopLiveLootTakeAll();
    }
    if (cancelDown && !gLocalCoopLiveLootState.cancelWasDown) {
        localCoopLiveLootClose();
        return;
    }

    gLocalCoopLiveLootState.upWasDown = upDown;
    gLocalCoopLiveLootState.downWasDown = downDown;
    gLocalCoopLiveLootState.leftWasDown = leftDown;
    gLocalCoopLiveLootState.rightWasDown = rightDown;
    gLocalCoopLiveLootState.confirmWasDown = confirmDown;
    gLocalCoopLiveLootState.stackWasDown = stackDown;
    gLocalCoopLiveLootState.takeAllWasDown = takeAllDown;
    gLocalCoopLiveLootState.cancelWasDown = cancelDown;
}

inline void localCoopLiveLootTick()
{
    if (gLocalCoopLiveLootState.insideTick) {
        return;
    }

    gLocalCoopLiveLootState.insideTick = true;

    if (gLocalCoopLiveLootState.open) {
        Object* target = localCoopLiveLootTarget();
        if (target == nullptr
            || gDude == nullptr
            || target->elevation != gDude->elevation
            || objectGetDistanceBetween(gDude, target) > 8) {
            localCoopLiveLootClose();
        } else {
            localCoopLiveLootProcessInput();
            localCoopLiveLootRender();
        }
    }

    gLocalCoopLiveLootState.insideTick = false;
}

inline void localCoopLiveLootTicker()
{
    localCoopLiveLootTick();
}

inline void localCoopLiveLootEnsureTicker()
{
    if (!gLocalCoopLiveLootState.tickerInstalled) {
        tickersAdd(localCoopLiveLootTicker);
        gLocalCoopLiveLootState.tickerInstalled = true;
    }
}

} // namespace fallout

#endif /* LOCAL_COOP_LOOT_UI_H */
