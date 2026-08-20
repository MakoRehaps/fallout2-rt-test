#ifndef CHARACTER_SELECTOR_H
#define CHARACTER_SELECTOR_H

namespace fallout {

int characterSelectorOpen();

void premadeCharactersInit();
void premadeCharactersExit();

} // namespace fallout

// main.cc includes main.h before this header, while character_selector.cc
// includes this header directly. During the one-shot unified F1 -> F2 handoff,
// skip the unrelated F2 premade picker and restore the captured F1 player into
// the freshly bootstrapped F2 runtime. All ordinary F2 new games keep the stock
// character selector unchanged.
#if defined(MAIN_H)
#include "unified_campaign_carryover.h"

namespace fallout {

inline int unifiedCampaignCharacterSelectorOpen()
{
    if (unifiedCampaignCarryoverCanApply()) {
        return unifiedCampaignApplyPlayerCarryover() ? 2 : 0;
    }

    return characterSelectorOpen();
}

} // namespace fallout

#define characterSelectorOpen unifiedCampaignCharacterSelectorOpen
#endif

#endif /* CHARACTER_SELECTOR_H */
