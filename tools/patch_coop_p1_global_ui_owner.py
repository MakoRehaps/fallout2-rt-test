from pathlib import Path

p = Path('src/local_coop_runtime.h')
s = p.read_text(encoding='utf-8')
marker = '// COOP_P1_GLOBAL_UI_OWNER_V1'
if marker in s:
    print('P1 global UI ownership already applied')
    raise SystemExit(0)

old = '''        bool canOpen = !modalActive
            && player.connected
            && player.humanOwned
            && player.controller != nullptr
            && player.uiMode == LocalCoopUiMode::World
            && !localCoopDangerBlocksMapExit();

        if (canOpen && pipboyDown && !runtime.pipboyWasDown) {
            gLocalCoopModalControllerSlot = slot;
            enqueueInputEvent(KEY_LOWERCASE_P);
            modalActive = true;
            debugPrint("[PHOBOI INPUT] slot=%d source=controller button=dpad-left\\n", slot);
        } else if (canOpen && inventoryDown && !runtime.inventoryWasDown) {
            gLocalCoopModalControllerSlot = slot;
            enqueueInputEvent(KEY_LOWERCASE_I);
            modalActive = true;
            debugPrint("[COOP INVENTORY] slot=%d source=controller button=back\\n", slot);
        } else if (canOpen && skilldexDown && !runtime.skilldexWasDown) {'''
new = '''        bool canOpen = !modalActive
            && player.connected
            && player.humanOwned
            && player.controller != nullptr
            && player.uiMode == LocalCoopUiMode::World
            && !localCoopDangerBlocksMapExit();
        // COOP_P1_GLOBAL_UI_OWNER_V1
        // Inventory and Pip-Boy/map are global modal screens, so only P1 may
        // open or close them. P2-P4 keep their normal co-op equipment/map
        // interaction once those screens are already open; this gate only owns
        // the global modal launch/close action.
        bool canOwnGlobalUi = canOpen && slot == 0;

        if (canOwnGlobalUi && pipboyDown && !runtime.pipboyWasDown) {
            gLocalCoopModalControllerSlot = 0;
            enqueueInputEvent(KEY_LOWERCASE_P);
            modalActive = true;
            debugPrint("[PHOBOI INPUT] slot=0 source=controller button=dpad-left global-ui=pipboy\\n");
        } else if (canOwnGlobalUi && inventoryDown && !runtime.inventoryWasDown) {
            gLocalCoopModalControllerSlot = 0;
            enqueueInputEvent(KEY_LOWERCASE_I);
            modalActive = true;
            debugPrint("[COOP INVENTORY] slot=0 source=controller button=back global-ui=inventory\\n");
        } else if (canOpen && skilldexDown && !runtime.skilldexWasDown) {'''
if old not in s:
    raise SystemExit('modal input block not found')
s = s.replace(old, new, 1)
p.write_text(s, encoding='utf-8')
print('Applied P1-only global Inventory/Pip-Boy ownership')
