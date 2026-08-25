#ifndef LOCAL_COOP_AUTOSAVE_H
#define LOCAL_COOP_AUTOSAVE_H

#include <SDL.h>

#include "animation.h"
#include "combat.h"
#include "display_monitor.h"
#include "game.h"
#include "game_dialog.h"
#include "loadsave.h"
#include "local_coop_danger.h"
#include "map.h"
#include "object.h"
#include "stat.h"

namespace fallout {

inline constexpr Uint32 kLocalCoopAutosaveIntervalMs = 15 * 60 * 1000;
inline constexpr int kLocalCoopAutosaveMapTransfers = 3;

inline Uint32 gLocalCoopAutosaveActiveMs = 0;
inline Uint32 gLocalCoopAutosaveLastTick = 0;
inline int gLocalCoopAutosaveLastMap = -1;
inline int gLocalCoopAutosaveLastLevel = -1;
inline int gLocalCoopAutosaveMapTransferCount = 0;
inline bool gLocalCoopAutosavePending = false;
inline bool gLocalCoopAutosaveInProgress = false;

inline void localCoopAutosaveReset()
{
    gLocalCoopAutosaveActiveMs = 0;
    gLocalCoopAutosaveLastTick = 0;
    gLocalCoopAutosaveLastMap = -1;
    gLocalCoopAutosaveLastLevel = -1;
    gLocalCoopAutosaveMapTransferCount = 0;
    gLocalCoopAutosavePending = false;
    gLocalCoopAutosaveInProgress = false;
}

inline bool localCoopAutosaveWorldIsSafe()
{
    return gDude != nullptr
        && gDude->tile >= 0
        && (gDude->data.critter.combat.results
               & (DAM_DEAD | DAM_KNOCKED_OUT))
            == 0
        && !animationIsBusy(gDude)
        && !_isLoadingGame()
        && !isInCombat()
        && !_gdialogActive()
        && gameUiIsDisabled() == 0
        && !gLocalCoopDangerActive
        && gLocalCoopDangerLiveHostiles == 0;
}

inline void localCoopAutosaveTick()
{
    Uint32 now = SDL_GetTicks();
    if (gLocalCoopAutosaveLastTick == 0) {
        gLocalCoopAutosaveLastTick = now;
    }

    Uint32 elapsed = now - gLocalCoopAutosaveLastTick;
    gLocalCoopAutosaveLastTick = now;

    if (gDude == nullptr || _isLoadingGame()) {
        return;
    }

    int currentMap = gMapHeader.field_34;
    if (gLocalCoopAutosaveLastMap == -1) {
        gLocalCoopAutosaveLastMap = currentMap;
    } else if (currentMap >= 0 && currentMap != gLocalCoopAutosaveLastMap) {
        gLocalCoopAutosaveLastMap = currentMap;
        gLocalCoopAutosaveMapTransferCount++;
        if (gLocalCoopAutosaveMapTransferCount >= kLocalCoopAutosaveMapTransfers) {
            gLocalCoopAutosavePending = true;
        }
    }

    int currentLevel = pcGetStat(PC_STAT_LEVEL);
    if (gLocalCoopAutosaveLastLevel == -1) {
        gLocalCoopAutosaveLastLevel = currentLevel;
    } else if (currentLevel > gLocalCoopAutosaveLastLevel) {
        gLocalCoopAutosaveLastLevel = currentLevel;
        gLocalCoopAutosavePending = true;
    } else if (currentLevel < gLocalCoopAutosaveLastLevel) {
        gLocalCoopAutosaveLastLevel = currentLevel;
    }

    bool safe = localCoopAutosaveWorldIsSafe();
    if (safe) {
        if (elapsed <= 1000) {
            gLocalCoopAutosaveActiveMs += elapsed;
        }
        if (gLocalCoopAutosaveActiveMs >= kLocalCoopAutosaveIntervalMs) {
            gLocalCoopAutosavePending = true;
        }
    }

    if (!gLocalCoopAutosavePending || !safe || gLocalCoopAutosaveInProgress) {
        return;
    }

    gLocalCoopAutosaveInProgress = true;
    displayMonitorAddMessage("Autosaving...");
    int rc = lsgAutosaveGame();
    gLocalCoopAutosaveInProgress = false;

    if (rc == 1) {
        displayMonitorAddMessage("Autosave complete.");
        gLocalCoopAutosavePending = false;
        gLocalCoopAutosaveActiveMs = 0;
        gLocalCoopAutosaveMapTransferCount = 0;
        gLocalCoopAutosaveLastTick = SDL_GetTicks();
    } else {
        displayMonitorAddMessage("Autosave failed; will retry when safe.");
    }
}

} // namespace fallout

#endif /* LOCAL_COOP_AUTOSAVE_H */
