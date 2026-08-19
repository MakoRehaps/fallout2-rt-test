#include "game_dialog.h"

// game_dialog.h reaches inventory.h through interpreter/object and installs the
// dialogue-side redirect macro. The implementation below needs to name the
// untouched stock function when the controller bridge falls back, so compile
// the barter implementation with that token mapped explicitly to the stock
// symbol instead.
#ifdef inventoryOpenTrade
#undef inventoryOpenTrade
#endif
#define inventoryOpenTrade inventoryOpenTradeStock
#include "local_coop_barter_ui.h"
#undef inventoryOpenTrade

namespace fallout {

void localCoopInventoryOpenTradeBridge(int win,
    Object* barterer,
    Object* playerTable,
    Object* bartererTable,
    int barterMod)
{
    localCoopInventoryOpenTrade(win,
        barterer,
        playerTable,
        bartererTable,
        barterMod);
}

} // namespace fallout
