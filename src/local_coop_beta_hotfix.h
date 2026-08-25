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
 // continuous steering and SDL aim rendering. Shared-camera ownership now lives
 // entirely in localCoopUpdateSharedCamera so no later pass can undo smoothing.
inline void localCoopBetaHotfixBeginFrame()
{
    // Queue continuous movement before the older controller poll so it does not
    // inject a one-hex stop/start step.
    localCoopAnalogAimPreRuntimeTick();
}

inline void localCoopBetaHotfixAfterRuntime()
{
    // No combat scheduler, no SPACE/end-turn injection, and no combat-mode focus
    // branch. Danger is handled entirely by the realtime world AI runtime.
    localCoopAnalogAimPostRuntimeTick();

    // Installs the SDL post-world renderer for the current right-stick bead.
    localCoopAimBeadTick();
}

} // namespace fallout

#endif /* LOCAL_COOP_BETA_HOTFIX_H */
