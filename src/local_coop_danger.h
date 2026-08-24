#ifndef LOCAL_COOP_DANGER_H
#define LOCAL_COOP_DANGER_H

#include <SDL.h>

namespace fallout {

// Realtime co-op never enters Fallout's turn-based combat mode. This flag is
// only an encounter/danger state: movement, interaction and UI remain normal,
// while map exits stay locked until the active hostile encounter is cleared.
inline bool gLocalCoopDangerActive = false;
inline Uint32 gLocalCoopDangerStartedTick = 0;
inline Uint32 gLocalCoopDangerLastHostileTick = 0;

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
}

inline bool localCoopDangerBlocksMapExit()
{
    return gLocalCoopDangerActive;
}

} // namespace fallout

#endif /* LOCAL_COOP_DANGER_H */
