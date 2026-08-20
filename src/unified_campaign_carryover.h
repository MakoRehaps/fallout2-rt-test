#ifndef UNIFIED_CAMPAIGN_CARRYOVER_H
#define UNIFIED_CAMPAIGN_CARRYOVER_H

#include <array>
#include <cstring>

#include "inventory.h"
#include "item.h"
#include "object.h"
#include "platform_compat.h"
#include "proto.h"
#include "skill.h"
#include "stat.h"
#include "trait.h"
#include "unified_campaign.h"

namespace fallout {

inline constexpr int kUnifiedCampaignCarryoverItemCapacity = 256;
inline constexpr int kUnifiedCampaignCarryoverItemNameLength = 80;

struct UnifiedCampaignCarryoverItem {
    bool valid = false;
    char name[kUnifiedCampaignCarryoverItemNameLength] {};
    int quantity = 0;
    int itemType = -1;
    int equippedFlags = 0;
    int ammoQuantity = -1;
    int miscCharges = -1;
};

struct UnifiedCampaignCarryover {
    bool pending = false;
    UnifiedGameId sourceGame = UnifiedGameId::Fallout1;
    UnifiedGameId destinationGame = UnifiedGameId::Fallout2;
    std::array<int, STAT_COUNT> baseStats {};
    std::array<int, STAT_COUNT> bonusStats {};
    std::array<int, PC_STAT_COUNT> pcStats {};
    std::array<int, SKILL_COUNT> baseSkills {};
    std::array<int, 4> taggedSkills { -1, -1, -1, -1 };
    int trait1 = -1;
    int trait2 = -1;
    std::array<UnifiedCampaignCarryoverItem, kUnifiedCampaignCarryoverItemCapacity> items {};
    int itemCount = 0;
};

inline UnifiedCampaignCarryover gUnifiedCampaignCarryover;

inline void unifiedCampaignClearCarryover()
{
    gUnifiedCampaignCarryover = UnifiedCampaignCarryover {};
}

inline void unifiedCampaignCaptureInventory(UnifiedCampaignCarryover& carryover)
{
    if (gDude == nullptr) {
        return;
    }

    Inventory& inventory = gDude->data.inventory;
    for (int index = 0;
         index < inventory.length && carryover.itemCount < kUnifiedCampaignCarryoverItemCapacity;
         index++) {
        InventoryItem& inventoryItem = inventory.items[index];
        Object* item = inventoryItem.item;
        if (item == nullptr || inventoryItem.quantity <= 0 || PID_TYPE(item->pid) != OBJ_TYPE_ITEM) {
            continue;
        }

        const char* name = itemGetName(item);
        if (name == nullptr || *name == '\0') {
            continue;
        }

        UnifiedCampaignCarryoverItem& captured = carryover.items[carryover.itemCount++];
        captured.valid = true;
        std::strncpy(captured.name, name, sizeof(captured.name) - 1);
        captured.name[sizeof(captured.name) - 1] = '\0';
        captured.quantity = inventoryItem.quantity;
        captured.itemType = itemGetType(item);
        captured.equippedFlags = item->flags & OBJECT_EQUIPPED;

        if (captured.itemType == ITEM_TYPE_WEAPON || captured.itemType == ITEM_TYPE_AMMO) {
            captured.ammoQuantity = ammoGetQuantity(item);
        }

        if (captured.itemType == ITEM_TYPE_MISC) {
            captured.miscCharges = miscItemGetCharges(item);
        }
    }
}

inline bool unifiedCampaignCapturePlayerCarryover(UnifiedGameId destinationGame)
{
    if (gDude == nullptr) {
        return false;
    }

    UnifiedCampaignCarryover carryover;
    carryover.pending = true;
    carryover.sourceGame = unifiedCampaignGetActiveGame();
    carryover.destinationGame = destinationGame;

    for (int stat = 0; stat < STAT_COUNT; stat++) {
        carryover.baseStats[stat] = critterGetBaseStat(gDude, stat);
        carryover.bonusStats[stat] = critterGetBonusStat(gDude, stat);
    }

    for (int pcStat = 0; pcStat < PC_STAT_COUNT; pcStat++) {
        carryover.pcStats[pcStat] = pcGetStat(pcStat);
    }

    for (int skill = 0; skill < SKILL_COUNT; skill++) {
        carryover.baseSkills[skill] = skillGetBaseValue(gDude, skill);
    }

    skillsGetTagged(carryover.taggedSkills.data(), static_cast<int>(carryover.taggedSkills.size()));
    traitsGetSelected(&carryover.trait1, &carryover.trait2);
    unifiedCampaignCaptureInventory(carryover);

    gUnifiedCampaignCarryover = carryover;
    return true;
}

inline bool unifiedCampaignCarryoverCanApply()
{
    return gUnifiedCampaignCarryover.pending
        && unifiedCampaignGetActiveGame() == gUnifiedCampaignCarryover.destinationGame;
}

inline void unifiedCampaignRestoreSkillBase(int skill, int target)
{
    int current = skillGetBaseValue(gDude, skill);
    int guard = 0;

    while (current < target && guard++ < 1000) {
        int previous = current;
        if (skillAddForce(gDude, skill) == -1) {
            break;
        }
        current = skillGetBaseValue(gDude, skill);
        if (current <= previous) {
            break;
        }
    }

    guard = 0;
    while (current > target && guard++ < 1000) {
        int previous = current;
        if (skillSubForce(gDude, skill) == -1) {
            break;
        }
        current = skillGetBaseValue(gDude, skill);
        if (current >= previous) {
            break;
        }
    }
}

inline int unifiedCampaignFindDestinationItemPid(const UnifiedCampaignCarryoverItem& captured)
{
    int maxItemId = proto_max_id(OBJ_TYPE_ITEM);
    for (int id = 0; id <= maxItemId; id++) {
        int pid = (OBJ_TYPE_ITEM << 24) | id;
        Proto* proto = nullptr;
        if (protoGetProto(pid, &proto) != 0 || proto == nullptr) {
            continue;
        }

        const char* destinationName = protoGetName(pid);
        if (destinationName == nullptr || compat_stricmp(destinationName, captured.name) != 0) {
            continue;
        }

        Object* probe = nullptr;
        if (objectCreateWithPid(&probe, pid) != 0 || probe == nullptr) {
            continue;
        }

        int destinationType = itemGetType(probe);
        objectDestroy(probe, nullptr);
        if (destinationType == captured.itemType) {
            return pid;
        }
    }

    return -1;
}

inline Object* unifiedCampaignRestoreOneInventoryItem(const UnifiedCampaignCarryoverItem& captured)
{
    int pid = unifiedCampaignFindDestinationItemPid(captured);
    if (pid == -1) {
        return nullptr;
    }

    Object* item = nullptr;
    if (objectCreateWithPid(&item, pid) != 0 || item == nullptr) {
        return nullptr;
    }

    if (captured.ammoQuantity >= 0
        && (captured.itemType == ITEM_TYPE_WEAPON || captured.itemType == ITEM_TYPE_AMMO)) {
        ammoSetQuantity(item, captured.ammoQuantity);
    }

    if (captured.miscCharges >= 0 && captured.itemType == ITEM_TYPE_MISC) {
        miscItemSetCharges(item, captured.miscCharges);
    }

    if (itemAdd(gDude, item, captured.quantity) != 0) {
        objectDestroy(item, nullptr);
        return nullptr;
    }

    return item;
}

inline void unifiedCampaignRestoreInventory(const UnifiedCampaignCarryover& carryover)
{
    Object* leftHand = nullptr;
    Object* rightHand = nullptr;
    Object* armor = nullptr;

    for (int index = 0; index < carryover.itemCount; index++) {
        const UnifiedCampaignCarryoverItem& captured = carryover.items[index];
        if (!captured.valid) {
            continue;
        }

        Object* restored = unifiedCampaignRestoreOneInventoryItem(captured);
        if (restored == nullptr) {
            continue;
        }

        if ((captured.equippedFlags & OBJECT_WORN) != 0) {
            armor = restored;
        }
        if ((captured.equippedFlags & OBJECT_IN_LEFT_HAND) != 0) {
            leftHand = restored;
        }
        if ((captured.equippedFlags & OBJECT_IN_RIGHT_HAND) != 0) {
            rightHand = restored;
        }
    }

    // Use the stock equip path after every translated object is safely resident
    // in the Fallout 2 inventory. This refreshes armor AC/FID and hand state
    // instead of copying Fallout 1 object flags directly across runtimes.
    if (armor != nullptr) {
        _invenWieldFunc(gDude, armor, 0, false);
    }
    if (leftHand != nullptr) {
        _invenWieldFunc(gDude, leftHand, 0, false);
    }
    if (rightHand != nullptr) {
        _invenWieldFunc(gDude, rightHand, 1, false);
    }
}

inline bool unifiedCampaignApplyPlayerCarryover()
{
    if (!unifiedCampaignCarryoverCanApply() || gDude == nullptr) {
        return false;
    }

    UnifiedCampaignCarryover carryover = gUnifiedCampaignCarryover;

    // Restore traits before raw base values so trait-derived display modifiers
    // do not get baked into the carried SPECIAL/skill values a second time.
    traitsSetSelected(carryover.trait1, carryover.trait2);
    skillsSetTagged(carryover.taggedSkills.data(), static_cast<int>(carryover.taggedSkills.size()));

    for (int stat = 0; stat < STAT_COUNT; stat++) {
        critterSetBaseStat(gDude, stat, carryover.baseStats[stat]);
        critterSetBonusStat(gDude, stat, carryover.bonusStats[stat]);
    }
    critterUpdateDerivedStats(gDude);

    for (int skill = 0; skill < SKILL_COUNT; skill++) {
        unifiedCampaignRestoreSkillBase(skill, carryover.baseSkills[skill]);
    }

    for (int pcStat = 0; pcStat < PC_STAT_COUNT; pcStat++) {
        pcSetStat(pcStat, carryover.pcStats[pcStat]);
    }

    unifiedCampaignRestoreInventory(carryover);
    unifiedCampaignClearCarryover();
    return true;
}

} // namespace fallout

#endif /* UNIFIED_CAMPAIGN_CARRYOVER_H */
