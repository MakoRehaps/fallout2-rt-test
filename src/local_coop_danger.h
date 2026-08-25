#ifndef LOCAL_COOP_DANGER_H
#define LOCAL_COOP_DANGER_H

#include <SDL.h>

namespace fallout {

// Realtime co-op never enters Fallout's turn-based combat mode. Danger is only
// an encounter lock for leaving the map; movement, attacks and interaction remain
// ordinary live-world actions.
inline bool gLocalCoopDangerActive = false;
inline Uint32 gLocalCoopDangerStartedTick = 0;
inline Uint32 gLocalCoopDangerLastHostileTick = 0;
inline int gLocalCoopDangerLiveHostiles = 0;
inline int gLocalCoopPendingMapExitTile = -1;

inline void localCoopMarkMapExitTile(int tile)
{
    gLocalCoopPendingMapExitTile = tile;
}

inline int localCoopConsumeMapExitTile()
{
    int tile = gLocalCoopPendingMapExitTile;
    gLocalCoopPendingMapExitTile = -1;
    return tile;
}


inline void localCoopDangerBegin()
{
    Uint32 now = SDL_GetTicks();
    if (!gLocalCoopDangerActive) {
        gLocalCoopDangerStartedTick = now;
    }
    gLocalCoopDangerActive = true;
    gLocalCoopDangerLastHostileTick = now;
}

inline void localCoopDangerTouch()
{
    if (gLocalCoopDangerActive) {
        gLocalCoopDangerLastHostileTick = SDL_GetTicks();
    }
}

inline void localCoopDangerEnd()
{
    gLocalCoopDangerActive = false;
    gLocalCoopDangerStartedTick = 0;
    gLocalCoopDangerLastHostileTick = 0;
    gLocalCoopDangerLiveHostiles = 0;
}

inline void localCoopDangerSetLiveHostiles(int count)
{
    gLocalCoopDangerLiveHostiles = count > 0 ? count : 0;
    if (gLocalCoopDangerLiveHostiles > 0) {
        if (!gLocalCoopDangerActive) {
            localCoopDangerBegin();
        } else {
            localCoopDangerTouch();
        }
    }
}

inline bool localCoopDangerBlocksMapExit()
{
    if (!gLocalCoopDangerActive) {
        return false;
    }

    if (gLocalCoopDangerLiveHostiles > 0) {
        return true;
    }

    // A script can request combat without identifying a usable hostile. Do not
    // let that orphan request permanently trap the party on a map. Give a brief
    // grace period for the AI registrar to resolve an attacker, then unlock.
    Uint32 now = SDL_GetTicks();
    if (gLocalCoopDangerLastHostileTick == 0
        || static_cast<Sint32>(now - gLocalCoopDangerLastHostileTick) >= 1000) {
        localCoopDangerEnd();
        return false;
    }

    return true;
}

} // namespace fallout

#endif /* LOCAL_COOP_DANGER_H */
