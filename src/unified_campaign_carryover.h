#ifndef UNIFIED_CAMPAIGN_CARRYOVER_H
#define UNIFIED_CAMPAIGN_CARRYOVER_H

#include <array>

#include "object.h"
#include "skill.h"
#include "stat.h"
#include "trait.h"
#include "unified_campaign.h"

namespace fallout {

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
};

inline UnifiedCampaignCarryover gUnifiedCampaignCarryover;

inline void unifiedCampaignClearCarryover()
{
    gUnifiedCampaignCarryover = UnifiedCampaignCarryover {};
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

    unifiedCampaignClearCarryover();
    return true;
}

} // namespace fallout

#endif /* UNIFIED_CAMPAIGN_CARRYOVER_H */
