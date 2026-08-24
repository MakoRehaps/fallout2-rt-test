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
// selector window and enter the character editor directly until the co-op lobby
// replaces the stock premade selector.
#if defined(MAIN_H)
#include "character_editor.h"
#include "color.h"
#include "input.h"
#include "kb.h"
#include "local_coop.h"
#include "local_coop_generic_ui_controller.h"
#include "mouse.h"
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

        // The stock main-menu loop hides the hardware cursor immediately after
        // the user chooses New Game. Fallout 2's premade selector normally shows
        // it again. The fused F1 path bypasses that selector, so explicitly give
        // the creation editor a visible/live mouse and restore the previous state
        // when it closes.
        bool cursorWasHidden = cursorIsHidden();
        if (cursorWasHidden) {
            mouseShowCursor();
        }

        // Drop the New Game button/key event that opened this modal. Otherwise a
        // stale release/repeat can become the first character-editor command.
        inputEventQueueReset();
        keyboardReset();

        // Character creation used to pass 0 to ScopedGameMode inside the stock
        // editor, which meant our controller UI bridge never recognized it as an
        // editor screen. Keep an outer editor mode active for the whole creation
        // session, and make sure the controller ticker exists before entering the
        // editor. Keyboard and mouse remain stock inputs; this only adds controller
        // navigation on top of them.
        localCoopInit();
        localCoopGenericUiControllerEnsureTicker();
        ScopedGameMode editorMode(GameMode::kEditor);

        // The fused game uses the Fallout 2 engine/UI layer. Keep the player's
        // F1 world/proto origin intact, but resolve the entire character-editor
        // modal (palette, editor.msg and interface art) from the F2 dataset.
        // Leaving the scope immediately restores F1-preferred world resources
        // before V13Ent.map is loaded.
        UnifiedResourceOriginScope editorResources(UnifiedGameId::Fallout2);

        // mainMenuWindowHide(true) fades the hardware palette to black before
        // calling the selector. The stock Fallout 2 selector restores color.pal
        // before opening its UI, but this unified Fallout 1 path intentionally
        // bypasses that selector and enters the editor directly.
        colorPaletteLoad("color.pal");
        paletteFadeTo(_cmap);

        int editorRc = characterEditorShow(true);

        if (cursorWasHidden) {
            mouseHideCursor();
        }

        return editorRc == 0 ? 2 : 3;
    }

    return characterSelectorOpen();
}

} // namespace fallout

#define characterSelectorOpen unifiedCampaignCharacterSelectorOpen
#endif

#endif /* CHARACTER_SELECTOR_H */
