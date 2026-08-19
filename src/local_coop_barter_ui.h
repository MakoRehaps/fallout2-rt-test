#ifndef LOCAL_COOP_BARTER_UI_H
#define LOCAL_COOP_BARTER_UI_H

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

#include "color.h"
#include "critter.h"
#include "game.h"
#include "game_dialog.h"
#include "input.h"
#include "item.h"
#include "kb.h"
#include "local_coop_controller_bridge.h"
#include "party_member.h"
#include "perk.h"
#include "reaction.h"
#include "skill.h"
#include "stat.h"
#include "svga.h"
#include "window_manager.h"

namespace fallout {

extern Object* gDude;

struct LocalCoopBarterUiState {
    int windowId = -1;
    int width = 0;
    int height = 0;
    int activePane = 0;
    std::array<int, 4> selected = {};

    bool upWasDown = false;
    bool downWasDown = false;
    bool leftWasDown = false;
    bool rightWasDown = false;
    bool confirmWasDown = false;
    bool stackWasDown = false;
    bool tradeWasDown = false;
    bool cancelWasDown = false;

    char status[160] = "Build an offer, then press Y to trade.";
};

inline void localCoopBarterSetStatus(LocalCoopBarterUiState& state, const char* text)
{
    std::snprintf(state.status, sizeof(state.status), "%s", text != nullptr ? text : "");
}

inline bool localCoopBarterItemVisible(Object* item, bool hideEquipped)
{
    if (item == nullptr || itemIsHidden(item)) {
        return false;
    }

    if (hideEquipped && (item->flags & OBJECT_EQUIPPED) != 0) {
        return false;
    }

    return true;
}

inline int localCoopBarterVisibleCount(Object* owner, bool hideEquipped)
{
    if (owner == nullptr) {
        return 0;
    }

    int count = 0;
    Inventory& inventory = owner->data.inventory;
    for (int index = 0; index < inventory.length; index++) {
        if (localCoopBarterItemVisible(inventory.items[index].item, hideEquipped)) {
            count++;
        }
    }

    return count;
}

inline Object* localCoopBarterVisibleItem(Object* owner,
    bool hideEquipped,
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
        if (!localCoopBarterItemVisible(entry.item, hideEquipped)) {
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

inline int localCoopBarterReactionModifier(Object* barterer)
{
    switch (reactionTranslateValue(reactionGetValue(barterer))) {
    case NPC_REACTION_BAD:
        return 25;
    case NPC_REACTION_GOOD:
        return -15;
    default:
        return 0;
    }
}

inline int localCoopBarterComputeRequestedValue(Object* barterer,
    Object* bartererTable,
    int effectiveBarterMod)
{
    if (gGameDialogSpeakerIsPartyMember) {
        return objectGetInventoryWeight(bartererTable);
    }

    int cost = objectGetCost(bartererTable);
    int caps = itemGetTotalCaps(bartererTable);
    int nonCapsCost = cost - caps;

    double masterTraderBonus = perkHasRank(gDude, PERK_MASTER_TRADER) ? 25.0 : 0.0;
    int partyBarter = partyGetBestSkillValue(SKILL_BARTER);
    int npcBarter = skillGetValue(barterer, SKILL_BARTER);

    // This is the stock Fallout 2 barter formula from _barter_compute_value.
    double barterMultiplier = (effectiveBarterMod + 100.0 - masterTraderBonus) * 0.01;
    if (barterMultiplier < 0.0) {
        barterMultiplier = 0.0099999998;
    }

    double skillMultiplier = (160.0 + npcBarter) / (160.0 + partyBarter);
    return static_cast<int>(barterMultiplier * skillMultiplier * (nonCapsCost * 2.0) + caps);
}

inline void localCoopBarterClampSelections(LocalCoopBarterUiState& state,
    Object* barterer,
    Object* playerTable,
    Object* bartererTable)
{
    Object* owners[4] = { gDude, playerTable, bartererTable, barterer };
    bool hideEquipped[4] = { true, false, false, true };

    for (int pane = 0; pane < 4; pane++) {
        int count = localCoopBarterVisibleCount(owners[pane], hideEquipped[pane]);
        if (count <= 0) {
            state.selected[pane] = 0;
        } else {
            state.selected[pane] = std::max(0, std::min(state.selected[pane], count - 1));
        }
    }
}

inline bool localCoopBarterMoveSelected(LocalCoopBarterUiState& state,
    Object* barterer,
    Object* playerTable,
    Object* bartererTable,
    bool wholeStack)
{
    Object* from = nullptr;
    Object* to = nullptr;
    bool hideEquipped = false;

    switch (state.activePane) {
    case 0:
        from = gDude;
        to = playerTable;
        hideEquipped = true;
        break;
    case 1:
        from = playerTable;
        to = gDude;
        break;
    case 2:
        from = bartererTable;
        to = barterer;
        break;
    case 3:
        from = barterer;
        to = bartererTable;
        hideEquipped = true;
        break;
    default:
        return false;
    }

    int quantity = 0;
    Object* item = localCoopBarterVisibleItem(from,
        hideEquipped,
        state.selected[state.activePane],
        &quantity);
    if (item == nullptr || quantity <= 0) {
        localCoopBarterSetStatus(state, "Nothing selected.");
        return false;
    }

    int quantityToMove = wholeStack ? quantity : 1;
    if (itemMoveForce(from, to, item, quantityToMove) != 0) {
        localCoopBarterSetStatus(state, "That item could not be moved.");
        return false;
    }

    localCoopBarterSetStatus(state, wholeStack ? "Stack moved." : "Item moved.");
    localCoopBarterClampSelections(state, barterer, playerTable, bartererTable);
    return true;
}

inline bool localCoopBarterAttemptTransaction(LocalCoopBarterUiState& state,
    Object* barterer,
    Object* playerTable,
    Object* bartererTable,
    int effectiveBarterMod)
{
    int playerFreeWeight = critterGetStat(gDude, STAT_CARRY_WEIGHT) - objectGetInventoryWeight(gDude);
    if (objectGetInventoryWeight(bartererTable) > playerFreeWeight) {
        localCoopBarterSetStatus(state, "You cannot carry everything in their offer.");
        return false;
    }

    if (gGameDialogSpeakerIsPartyMember) {
        int bartererFreeWeight = critterGetStat(barterer, STAT_CARRY_WEIGHT)
            - objectGetInventoryWeight(barterer);
        if (objectGetInventoryWeight(playerTable) > bartererFreeWeight) {
            localCoopBarterSetStatus(state, "Your companion cannot carry that much.");
            return false;
        }
    } else {
        if (playerTable->data.inventory.length == 0) {
            localCoopBarterSetStatus(state, "Your offer is empty.");
            return false;
        }

        int offeredValue = objectGetCost(playerTable);
        int requestedValue = localCoopBarterComputeRequestedValue(barterer,
            bartererTable,
            effectiveBarterMod);
        if (requestedValue > offeredValue) {
            localCoopBarterSetStatus(state, "No, your offer is not good enough.");
            return false;
        }
    }

    itemMoveAll(bartererTable, gDude);
    itemMoveAll(playerTable, barterer);
    localCoopBarterSetStatus(state, "Trade accepted.");
    localCoopBarterClampSelections(state, barterer, playerTable, bartererTable);
    return true;
}

inline void localCoopBarterDestroyWindow(LocalCoopBarterUiState& state)
{
    if (state.windowId != -1) {
        windowDestroy(state.windowId);
        state.windowId = -1;
    }
}

inline bool localCoopBarterEnsureWindow(LocalCoopBarterUiState& state)
{
    if (!gWindowSystemInitialized) {
        return false;
    }

    int screenWidth = screenGetWidth();
    int screenHeight = screenGetHeight();
    state.width = std::min(screenWidth, 780);
    state.height = std::min(screenHeight, 420);

    if (state.width < 500 || state.height < 260) {
        return false;
    }

    if (state.windowId == -1) {
        int x = std::max(0, (screenWidth - state.width) / 2);
        int y = std::max(0, (screenHeight - state.height) / 2);
        state.windowId = windowCreate(x,
            y,
            state.width,
            state.height,
            _colorTable[0],
            WINDOW_MOVE_ON_TOP);
    }

    return state.windowId != -1;
}

inline void localCoopBarterRenderPane(LocalCoopBarterUiState& state,
    int pane,
    const char* title,
    Object* owner,
    bool hideEquipped,
    int x,
    int y,
    int width,
    int height)
{
    int textColor = _colorTable[992];
    bool active = state.activePane == pane;

    char header[80];
    std::snprintf(header, sizeof(header), "%s%s", active ? "> " : "  ", title);
    windowDrawText(state.windowId, header, width - 4, x + 2, y, textColor);

    int rowHeight = 18;
    int maxRows = std::max(1, (height - 24) / rowHeight);
    int count = localCoopBarterVisibleCount(owner, hideEquipped);
    int selected = state.selected[pane];
    int first = selected >= maxRows ? selected - maxRows + 1 : 0;

    for (int row = 0; row < maxRows; row++) {
        int visibleIndex = first + row;
        if (visibleIndex >= count) {
            break;
        }

        int quantity = 0;
        Object* item = localCoopBarterVisibleItem(owner,
            hideEquipped,
            visibleIndex,
            &quantity);
        if (item == nullptr) {
            continue;
        }

        char line[160];
        std::snprintf(line,
            sizeof(line),
            "%c %-18.18s x%d",
            active && visibleIndex == selected ? '>' : ' ',
            itemGetName(item),
            quantity);
        windowDrawText(state.windowId,
            line,
            width - 4,
            x + 2,
            y + 22 + row * rowHeight,
            textColor);
    }

    if (count == 0) {
        windowDrawText(state.windowId, "  (empty)", width - 4, x + 2, y + 22, textColor);
    }
}

inline void localCoopBarterRender(LocalCoopBarterUiState& state,
    Object* barterer,
    Object* playerTable,
    Object* bartererTable,
    int effectiveBarterMod)
{
    if (!localCoopBarterEnsureWindow(state)) {
        return;
    }

    int textColor = _colorTable[992];
    windowFill(state.windowId, 0, 0, state.width, state.height, _colorTable[0]);
    windowDrawBorder(state.windowId);

    char title[160];
    std::snprintf(title, sizeof(title), "LIVE BARTER - %s", critterGetName(barterer));
    windowDrawText(state.windowId, title, state.width - 20, 10, 8, textColor);

    int paneGap = 6;
    int paneWidth = (state.width - 20 - paneGap * 3) / 4;
    int paneTop = 38;
    int paneHeight = state.height - 112;

    localCoopBarterRenderPane(state, 0, "YOU HAVE", gDude, true, 10, paneTop, paneWidth, paneHeight);
    localCoopBarterRenderPane(state,
        1,
        "YOU OFFER",
        playerTable,
        false,
        10 + (paneWidth + paneGap),
        paneTop,
        paneWidth,
        paneHeight);
    localCoopBarterRenderPane(state,
        2,
        "THEY OFFER",
        bartererTable,
        false,
        10 + (paneWidth + paneGap) * 2,
        paneTop,
        paneWidth,
        paneHeight);
    localCoopBarterRenderPane(state,
        3,
        "THEY HAVE",
        barterer,
        true,
        10 + (paneWidth + paneGap) * 3,
        paneTop,
        paneWidth,
        paneHeight);

    char valueLine[200];
    if (gGameDialogSpeakerIsPartyMember) {
        std::snprintf(valueLine,
            sizeof(valueLine),
            "Your offer weight: %d   Their offer weight: %d",
            objectGetInventoryWeight(playerTable),
            objectGetInventoryWeight(bartererTable));
    } else {
        std::snprintf(valueLine,
            sizeof(valueLine),
            "Your offer: $%d   Their adjusted offer: $%d",
            objectGetCost(playerTable),
            localCoopBarterComputeRequestedValue(barterer, bartererTable, effectiveBarterMod));
    }
    windowDrawText(state.windowId, valueLine, state.width - 20, 10, state.height - 64, textColor);
    windowDrawText(state.windowId, state.status, state.width - 20, 10, state.height - 46, textColor);
    windowDrawText(state.windowId,
        "D-pad: select/pane   A: move 1   X: move stack   Y: trade   B: talk",
        state.width - 20,
        10,
        state.height - 28,
        textColor);

    windowShow(state.windowId);
    windowRefresh(state.windowId);
}

inline void localCoopInventoryOpenTrade(int win,
    Object* barterer,
    Object* playerTable,
    Object* bartererTable,
    int barterMod)
{
    SDL_GameController* controller = localCoopBridgeGetController(0);

    // Keep mouse/keyboard-only sessions on Fallout's untouched stock barter UI.
    if (controller == nullptr
        || barterer == nullptr
        || playerTable == nullptr
        || bartererTable == nullptr) {
        inventoryOpenTrade(win, barterer, playerTable, bartererTable, barterMod);
        return;
    }

    ScopedGameMode gm(GameMode::kBarter);
    LocalCoopBarterUiState state;
    localCoopBarterClampSelections(state, barterer, playerTable, bartererTable);

    int effectiveBarterMod = barterMod + localCoopBarterReactionModifier(barterer);
    bool done = false;

    while (!done && _game_user_wants_to_quit == 0) {
        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();
        controller = localCoopBridgeGetController(0);

        bool upDown = controller != nullptr
            && SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP) != 0;
        bool downDown = controller != nullptr
            && SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN) != 0;
        bool leftDown = controller != nullptr
            && SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT) != 0;
        bool rightDown = controller != nullptr
            && SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) != 0;
        bool confirmDown = controller != nullptr
            && SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A) != 0;
        bool stackDown = controller != nullptr
            && SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_X) != 0;
        bool tradeDown = controller != nullptr
            && SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_Y) != 0;
        bool cancelDown = controller != nullptr
            && SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_B) != 0;

        if ((leftDown && !state.leftWasDown) || keyCode == KEY_ARROW_LEFT) {
            state.activePane = (state.activePane + 3) % 4;
        }
        if ((rightDown && !state.rightWasDown) || keyCode == KEY_ARROW_RIGHT) {
            state.activePane = (state.activePane + 1) % 4;
        }
        if ((upDown && !state.upWasDown) || keyCode == KEY_ARROW_UP) {
            state.selected[state.activePane]--;
        }
        if ((downDown && !state.downWasDown) || keyCode == KEY_ARROW_DOWN) {
            state.selected[state.activePane]++;
        }

        localCoopBarterClampSelections(state, barterer, playerTable, bartererTable);

        if (confirmDown && !state.confirmWasDown) {
            localCoopBarterMoveSelected(state, barterer, playerTable, bartererTable, false);
        }
        if (stackDown && !state.stackWasDown) {
            localCoopBarterMoveSelected(state, barterer, playerTable, bartererTable, true);
        }
        if ((tradeDown && !state.tradeWasDown) || keyCode == KEY_LOWERCASE_M) {
            localCoopBarterAttemptTransaction(state,
                barterer,
                playerTable,
                bartererTable,
                effectiveBarterMod);
        }
        if ((cancelDown && !state.cancelWasDown)
            || keyCode == KEY_ESCAPE
            || keyCode == KEY_LOWERCASE_T) {
            done = true;
        }

        state.upWasDown = upDown;
        state.downWasDown = downDown;
        state.leftWasDown = leftDown;
        state.rightWasDown = rightDown;
        state.confirmWasDown = confirmDown;
        state.stackWasDown = stackDown;
        state.tradeWasDown = tradeDown;
        state.cancelWasDown = cancelDown;

        localCoopBarterRender(state,
            barterer,
            playerTable,
            bartererTable,
            effectiveBarterMod);

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    localCoopBarterDestroyWindow(state);
}

} // namespace fallout

#endif /* LOCAL_COOP_BARTER_UI_H */
