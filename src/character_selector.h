#ifndef CHARACTER_SELECTOR_H
#define CHARACTER_SELECTOR_H

namespace fallout {

int characterSelectorOpen();

void premadeCharactersInit();
void premadeCharactersExit();

} // namespace fallout

// main.cc includes main.h before this header, while character_selector.cc
// includes this header directly. Unified campaign transitions restore a captured
// player into the freshly bootstrapped destination runtime and skip unrelated
// premade-character selection. Ordinary new games retain the stock selector.
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
