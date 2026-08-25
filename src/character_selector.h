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
#include "svga.h"
#include "window_manager.h"
#include "unified_campaign_carryover.h"
#include "unified_campaign_transition.h"
#include "unified_resource_origin.h"

namespace fallout {

inline int localCoopPlayerOneCharacterSelector()
{
    _ResetPlayer();
    localCoopInit();
    localCoopRefreshActorBindings();

    LocalCoopPlayer& player = gLocalCoopPlayers[0];
    player.archetype = std::max(0, std::min(player.archetype, kLocalCoopArchetypeCount - 1));
    player.gender = player.gender == GENDER_FEMALE ? GENDER_FEMALE : GENDER_MALE;
    player.joinWindow = windowCreate(
        (screenGetWidth() - 420) / 2,
        (screenGetVisibleHeight() - 230) / 2,
        420,
        230,
        _colorTable[0],
        WINDOW_MOVE_ON_TOP);
    if (player.joinWindow == -1) {
        return 0;
    }

    bool cursorWasHidden = cursorIsHidden();
    mouseShowCursor();
    player.controllerInputActive = false;

    inputEventQueueReset();
    keyboardReset();
    colorPaletteLoad("color.pal");
    paletteFadeTo(_cmap);
    localCoopDrawJoinMenu(player);

    bool leftWasDown = false;
    bool rightWasDown = false;
    bool confirmWasDown = false;
    bool cancelWasDown = false;
    bool genderWasDown = false;
    int result = 0;
    bool done = false;

    while (!done) {
        int keyCode = inputGetInput();
        SDL_GameControllerUpdate();

        SDL_GameController* controller = player.connected ? player.controller : nullptr;
        bool leftDown = controller != nullptr
            && SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT) != 0;
        bool rightDown = controller != nullptr
            && SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) != 0;
        bool confirmDown = controller != nullptr
            && SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A) != 0;
        bool cancelDown = controller != nullptr
            && SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_B) != 0;
        bool genderDown = controller != nullptr
            && SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_Y) != 0;

        bool controllerEvent = (leftDown && !leftWasDown)
            || (rightDown && !rightWasDown)
            || (confirmDown && !confirmWasDown)
            || (cancelDown && !cancelWasDown)
            || (genderDown && !genderWasDown);
        bool keyboardEvent = keyCode != -1 && keyCode != -2;

        if (controllerEvent && !player.controllerInputActive) {
            player.controllerInputActive = true;
            mouseHideCursor();
        } else if (keyboardEvent && player.controllerInputActive) {
            player.controllerInputActive = false;
            mouseShowCursor();
        }

        bool dirty = false;
        if (keyCode == KEY_ARROW_LEFT || (leftDown && !leftWasDown)) {
            player.archetype = (player.archetype + kLocalCoopArchetypeCount - 1)
                % kLocalCoopArchetypeCount;
            dirty = true;
        } else if (keyCode == KEY_ARROW_RIGHT || (rightDown && !rightWasDown)) {
            player.archetype = (player.archetype + 1) % kLocalCoopArchetypeCount;
            dirty = true;
        }

        if (keyCode == KEY_UPPERCASE_Y
            || keyCode == KEY_LOWERCASE_Y
            || (genderDown && !genderWasDown)) {
            player.gender = player.gender == GENDER_MALE ? GENDER_FEMALE : GENDER_MALE;
            dirty = true;
        }

        if (keyCode == KEY_RETURN || (confirmDown && !confirmWasDown)) {
            if (localCoopApplyPlayerOneArchetype(player.archetype, player.gender)) {
                result = 2;
                done = true;
            }
        } else if (keyCode == KEY_ESCAPE || (cancelDown && !cancelWasDown)) {
            result = 3;
            done = true;
        } else if (dirty) {
            localCoopDrawJoinMenu(player);
        }

        leftWasDown = leftDown;
        rightWasDown = rightDown;
        confirmWasDown = confirmDown;
        cancelWasDown = cancelDown;
        genderWasDown = genderDown;

        renderPresent();
        SDL_Delay(8);
    }

    localCoopCloseJoinMenu(player);
    if (player.controllerInputActive || cursorWasHidden) {
        mouseHideCursor();
    } else {
        mouseShowCursor();
    }
    return result;
}

inline int unifiedCampaignCharacterSelectorOpen()
{
    if (unifiedCampaignCarryoverCanApply()) {
        bool applied = unifiedCampaignApplyPlayerCarryover();
        if (applied) {
            unifiedCampaignConsumePostgameResume();
        }
        return applied ? 2 : 0;
    }

    // Every new main character now uses the same compact archetype/gender
    // builder as players 2-4, independent of the active campaign.
    UnifiedResourceOriginScope editorResources(UnifiedGameId::Fallout2);
    return localCoopPlayerOneCharacterSelector();
}

} // namespace fallout

#define characterSelectorOpen unifiedCampaignCharacterSelectorOpen
#endif

#endif /* CHARACTER_SELECTOR_H */
