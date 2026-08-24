#ifndef LOCAL_COOP_BETA_HOTFIX_H
#define LOCAL_COOP_BETA_HOTFIX_H

#include <SDL.h>

#include "combat.h"
#include "local_coop.h"
#include "local_coop_analog_aim.h"
#include "local_coop_runtime.h"
#include "object.h"
#include "tile.h"

namespace fallout {

// Runtime corrections that remain useful after removing Fallout's combat phase:
// continuous steering, shared-camera dead-zone, and SDL aim rendering.
inline int gLocalCoopHotfixCameraTileBeforeTick = -1;
inline Uint32 gLocalCoopHotfixNextLegacyAdvanceTick = 0;
inline bool gLocalCoopHotfixCombatWasActive = false;

inline void localCoopBetaHotfixBeginFrame()
{
    gLocalCoopHotfixCameraTileBeforeTick = gCenterTile;

    // Queue continuous movement before the older controller poll so it does not
    // inject a one-hex stop/start step.
    localCoopAnalogAimPreRuntimeTick();
}

inline int localCoopBetaHotfixPartyCenterTile()
{
    if (gDude == nullptr || !tileIsValid(gDude->tile)) {
        return -1;
    }

    const int elevation = gDude->elevation;
    long long totalX = 0;
    long long totalY = 0;
    int count = 0;

    for (const LocalCoopPlayer& player : gLocalCoopPlayers) {
        Object* actor = player.actor;
        if (!player.humanOwned
            || actor == nullptr
            || actor->elevation != elevation
            || !tileIsValid(actor->tile)
            || (actor->data.critter.combat.results & DAM_DEAD) != 0) {
            continue;
        }

        int x = 0;
        int y = 0;
        if (tileToScreenXY(actor->tile, &x, &y, elevation) == 0) {
            totalX += x;
            totalY += y;
            count++;
        }
    }

    if (count == 0) {
        return -1;
    }

    return tileFromScreenXY(static_cast<int>(totalX / count), static_cast<int>(totalY / count), elevation, true);
}

inline void localCoopBetaHotfixCameraAfterRuntime()
{
    if (!tileIsValid(gLocalCoopHotfixCameraTileBeforeTick)) {
        return;
    }

    int partyCenter = localCoopBetaHotfixPartyCenterTile();
    if (!tileIsValid(partyCenter)) {
        return;
    }

    constexpr int kCameraDeadZoneTiles = 5;
    if (tileDistanceBetween(partyCenter, gLocalCoopHotfixCameraTileBeforeTick) <= kCameraDeadZoneTiles) {
        if (gCenterTile != gLocalCoopHotfixCameraTileBeforeTick) {
            tileSetCenter(gLocalCoopHotfixCameraTileBeforeTick,
                TILE_SET_CENTER_REFRESH_WINDOW | TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS);
        }
    }
}

inline void localCoopBetaHotfixAfterRuntime()
{
    // No combat scheduler, no SPACE/end-turn injection, and no combat-mode focus
    // branch. Danger is handled entirely by the realtime world AI runtime.
    localCoopAnalogAimPostRuntimeTick();
    localCoopBetaHotfixCameraAfterRuntime();

    // Installs the SDL post-world renderer for the current right-stick bead.
    localCoopAimBeadTick();
}

} // namespace fallout

#endif /* LOCAL_COOP_BETA_HOTFIX_H */
