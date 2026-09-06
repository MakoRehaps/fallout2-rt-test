from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(path: Path, old: str, new: str, marker: str) -> None:
    text = path.read_text(encoding="utf-8")
    if marker in text:
        return
    if old not in text:
        raise RuntimeError(f"anchor not found in {path}: {old[:120]!r}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


def replace_all(path: Path, old: str, new: str, marker: str) -> None:
    text = path.read_text(encoding="utf-8")
    if marker in text:
        return
    if old not in text:
        raise RuntimeError(f"anchor not found in {path}: {old[:120]!r}")
    path.write_text(text.replace(old, new), encoding="utf-8")


perk_h = ROOT / "src/perk.h"
perk_cc = ROOT / "src/perk.cc"
local_coop_h = ROOT / "src/local_coop.h"
runtime_h = ROOT / "src/local_coop_runtime.h"
character_editor_cc = ROOT / "src/character_editor.cc"
global_perks_h = ROOT / "src/local_coop_global_perks.h"

GLOBAL_HEADER = r'''#ifndef LOCAL_COOP_GLOBAL_PERKS_H
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
'''

global_perks_h.write_text(GLOBAL_HEADER, encoding="utf-8")

replace_once(
    perk_h,
    "void perkRemoveEffect(Object* critter, int perk);\nint perkGetSkillModifier(Object* critter, int skill);",
    "void perkRemoveEffect(Object* critter, int perk);\n// COOP_GLOBAL_PERKS_V1\nvoid perkApplyGlobalCoopEffectsToActor(Object* critter);\nint perkGetSkillModifier(Object* critter, int skill);",
    "COOP_GLOBAL_PERKS_V1",
)

replace_once(
    perk_cc,
    '#include "memory.h"\n#include "object.h"',
    '#include "memory.h"\n#include "local_coop.h"\n#include "object.h"',
    '#include "local_coop.h"',
)

replace_once(
    perk_cc,
    "    if (critter == gDude) {\n        return gPartyMemberPerkRanks;\n    }",
    "    // COOP_GLOBAL_PERKS_V1: P1 is the single saved perk-rank table for all\n"
    "    // synthetic co-op players. Stock companions keep their own tables.\n"
    "    if (critter == gDude\n"
    "        || (critter != nullptr && protoIsLocalCoopPlayerPid(critter->pid))) {\n"
    "        return gPartyMemberPerkRanks;\n"
    "    }",
    "P1 is the single saved perk-rank table",
)

replace_once(
    perk_cc,
    "    if (critter == gDude) {\n        if (pcGetStat(PC_STAT_LEVEL) < perkDescription->minLevel) {\n            return false;\n        }\n    }",
    "    // COOP_GLOBAL_PERKS_LEVEL_GATE_V1: every co-op actor uses P1's party\n"
    "    // level for perk requirements, while keeping its own SPECIAL/skills.\n"
    "    if (critter == gDude\n"
    "        || (critter != nullptr && protoIsLocalCoopPlayerPid(critter->pid))) {\n"
    "        if (pcGetStat(PC_STAT_LEVEL) < perkDescription->minLevel) {\n"
    "            return false;\n"
    "        }\n"
    "    }",
    "COOP_GLOBAL_PERKS_LEVEL_GATE_V1",
)

GLOBAL_EFFECTS = r'''// COOP_GLOBAL_PERKS_EFFECTS_V1
static bool perkIsGlobalCoopActor(const Object* critter)
{
    return critter != nullptr
        && (critter == gDude || protoIsLocalCoopPlayerPid(critter->pid));
}

static void perkApplyGlobalCoopDirectStatEffect(Object* critter, int perk, int direction)
{
    if (critter == nullptr || !perkIsValid(perk) || direction == 0) {
        return;
    }

    PerkDescription* perkDescription = &(gPerkDescriptions[perk]);
    if (perkDescription->stat != -1) {
        int value = critterGetBonusStat(critter, perkDescription->stat);
        critterSetBonusStat(
            critter,
            perkDescription->stat,
            value + direction * perkDescription->statModifier);
    }
}

static void perkApplyGlobalCoopDirectStatEffectToParty(int perk, int direction)
{
    for (int slot = 1; slot < kLocalCoopMaxPlayers; slot++) {
        LocalCoopPlayer& player = gLocalCoopPlayers[slot];
        if (!player.slotLocked || player.actor == nullptr) {
            continue;
        }
        perkApplyGlobalCoopDirectStatEffect(player.actor, perk, direction);
        critterUpdateDerivedStats(player.actor);
    }
}

void perkApplyGlobalCoopEffectsToActor(Object* critter)
{
    if (critter == nullptr || critter == gDude || !protoIsLocalCoopPlayerPid(critter->pid)) {
        return;
    }

    // Synthetic player critters are rebuilt on map/load handoffs. Reconstruct
    // the direct stat side of every shared perk from P1's persistent rank table.
    for (int perk = 0; perk < PERK_COUNT; perk++) {
        int rank = gPartyMemberPerkRanks[0].ranks[perk];
        for (int index = 0; index < rank; index++) {
            perkApplyGlobalCoopDirectStatEffect(critter, perk, 1);
        }
    }

    // P2-P4 do not own PC_STAT_LEVEL, but they still share P1's party level.
    // Rebuild their HP growth deterministically whenever their synthetic actor
    // is recreated so save/load and map transitions do not erase progression.
    int partyLevel = std::max(1, pcGetStat(PC_STAT_LEVEL));
    if (partyLevel > 1) {
        int endurance = critterGetBaseStatWithTraitModifier(critter, STAT_ENDURANCE);
        int hpPerLevel = endurance / 2 + 2;
        hpPerLevel += perkGetRank(gDude, PERK_LIFEGIVER) * 4;
        int bonusHp = critterGetBonusStat(critter, STAT_MAXIMUM_HIT_POINTS);
        critterSetBonusStat(
            critter,
            STAT_MAXIMUM_HIT_POINTS,
            bonusHp + (partyLevel - 1) * hpPerLevel);
    }

    critterUpdateDerivedStats(critter);
}

'''

replace_once(
    perk_cc,
    "// 0x496A5C\nint perkAdd(Object* critter, int perk)",
    GLOBAL_EFFECTS + "// 0x496A5C\nint perkAdd(Object* critter, int perk)",
    "COOP_GLOBAL_PERKS_EFFECTS_V1",
)

replace_once(
    perk_cc,
    "    ranksData->ranks[perk] += 1;\n\n    perkAddEffect(critter, perk);\n\n    return 0;\n}\n\n// perk_add_force",
    "    ranksData->ranks[perk] += 1;\n\n"
    "    if (perkIsGlobalCoopActor(critter)) {\n"
    "        // Rank ownership is global. Apply global/PC side effects once to P1,\n"
    "        // then mirror critter-local stat effects to P2-P4.\n"
    "        perkAddEffect(gDude, perk);\n"
    "        perkApplyGlobalCoopDirectStatEffectToParty(perk, 1);\n"
    "    } else {\n"
    "        perkAddEffect(critter, perk);\n"
    "    }\n\n"
    "    return 0;\n}\n\n// perk_add_force",
    "Rank ownership is global. Apply global/PC side effects once to P1",
)

replace_once(
    perk_cc,
    "    ranksData->ranks[perk] += 1;\n\n    perkAddEffect(critter, perk);\n\n    return 0;\n}\n\n// perk_sub",
    "    ranksData->ranks[perk] += 1;\n\n"
    "    if (perkIsGlobalCoopActor(critter)) {\n"
    "        perkAddEffect(gDude, perk);\n"
    "        perkApplyGlobalCoopDirectStatEffectToParty(perk, 1);\n"
    "    } else {\n"
    "        perkAddEffect(critter, perk);\n"
    "    }\n\n"
    "    return 0;\n}\n\n// perk_sub",
    "perkApplyGlobalCoopDirectStatEffectToParty(perk, 1);\n    } else {\n        perkAddEffect(critter, perk);\n    }\n\n    return 0;\n}\n\n// perk_sub",
)

replace_once(
    perk_cc,
    "    ranksData->ranks[perk] -= 1;\n\n    perkRemoveEffect(critter, perk);\n\n    return 0;",
    "    ranksData->ranks[perk] -= 1;\n\n"
    "    if (perkIsGlobalCoopActor(critter)) {\n"
    "        perkRemoveEffect(gDude, perk);\n"
    "        perkApplyGlobalCoopDirectStatEffectToParty(perk, -1);\n"
    "    } else {\n"
    "        perkRemoveEffect(critter, perk);\n"
    "    }\n\n"
    "    return 0;",
    "perkApplyGlobalCoopDirectStatEffectToParty(perk, -1)",
)

replace_once(
    local_coop_h,
    '#include "party_member.h"\n#include "platform_compat.h"',
    '#include "party_member.h"\n#include "perk.h"\n#include "platform_compat.h"',
    '#include "perk.h"',
)

replace_once(
    local_coop_h,
    "    actor->flags &= ~OBJECT_HIDDEN;\n    actor->data.critter.combat.results = 0;\n    critterUpdateDerivedStats(actor);",
    "    actor->flags &= ~OBJECT_HIDDEN;\n    actor->data.critter.combat.results = 0;\n"
    "    // COOP_GLOBAL_PERKS_ACTOR_RESTORE_V1\n"
    "    perkApplyGlobalCoopEffectsToActor(actor);\n"
    "    critterUpdateDerivedStats(actor);",
    "COOP_GLOBAL_PERKS_ACTOR_RESTORE_V1",
)

replace_once(
    runtime_h,
    '#include "local_coop_focus.h"\n#include "local_coop_fps.h"',
    '#include "local_coop_focus.h"\n#include "local_coop_global_perks.h"\n#include "local_coop_fps.h"',
    '#include "local_coop_global_perks.h"',
)

replace_once(
    runtime_h,
    "        localCoopPersonalUiShutdown();\n        localCoopDestroyHud();",
    "        localCoopPersonalUiShutdown();\n        localCoopGlobalPerksShutdown();\n        localCoopDestroyHud();",
    "localCoopGlobalPerksShutdown();",
)

replace_once(
    runtime_h,
    "    localCoopSpawnPrejoinedPlayers();\n    localCoopKeepReservedActorsWithParty();\n\n    // COOP_EXPLICIT_SIMULATION_PAUSE_RUNTIME_V1",
    "    localCoopSpawnPrejoinedPlayers();\n    localCoopKeepReservedActorsWithParty();\n"
    "    // COOP_GLOBAL_PERKS_RUNTIME_V1\n"
    "    localCoopGlobalPerksTick();\n\n"
    "    // COOP_EXPLICIT_SIMULATION_PAUSE_RUNTIME_V1",
    "COOP_GLOBAL_PERKS_RUNTIME_V1",
)

replace_all(
    character_editor_cc,
    "                    gCharacterEditorHasFreePerk = 1;",
    "                    // COOP_GLOBAL_PERKS_STOCK_PICKER_DISABLED_V1\n"
    "                    // The co-op draft owns perk awards; do not also grant\n"
    "                    // the stock full-list perk picker.\n"
    "                    gCharacterEditorHasFreePerk = 0;",
    "COOP_GLOBAL_PERKS_STOCK_PICKER_DISABLED_V1",
)

print("Global co-op perk draft materialized.")
