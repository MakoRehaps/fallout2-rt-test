#ifndef LOCAL_COOP_BETA_HOTFIX_H
#define LOCAL_COOP_BETA_HOTFIX_H

#include <SDL.h>

#include "combat.h"
#include "input.h"
#include "local_coop.h"
#include "local_coop_ai_realtime.h"
#include "local_coop_focus.h"
#include "local_coop_runtime.h"
#include "object.h"
#include "tile.h"

namespace fallout {

// Beta runtime corrections kept separate from the original controller slice so
// they can be validated independently and later folded into the main runtime.
inline int gLocalCoopHotfixCameraTileBeforeTick = -1;
inline Uint32 gLocalCoopHotfixNextLegacyAdvanceTick = 0;
inline bool gLocalCoopHotfixCombatWasActive = false;

inline void localCoopBetaHotfixBeginFrame()
{
    gLocalCoopHotfixCameraTileBeforeTick = gCenterTile;
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

    // The old runtime recenters every movement step, which makes analog movement
    // feel as though the camera is physically attached to a critter. Keep the
    // previous camera center until the party centroid leaves this dead-zone.
    constexpr int kCameraDeadZoneTiles = 5;
    if (tileDistanceBetween(partyCenter, gLocalCoopHotfixCameraTileBeforeTick) <= kCameraDeadZoneTiles) {
        if (gCenterTile != gLocalCoopHotfixCameraTileBeforeTick) {
            tileSetCenter(gLocalCoopHotfixCameraTileBeforeTick,
                TILE_SET_CENTER_REFRESH_WINDOW | TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS);
        }
    }
}

inline void localCoopBetaHotfixSeedHostileAi()
{
    if (!isInCombat() || gDude == nullptr) {
        return;
    }

    Object** critters = nullptr;
    int count = objectListCreate(-1, gDude->elevation, OBJ_TYPE_CRITTER, &critters);
    if (count <= 0 || critters == nullptr) {
        return;
    }

    for (int index = 0; index < count; index++) {
        Object* actor = critters[index];
        if (actor == nullptr
            || actor == gDude
            || localCoopActorIsHumanOwned(actor)
            || (actor->flags & OBJECT_HIDDEN) != 0
            || (actor->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0
            || actor->data.critter.combat.team == gDude->data.critter.combat.team) {
            continue;
        }

        if (actor->id != -1 && gLocalCoopRealtimeAiActors.find(actor->id) == gLocalCoopRealtimeAiActors.end()) {
            localCoopRealtimeAiRegisterLegacyTurn(actor, gDude);
        }
    }

    objectListFree(critters);
}

inline void localCoopBetaHotfixAdvanceLegacyScheduler()
{
    if (!isInCombat()) {
        gLocalCoopHotfixNextLegacyAdvanceTick = 0;
        return;
    }

    // The previous bridge queued SPACE once and then waited forever for the
    // legacy turn owner to change. If that input was consumed elsewhere, enemy
    // AI was never registered. Retry at a bounded cadence until ownership moves.
    if (_combat_whose_turn() != gDude) {
        return;
    }

    Uint32 now = SDL_GetTicks();
    if (gLocalCoopHotfixNextLegacyAdvanceTick == 0
        || static_cast<Sint32>(now - gLocalCoopHotfixNextLegacyAdvanceTick) >= 0) {
        enqueueInputEvent(KEY_SPACE);
        gLocalCoopHotfixNextLegacyAdvanceTick = now + 250;
        gLocalCoopLegacyYieldQueued = false;
    }
}

inline void localCoopBetaHotfixCombatInput()
{
    if (!isInCombat()) {
        gLocalCoopHotfixCombatWasActive = false;
        return;
    }

    bool combatJustStarted = !gLocalCoopHotfixCombatWasActive;
    gLocalCoopHotfixCombatWasActive = true;

    if (combatJustStarted || gLocalCoopRealtimeAiActors.empty()) {
        localCoopBetaHotfixSeedHostileAi();
    }

    // Keep focus selection updated after the runtime has read the right stick.
    // A centered stick retains the previous valid target; moving it selects a
    // new enemy in the requested direction.
    for (LocalCoopPlayer& player : gLocalCoopPlayers) {
        if (!player.connected
            || !player.humanOwned
            || player.controller == nullptr
            || player.actor == nullptr
            || player.uiMode != LocalCoopUiMode::World) {
            continue;
        }

        Object* target = localCoopFocusFindEnemy(player);
        if (target != nullptr) {
            gLocalCoopRuntimeSlots[player.slot].aimTarget = target;
            localCoopFocusApplyOutline(player.slot, target, true);
        }
    }

    localCoopBetaHotfixAdvanceLegacyScheduler();
    localCoopRealtimeAiTick();
}

inline void localCoopBetaHotfixAfterRuntime()
{
    localCoopBetaHotfixCameraAfterRuntime();
    localCoopBetaHotfixCombatInput();
}

} // namespace fallout

#endif /* LOCAL_COOP_BETA_HOTFIX_H */
