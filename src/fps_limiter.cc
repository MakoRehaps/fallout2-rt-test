#include "fps_limiter.h"

#include <SDL.h>

#include "fo3_runtime_layout.h"

namespace fallout {

FpsLimiter::FpsLimiter(unsigned int fps)
    : _fps(fps)
    , _ticks(0)
{
}

void FpsLimiter::mark()
{
    // Post-map-load heartbeat for generated Fallout 3 layouts. The loader is
    // edge-triggered on gMapHeader.name, so normal frames are effectively free.
    fo3RuntimeLayoutTick();
    _ticks = SDL_GetTicks();
}

void FpsLimiter::throttle() const
{
    if (1000 / _fps > SDL_GetTicks() - _ticks) {
        SDL_Delay(1000 / _fps - (SDL_GetTicks() - _ticks));
    }
}

} // namespace fallout
