#ifndef LOCAL_COOP_DANGER_H
#define LOCAL_COOP_DANGER_H

#include <SDL.h>

namespace fallout {

// Realtime co-op never enters Fallout's turn-based combat mode. This state is a
// short post-damage cooldown only. Living enemies, enemy movement, misses and
// zero-damage hits must never keep the party flagged as being in combat.
inline constexpr Uint32 kLocalCoopDangerCooldownMs = 1000;

inline bool gLocalCoopDangerActive = false;
inline Uint32 gLocalCoopDangerLastPlayerDamageTick = 0;
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

inline void localCoopDangerRecordPlayerDamage(int damage)
{
    if (damage <= 0) {
        return;
    }

    gLocalCoopDangerActive = true;
    gLocalCoopDangerLastPlayerDamageTick = SDL_GetTicks();
}

inline void localCoopDangerEnd()
{
    gLocalCoopDangerActive = false;
    gLocalCoopDangerLastPlayerDamageTick = 0;
    gLocalCoopDangerLiveHostiles = 0;
}

inline void localCoopDangerSetLiveHostiles(int count)
{
    // Retained for diagnostics/AI state only. Hostile count does not control the
    // player's post-damage combat cooldown.
    gLocalCoopDangerLiveHostiles = count > 0 ? count : 0;
}

inline bool localCoopDangerBlocksMapExit()
{
    if (!gLocalCoopDangerActive) {
        return false;
    }

    Uint32 now = SDL_GetTicks();
    if (gLocalCoopDangerLastPlayerDamageTick == 0
        || static_cast<Sint32>(now - gLocalCoopDangerLastPlayerDamageTick)
            >= static_cast<Sint32>(kLocalCoopDangerCooldownMs)) {
        localCoopDangerEnd();
        return false;
    }

    return true;
}

} // namespace fallout

#endif /* LOCAL_COOP_DANGER_H */
