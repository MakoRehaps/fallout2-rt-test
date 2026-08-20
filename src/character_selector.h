#ifndef CHARACTER_SELECTOR_H
#define CHARACTER_SELECTOR_H

namespace fallout {

int characterSelectorOpen();

void premadeCharactersInit();
void premadeCharactersExit();

} // namespace fallout

#if defined(MAIN_H)
#include "unified_campaign_carryover.h"
#include "unified_campaign_transition.h"

namespace fallout {

inline int unifiedCampaignCharacterSelectorOpen()
{
    if (unifiedCampaignCarryoverCanApply()) {
        return unifiedCampaignApplyPlayerCarryover() ? 2 : 0;
    }

    if (unifiedCampaignConsumePostgameResume()) {
        return 2;
    }

    return characterSelectorOpen();
}

} // namespace fallout

#define characterSelectorOpen unifiedCampaignCharacterSelectorOpen
#endif

#endif /* CHARACTER_SELECTOR_H */
