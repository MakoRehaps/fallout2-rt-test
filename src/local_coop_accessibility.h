#ifndef LOCAL_COOP_ACCESSIBILITY_H
#define LOCAL_COOP_ACCESSIBILITY_H

#include <SDL.h>

#include <algorithm>
#include <vector>

#include "critter.h"
#include "local_coop.h"
#include "object.h"
#include "obj_types.h"
#include "tile.h"

namespace fallout {

// COOP_ACCESSIBILITY_HIGHLIGHTS_V1
// Accessibility-only visual aid. This does not replace or recolor Fallout art;
// it uses the engine's native object outline renderer around nearby useful
// objects. Objects that already have an outline are left alone so normal game
// feedback remains authoritative.
inline bool gLocalCoopAccessibilityHighlightsEnabled = false;
inline Uint32 gLocalCoopAccessibilityNextRefreshTick = 0;

struct LocalCoopAccessibilityOutline {
    int objectId = -1;
};

inline std::vector<LocalCoopAccessibilityOutline> gLocalCoopAccessibilityOutlinedObjects;

inline bool localCoopAccessibilityIsHumanActor(Object* object)
{
    if (object == nullptr) {
        return false;
    }

    for (const LocalCoopPlayer& player : gLocalCoopPlayers) {
        if (player.slotLocked && player.actor == object) {
            return true;
        }
    }
    return false;
}

inline int localCoopAccessibilityDistanceFromParty(Object* object)
{
    if (object == nullptr || !tileIsValid(object->tile)) {
        return 9999;
    }

    int best = 9999;
    for (const LocalCoopPlayer& player : gLocalCoopPlayers) {
        if (!player.slotLocked || player.actor == nullptr || !tileIsValid(player.actor->tile)) {
            continue;
        }
        best = std::min(best, tileDistanceBetween(player.actor->tile, object->tile));
    }
    return best;
}

inline bool localCoopAccessibilityIsUsefulScenery(Object* object)
{
    if (object == nullptr || PID_TYPE(object->pid) != OBJ_TYPE_SCENERY) {
        return false;
    }

    return _obj_action_can_use(object)
        || _obj_portal_is_walk_thru(object)
        || object->sid != -1;
}

inline int localCoopAccessibilityOutlineType(Object* object)
{
    if (object == nullptr) {
        return 0;
    }

    int type = PID_TYPE(object->pid);
    if (type == OBJ_TYPE_ITEM) {
        return OUTLINE_TYPE_ITEM;
    }

    if (type == OBJ_TYPE_MISC && isExitGridAt(object->tile, object->elevation)) {
        return OUTLINE_TYPE_4;
    }

    if (type == OBJ_TYPE_SCENERY && localCoopAccessibilityIsUsefulScenery(object)) {
        return OUTLINE_TYPE_2;
    }

    if (type == OBJ_TYPE_CRITTER) {
        if (localCoopAccessibilityIsHumanActor(object) && object->data.critter.hp <= 0) {
            return OUTLINE_TYPE_4;
        }

        if (object == gDude) {
            return 0;
        }

        int playerTeam = gDude != nullptr ? gDude->data.critter.combat.team : 0;
        int objectTeam = object->data.critter.combat.team;
        if (objectTeam != playerTeam) {
            return OUTLINE_TYPE_HOSTILE;
        }
        return OUTLINE_TYPE_FRIENDLY;
    }

    return 0;
}

inline void localCoopAccessibilityClearAppliedOutlines()
{
    for (const LocalCoopAccessibilityOutline& entry : gLocalCoopAccessibilityOutlinedObjects) {
        Object* object = objectFindById(entry.objectId);
        if (object != nullptr && objectPointerIsLive(object)) {
            objectClearOutline(object, nullptr);
        }
    }
    gLocalCoopAccessibilityOutlinedObjects.clear();
}

inline void localCoopAccessibilityRefresh()
{
    localCoopAccessibilityClearAppliedOutlines();

    if (!gLocalCoopAccessibilityHighlightsEnabled || gDude == nullptr) {
        return;
    }

    Object* object = objectFindFirstAtElevation(gDude->elevation);
    while (object != nullptr) {
        if (object != gDude
            && (object->flags & OBJECT_HIDDEN) == 0
            && (object->flags & OBJECT_NO_HIGHLIGHT) == 0
            && tileIsValid(object->tile)
            && localCoopAccessibilityDistanceFromParty(object) <= 24
            && object->outline == 0) {
            int outlineType = localCoopAccessibilityOutlineType(object);
            if (outlineType != 0 && objectSetOutline(object, outlineType, nullptr) == 0) {
                gLocalCoopAccessibilityOutlinedObjects.push_back({ object->id });
            }
        }
        object = objectFindNextAtElevation();
    }
}

inline void localCoopAccessibilitySetEnabled(bool enabled)
{
    if (gLocalCoopAccessibilityHighlightsEnabled == enabled) {
        return;
    }

    gLocalCoopAccessibilityHighlightsEnabled = enabled;
    gLocalCoopAccessibilityNextRefreshTick = 0;
    localCoopAccessibilityRefresh();
}

inline void localCoopAccessibilityToggle()
{
    localCoopAccessibilitySetEnabled(!gLocalCoopAccessibilityHighlightsEnabled);
}

inline const char* localCoopAccessibilityStatusLabel()
{
    return gLocalCoopAccessibilityHighlightsEnabled ? "ON" : "OFF";
}

inline void localCoopAccessibilityTick()
{
    if (!gLocalCoopAccessibilityHighlightsEnabled) {
        return;
    }

    Uint32 now = SDL_GetTicks();
    if (static_cast<Sint32>(now - gLocalCoopAccessibilityNextRefreshTick) < 0) {
        return;
    }

    gLocalCoopAccessibilityNextRefreshTick = now + 250;
    localCoopAccessibilityRefresh();
}

} // namespace fallout

#endif
