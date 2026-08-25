#ifndef PIPBOY_H
#define PIPBOY_H

#include "db.h"

namespace fallout {

typedef enum PipboyOpenIntent {
    PIPBOY_OPEN_INTENT_UNSPECIFIED = 0,
    PIPBOY_OPEN_INTENT_REST = 1,
    PIPBOY_OPEN_INTENT_WORLD_MAP = 2,
} PipboyOpenIntent;

// The combined co-op game exposes its own handset/menu identity. The legacy
// name remains as a compatibility entry point for engine and script callers.
int phoboiOpen(int intent);
bool phoboiWildernessMapActive();
int phoboiGetActiveTab();
int pipboyOpen(int intent);
void pipboyInit();
void pipboyReset();
int pipboySave(File* stream);
int pipboyLoad(File* stream);

} // namespace fallout

#endif /* PIPBOY_H */
