#ifndef ENDGAME_H
#define ENDGAME_H

namespace fallout {

typedef enum EndgameDeathEndingReason {
    // Dude died.
    ENDGAME_DEATH_ENDING_REASON_DEATH = 0,

    // 13 years passed.
    ENDGAME_DEATH_ENDING_REASON_TIMEOUT = 2,
} EndgameDeathEndingReason;

extern char _aEnglish_2[];

void endgamePlaySlideshow();
void endgamePlayMovie();
void unifiedFallout1EndgamePlaySlideshowImpl();
int endgameDeathEndingInit();
int endgameDeathEndingExit();
void endgameSetupDeathEnding(int reason);
char* endgameDeathEndingGetFileName();

} // namespace fallout

// interpreter_extra.cc includes interpreter_extra.h before this header, while
// endgame.cc includes endgame.h first. Redirect only script-side ending opcode
// calls through the unified profile; the stock endgame implementation compiles
// under its original symbols and remains the Fallout 2 fallback.
#if defined(INTERPRETER_EXTRA_H)
#include "unified_fallout1_endgame_profile.h"
#define endgamePlaySlideshow unifiedFallout1EndgamePlaySlideshow
#define endgamePlayMovie unifiedFallout1EndgamePlayMovie
#endif

// main.cc includes main.h before endgame.h. Fallout 1's player-death path does
// not use Fallout 2's death-ending condition table; it displays DEATH.FRM and
// randomly selects narrator nar_3 through nar_6. Keep endgame.cc untouched and
// redirect only executable-side death setup/selection calls.
#if defined(MAIN_H)
#include "unified_fallout1_death_profile.h"
#define endgameSetupDeathEnding unifiedFallout1SetupDeathEnding
#define endgameDeathEndingGetFileName unifiedFallout1DeathEndingGetFileName
#endif

#endif /* ENDGAME_H */
