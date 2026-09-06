#ifndef LOCAL_COOP_GLOBAL_PERKS_H
#define LOCAL_COOP_GLOBAL_PERKS_H

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cstring>

#include "critter.h"
#include "debug.h"
#include "local_coop.h"
#include "perk.h"
#include "random.h"
#include "stat.h"
#include "window_manager.h"

namespace fallout {

// COOP_GLOBAL_PERKS_V1
// P1 owns the saved perk rank table. Every co-op player reads that same table,
// and every selected perk affects the whole local party. On every shared level
// gained, each reserved player gets one draft turn: three random valid perks,
// pick one, then that perk becomes global immediately.
inline constexpr int kLocalCoopGlobalPerkChoiceCount = 3;

struct LocalCoopGlobalPerkDraftState {
    bool initialized = false;
    int observedLevel = 1;
    int pendingLevelUps = 0;
    std::array<int, kLocalCoopMaxPlayers> chooserSlots { -1, -1, -1, -1 };
    int chooserCount = 0;
    int chooserIndex = 0;
    std::array<int, kLocalCoopGlobalPerkChoiceCount> choices { -1, -1, -1 };
    int selectedChoice = 0;
    int window = -1;
    bool upWasDown = false;
    bool downWasDown = false;
    bool confirmWasDown = false;
};

inline LocalCoopGlobalPerkDraftState gLocalCoopGlobalPerkDraft;

inline bool localCoopGlobalPerkSelectable(int perk)
{
    if (!perkIsValid(perk)) {
        return false;
    }

    // These three depend on a second stock character-editor dialog or can
    // recursively create more levels. Keep the draft deterministic and modal.
    if (perk == PERK_TAG || perk == PERK_MUTATE || perk == PERK_HERE_AND_NOW) {
        return false;
    }

    const char* name = perkGetName(perk);
    return name != nullptr
        && *name != '\0'
        && std::strcmp(name, "<unused>") != 0;
}

inline Object* localCoopGlobalPerkChooserActor(int slot)
{
    if (slot >= 0 && slot < kLocalCoopMaxPlayers) {
        Object* actor = gLocalCoopPlayers[slot].actor;
        if (actor != nullptr) {
            return actor;
        }
    }
    return gDude;
}

inline int localCoopGlobalPerkInputSlot(int chooserSlot)
{
    if (chooserSlot >= 0 && chooserSlot < kLocalCoopMaxPlayers) {
        LocalCoopPlayer& chooser = gLocalCoopPlayers[chooserSlot];
        if (chooser.connected && chooser.controller != nullptr) {
            return chooserSlot;
        }
    }

    // A reserved player can be temporarily disconnected. Do not deadlock the
    // whole level-up; P1 can make that reserved slot's global choice.
    if (gLocalCoopPlayers[0].connected && gLocalCoopPlayers[0].controller != nullptr) {
        return 0;
    }

    return -1;
}

inline void localCoopGlobalPerkDestroyWindow()
{
    if (gLocalCoopGlobalPerkDraft.window != -1) {
        windowDestroy(gLocalCoopGlobalPerkDraft.window);
        gLocalCoopGlobalPerkDraft.window = -1;
    }
}

inline void localCoopGlobalPerkDraw()
{
    LocalCoopGlobalPerkDraftState& draft = gLocalCoopGlobalPerkDraft;
    if (draft.window == -1 || draft.chooserIndex < 0 || draft.chooserIndex >= draft.chooserCount) {
        return;
    }

    constexpr int width = 600;
    constexpr int height = 330;
    windowFill(draft.window, 0, 0, width, height, _colorTable[0]);
    windowDrawBorder(draft.window);

    int chooserSlot = draft.chooserSlots[draft.chooserIndex];
    char title[128];
    snprintf(title, sizeof(title), "PARTY LEVEL %d - PLAYER %d GLOBAL PERK PICK",
        pcGetStat(PC_STAT_LEVEL), chooserSlot + 1);
    windowDrawText(draft.window, title, 560, 20, 18, _colorTable[992]);
    windowDrawText(draft.window,
        "PICK 1 OF 3 - EVERY PLAYER GETS THE PERK",
        560,
        20,
        44,
        _colorTable[992]);
    windowDrawText(draft.window,
        "DPAD UP/DOWN: SELECT    A: CONFIRM",
        560,
        20,
        68,
        _colorTable[992]);

    for (int index = 0; index < kLocalCoopGlobalPerkChoiceCount; index++) {
        int perk = draft.choices[index];
        const char* name = perk >= 0 ? perkGetName(perk) : nullptr;
        if (name == nullptr) {
            name = "NO VALID PERK";
        }

        char line[192];
        snprintf(line, sizeof(line), "%s %s %s",
            index == draft.selectedChoice ? ">" : " ",
            name,
            index == draft.selectedChoice ? "<" : " ");
        windowDrawText(draft.window, line, 540, 30, 108 + index * 34, _colorTable[992]);
    }

    int selectedPerk = draft.choices[draft.selectedChoice];
    const char* description = selectedPerk >= 0 ? perkGetDescription(selectedPerk) : nullptr;
    if (description == nullptr || *description == '\0') {
        description = "Shared Fallout perk. The selected rank applies to the full co-op party.";
    }
    windowDrawText(draft.window, description, 540, 30, 226, _colorTable[992]);

    char footer[128];
    snprintf(footer, sizeof(footer), "PICK %d/%d FOR THIS LEVEL",
        draft.chooserIndex + 1,
        draft.chooserCount);
    windowDrawText(draft.window, footer, 540, 30, 298, _colorTable[992]);
    windowRefresh(draft.window);
}

inline bool localCoopGlobalPerkBuildChoices(int chooserSlot)
{
    Object* actor = localCoopGlobalPerkChooserActor(chooserSlot);
    if (actor == nullptr) {
        return false;
    }

    int available[PERK_COUNT];
    int rawCount = perkGetAvailablePerks(actor, available);
    int filtered[PERK_COUNT];
    int count = 0;
    for (int index = 0; index < rawCount; index++) {
        if (localCoopGlobalPerkSelectable(available[index])) {
            filtered[count++] = available[index];
        }
    }

    if (count == 0) {
        return false;
    }

    int take = std::min(count, kLocalCoopGlobalPerkChoiceCount);
    for (int index = 0; index < take; index++) {
        int swapIndex = randomBetween(index, count - 1);
        std::swap(filtered[index], filtered[swapIndex]);
        gLocalCoopGlobalPerkDraft.choices[index] = filtered[index];
    }
    for (int index = take; index < kLocalCoopGlobalPerkChoiceCount; index++) {
        gLocalCoopGlobalPerkDraft.choices[index] = filtered[index % take];
    }
    gLocalCoopGlobalPerkDraft.selectedChoice = 0;
    return true;
}

inline void localCoopGlobalPerkBuildChooserList()
{
    LocalCoopGlobalPerkDraftState& draft = gLocalCoopGlobalPerkDraft;
    draft.chooserSlots.fill(-1);
    draft.chooserCount = 0;
    draft.chooserIndex = 0;

    for (int slot = 0; slot < kLocalCoopMaxPlayers; slot++) {
        LocalCoopPlayer& player = gLocalCoopPlayers[slot];
        if (!player.slotLocked || player.actor == nullptr) {
            continue;
        }
        draft.chooserSlots[draft.chooserCount++] = slot;
    }
}

inline void localCoopGlobalPerkApplySharedLevelGrowthOnce()
{
    // P1 receives stock Fallout HP-per-level in pcAddExperienceWithOptions.
    // P2-P4 share the level but are synthetic critters, so mirror that growth.
    for (int slot = 1; slot < kLocalCoopMaxPlayers; slot++) {
        LocalCoopPlayer& player = gLocalCoopPlayers[slot];
        Object* actor = player.actor;
        if (!player.slotLocked || actor == nullptr) {
            continue;
        }

        int maxHpBefore = critterGetStat(actor, STAT_MAXIMUM_HIT_POINTS);
        int endurance = critterGetBaseStatWithTraitModifier(actor, STAT_ENDURANCE);
        int hpPerLevel = endurance / 2 + 2;
        hpPerLevel += perkGetRank(actor, PERK_LIFEGIVER) * 4;

        int bonusHp = critterGetBonusStat(actor, STAT_MAXIMUM_HIT_POINTS);
        critterSetBonusStat(actor, STAT_MAXIMUM_HIT_POINTS, bonusHp + hpPerLevel);
        critterUpdateDerivedStats(actor);

        int maxHpAfter = critterGetStat(actor, STAT_MAXIMUM_HIT_POINTS);
        critterAdjustHitPoints(actor, maxHpAfter - maxHpBefore);
    }
}

inline bool localCoopGlobalPerkStartNextChooser()
{
    LocalCoopGlobalPerkDraftState& draft = gLocalCoopGlobalPerkDraft;

    while (draft.pendingLevelUps > 0) {
        if (draft.chooserCount == 0) {
            localCoopGlobalPerkBuildChooserList();
        }

        if (draft.chooserIndex >= draft.chooserCount) {
            draft.pendingLevelUps--;
            if (draft.pendingLevelUps <= 0) {
                localCoopGlobalPerkDestroyWindow();
                localCoopSetLevelChoiceActive(false);
                draft.chooserCount = 0;
                draft.chooserIndex = 0;
                return false;
            }
            localCoopGlobalPerkBuildChooserList();
            continue;
        }

        int chooserSlot = draft.chooserSlots[draft.chooserIndex];
        if (!localCoopGlobalPerkBuildChoices(chooserSlot)) {
            debugPrint("[COOP GLOBAL PERK] P%d has no valid perks; skipping pick\n", chooserSlot + 1);
            draft.chooserIndex++;
            continue;
        }

        if (draft.window == -1) {
            constexpr int width = 600;
            constexpr int height = 330;
            draft.window = windowCreate(
                (screenGetWidth() - width) / 2,
                (screenGetVisibleHeight() - height) / 2,
                width,
                height,
                _colorTable[0],
                WINDOW_MOVE_ON_TOP);
            if (draft.window == -1) {
                return false;
            }
        }

        draft.upWasDown = false;
        draft.downWasDown = false;
        draft.confirmWasDown = false;
        localCoopSetLevelChoiceActive(true);
        localCoopGlobalPerkDraw();
        return true;
    }

    localCoopSetLevelChoiceActive(false);
    return false;
}

inline void localCoopGlobalPerkConfirmChoice()
{
    LocalCoopGlobalPerkDraftState& draft = gLocalCoopGlobalPerkDraft;
    if (draft.chooserIndex < 0 || draft.chooserIndex >= draft.chooserCount) {
        return;
    }

    int perk = draft.choices[draft.selectedChoice];
    int chooserSlot = draft.chooserSlots[draft.chooserIndex];
    Object* chooser = localCoopGlobalPerkChooserActor(chooserSlot);
    if (perk >= 0 && chooser != nullptr && perkAdd(chooser, perk) == 0) {
        const char* name = perkGetName(perk);
        debugPrint("[COOP GLOBAL PERK] P%d picked %s; rank=%d shared by party\n",
            chooserSlot + 1,
            name != nullptr ? name : "<unnamed>",
            perkGetRank(gDude, perk));
    }

    draft.chooserIndex++;
    localCoopGlobalPerkStartNextChooser();
}

inline void localCoopGlobalPerksTick()
{
    LocalCoopGlobalPerkDraftState& draft = gLocalCoopGlobalPerkDraft;
    if (gDude == nullptr) {
        return;
    }

    int level = pcGetStat(PC_STAT_LEVEL);
    if (!draft.initialized) {
        draft.initialized = true;
        draft.observedLevel = level;
    }

    // New game/load can move the party level backwards. Treat the loaded value
    // as a fresh baseline rather than awarding phantom perk drafts.
    if (level < draft.observedLevel) {
        localCoopGlobalPerkDestroyWindow();
        localCoopSetLevelChoiceActive(false);
        draft = LocalCoopGlobalPerkDraftState {};
        draft.initialized = true;
        draft.observedLevel = level;
        return;
    }

    if (level > draft.observedLevel) {
        int levelsGained = level - draft.observedLevel;
        for (int index = 0; index < levelsGained; index++) {
            localCoopGlobalPerkApplySharedLevelGrowthOnce();
        }
        draft.observedLevel = level;
        draft.pendingLevelUps += levelsGained;
        if (!gLocalCoopLevelChoiceActive) {
            draft.chooserCount = 0;
            draft.chooserIndex = 0;
            localCoopGlobalPerkStartNextChooser();
        }
    }

    if (draft.pendingLevelUps <= 0) {
        return;
    }

    if (draft.window == -1) {
        localCoopGlobalPerkStartNextChooser();
        if (draft.window == -1) {
            return;
        }
    }

    int chooserSlot = draft.chooserSlots[draft.chooserIndex];
    int inputSlot = localCoopGlobalPerkInputSlot(chooserSlot);
    if (inputSlot < 0) {
        return;
    }

    SDL_GameController* controller = gLocalCoopPlayers[inputSlot].controller;
    bool upDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP) != 0;
    bool downDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN) != 0;
    bool confirmDown = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A) != 0;

    bool dirty = false;
    if (upDown && !draft.upWasDown) {
        draft.selectedChoice = (draft.selectedChoice + kLocalCoopGlobalPerkChoiceCount - 1)
            % kLocalCoopGlobalPerkChoiceCount;
        dirty = true;
    }
    if (downDown && !draft.downWasDown) {
        draft.selectedChoice = (draft.selectedChoice + 1) % kLocalCoopGlobalPerkChoiceCount;
        dirty = true;
    }
    if (confirmDown && !draft.confirmWasDown) {
        localCoopGlobalPerkConfirmChoice();
    } else if (dirty) {
        localCoopGlobalPerkDraw();
    }

    draft.upWasDown = upDown;
    draft.downWasDown = downDown;
    draft.confirmWasDown = confirmDown;
}

inline void localCoopGlobalPerksShutdown()
{
    localCoopGlobalPerkDestroyWindow();
    localCoopSetLevelChoiceActive(false);
    gLocalCoopGlobalPerkDraft = LocalCoopGlobalPerkDraftState {};
}

} // namespace fallout

#endif /* LOCAL_COOP_GLOBAL_PERKS_H */
