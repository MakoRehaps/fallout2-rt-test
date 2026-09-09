#ifndef FALLOUT_MAINMENU_H_
#define FALLOUT_MAINMENU_H_

namespace fallout {

typedef enum MainMenuOption {
    MAIN_MENU_INTRO,
    MAIN_MENU_NEW_GAME,
    MAIN_MENU_LOAD_GAME,
    MAIN_MENU_SCREENSAVER,
    MAIN_MENU_TIMEOUT,
    MAIN_MENU_CREDITS,
    MAIN_MENU_QUOTES,
    MAIN_MENU_EXIT,
    MAIN_MENU_SELFRUN,
    MAIN_MENU_OPTIONS,
    MAIN_MENU_RESUME_CAMPAIGN,
} MainMenuOption;

int mainMenuWindowInit();
void mainMenuWindowFree();
void mainMenuWindowHide(bool animate);
void mainMenuWindowUnhide(bool animate);
int _main_menu_is_enabled();
int mainMenuWindowHandleEvents();

} // namespace fallout

// main.cc includes main.h before this header, while mainmenu.cc includes this
// header directly. Route only the executable's menu event read through the
// unified one-shot Act II starter; the stock menu implementation keeps its
// original symbol and behavior everywhere else.
#if defined(MAIN_H)
#include "unified_campaign_transition.h"

namespace fallout {

inline int unifiedCampaignMainMenuWindowHandleEvents()
{
    if (gUnifiedCampaignPostgameResumePending
        && unifiedCampaignBothGamesCompleted()) {
        return MAIN_MENU_RESUME_CAMPAIGN;
    }

    if (unifiedCampaignConsumeAutoStartNewGame()) {
        return MAIN_MENU_NEW_GAME;
    }

    return mainMenuWindowHandleEvents();
}

} // namespace fallout

#define mainMenuWindowHandleEvents unifiedCampaignMainMenuWindowHandleEvents
#endif

#endif /* FALLOUT_MAINMENU_H_ */
