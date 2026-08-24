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
// premade-character selection. Fallout 1 new games currently bypass the F2
// selector window and enter the character editor directly until the original F1
// selector presentation is fully ported.
#if defined(MAIN_H)
#include "character_editor.h"
#include "proto.h"
#include "unified_campaign_carryover.h"
#include "unified_campaign_transition.h"
#include "unified_resource_origin.h"

namespace fallout {

inline int unifiedCampaignCharacterSelectorOpen()
{
    if (unifiedCampaignCarryoverCanApply()) {
        bool applied = unifiedCampaignApplyPlayerCarryover();
        if (applied) {
            unifiedCampaignConsumePostgameResume();
        }
        return applied ? 2 : 0;
    }

    if (unifiedCampaignGetActiveGame() == UnifiedGameId::Fallout1) {
        _ResetPlayer();

        // The fused game uses the Fallout 2 engine/UI layer. Keep the player's
        // F1 world/proto origin intact, but resolve the character editor's
        // editor.msg and interface art from the F2 dataset for the duration of
        // this modal screen. Leaving the scope immediately restores F1-preferred
        // world resources before V13Ent.map is loaded.
        UnifiedResourceOriginScope editorResources(UnifiedGameId::Fallout2);
        return characterEditorShow(true) == 0 ? 2 : 3;
    }

    return characterSelectorOpen();
}

} // namespace fallout

#define characterSelectorOpen unifiedCampaignCharacterSelectorOpen
#endif

#endif /* CHARACTER_SELECTOR_H */
