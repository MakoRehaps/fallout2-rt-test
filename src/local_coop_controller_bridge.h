#ifndef LOCAL_COOP_CONTROLLER_BRIDGE_H
#define LOCAL_COOP_CONTROLLER_BRIDGE_H

#include <SDL.h>

namespace fallout {

using LocalCoopControllerLookup = SDL_GameController* (*)(int slot);

inline LocalCoopControllerLookup gLocalCoopControllerLookup = nullptr;

inline SDL_GameController* localCoopBridgeGetController(int slot)
{
    if (gLocalCoopControllerLookup == nullptr || slot < 0 || slot >= 4) {
        return nullptr;
    }

    return gLocalCoopControllerLookup(slot);
}

} // namespace fallout

#endif /* LOCAL_COOP_CONTROLLER_BRIDGE_H */
