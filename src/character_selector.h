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
#include "color.h"
#include "palette.h"
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
        // F1 world/proto origin intact, but resolve the entire character-editor
        // modal (palette, editor.msg and interface art) from the F2 dataset.
        // Leaving the scope immediately restores F1-preferred world resources
        // before V13Ent.map is loaded.
        UnifiedResourceOriginScope editorResources(UnifiedGameId::Fallout2);

        // mainMenuWindowHide(true) fades the hardware palette to black before
        // calling the selector. The stock Fallout 2 selector restores color.pal
        // before opening its UI, but this unified Fallout 1 path intentionally
        // bypasses that selector and enters the editor directly. Restore the
        // editor palette here or the editor is fully functional but renders as
        // a black screen (Esc still works, which is the telltale symptom).
        colorPaletteLoad("color.pal");
        paletteFadeTo(_cmap);

        return characterEditorShow(true) == 0 ? 2 : 3;
    }

    return characterSelectorOpen();
}

} // namespace fallout

#define characterSelectorOpen unifiedCampaignCharacterSelectorOpen
#endif

#endif /* CHARACTER_SELECTOR_H */
