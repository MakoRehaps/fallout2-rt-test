#ifndef MAIN_H
#define MAIN_H

#include "local_coop_inventory_ui.h"
#include "local_coop_runtime.h"
#include "unified_campaign.h"

namespace fallout {

// main.cc includes this header before input.h. Keep a declaration of the
// original input function here, then redirect only main.cc's calls through the
// cooperative frame wrapper below. input.cc itself is untouched and continues
// to define the original inputGetInput symbol.
int inputGetInput();

inline int localCoopMainInputGetInput()
{
    localCoopRuntimeTick();
    localCoopInventoryUiEnsureTicker();
    localCoopInventoryUiTick();
    return inputGetInput();
}

int falloutMain(int argc, char** argv);

} // namespace fallout

// main.cc includes input.h after main.h. Redirect the executable's main-loop
// reads without altering the low-level input implementation used elsewhere.
#define inputGetInput localCoopMainInputGetInput

#endif /* MAIN_H */
