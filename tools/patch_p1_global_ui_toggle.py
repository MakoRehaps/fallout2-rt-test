from pathlib import Path

path = Path('src/local_coop_runtime.h')
text = path.read_text(encoding='utf-8')
marker = '// COOP_P1_GLOBAL_UI_TOGGLE_V1'
if marker in text:
    print('P1 global UI toggle already applied')
    raise SystemExit(0)

old_fields = '''    bool pipboyWasDown = false;\n    bool inventoryWasDown = false;\n    bool startWasDown = false;'''
new_fields = '''    bool pipboyWasDown = false;\n    bool inventoryWasDown = false;\n    bool pipboyToggleArmed = true;\n    bool inventoryToggleArmed = true;\n    Uint32 pipboyReleaseStartedTick = 0;\n    Uint32 inventoryReleaseStartedTick = 0;\n    bool startWasDown = false;'''
if old_fields not in text:
    raise SystemExit('runtime slot field anchor not found')
text = text.replace(old_fields, new_fields, 1)

old_block = '''    bool modalActive = (GameMode::getCurrentGameMode() & kBlockingMenuModes) != 0;\n\n    for (int slot = 0; slot < kLocalCoopMaxPlayers; slot++) {'''
new_block = '''    int currentGameMode = GameMode::getCurrentGameMode();\n    bool modalActive = (currentGameMode & kBlockingMenuModes) != 0;\n    bool inventoryModalActive = (currentGameMode & GameMode::kInventory) != 0;\n    bool pipboyModalActive = (currentGameMode & (GameMode::kPipboy | GameMode::kAutomap)) != 0;\n    Uint32 now = SDL_GetTicks();\n\n    for (int slot = 0; slot < kLocalCoopMaxPlayers; slot++) {'''
if old_block not in text:
    raise SystemExit('modal state anchor not found')
text = text.replace(old_block, new_block, 1)

old_owner = '''        // COOP_P1_GLOBAL_UI_OWNER_V1\n        // Inventory and Pip-Boy/map are global modal screens, so only P1 may\n        // open or close them. P2-P4 keep their normal co-op equipment/map\n        // interaction once those screens are already open; this gate only owns\n        // the global modal launch/close action.\n        bool canOwnGlobalUi = canOpen && slot == 0;\n\n        if (canOwnGlobalUi && pipboyDown && !runtime.pipboyWasDown) {\n            gLocalCoopModalControllerSlot = 0;\n            enqueueInputEvent(KEY_LOWERCASE_P);\n            modalActive = true;\n            debugPrint("[PHOBOI INPUT] slot=0 source=controller button=dpad-left global-ui=pipboy\\n");\n        } else if (canOwnGlobalUi && inventoryDown && !runtime.inventoryWasDown) {\n            gLocalCoopModalControllerSlot = 0;\n            enqueueInputEvent(KEY_LOWERCASE_I);\n            modalActive = true;\n            debugPrint("[COOP INVENTORY] slot=0 source=controller button=back global-ui=inventory\\n");\n        } else if (canOpen && skilldexDown && !runtime.skilldexWasDown) {'''
new_owner = '''        // COOP_P1_GLOBAL_UI_OWNER_V1\n        // COOP_P1_GLOBAL_UI_TOGGLE_V1\n        // Inventory and Pip-Boy/map are global modal screens, so only P1 owns\n        // their open/close toggle. A phone packet can momentarily cross neutral,\n        // so a simple rising-edge test is not enough: after every toggle require\n        // a real release held for 140 ms before the button is armed again. This\n        // prevents one physical tap from closing and immediately reopening.\n        if (slot == 0) {\n            if (!pipboyDown) {\n                if (runtime.pipboyReleaseStartedTick == 0) runtime.pipboyReleaseStartedTick = now;\n                if (!runtime.pipboyToggleArmed\n                    && static_cast<Sint32>(now - runtime.pipboyReleaseStartedTick) >= 140) {\n                    runtime.pipboyToggleArmed = true;\n                }\n            } else {\n                runtime.pipboyReleaseStartedTick = 0;\n            }\n\n            if (!inventoryDown) {\n                if (runtime.inventoryReleaseStartedTick == 0) runtime.inventoryReleaseStartedTick = now;\n                if (!runtime.inventoryToggleArmed\n                    && static_cast<Sint32>(now - runtime.inventoryReleaseStartedTick) >= 140) {\n                    runtime.inventoryToggleArmed = true;\n                }\n            } else {\n                runtime.inventoryReleaseStartedTick = 0;\n            }\n        }\n\n        bool canOwnGlobalUi = canOpen && slot == 0;\n        bool p1PipboyToggle = slot == 0 && pipboyDown && runtime.pipboyToggleArmed;\n        bool p1InventoryToggle = slot == 0 && inventoryDown && runtime.inventoryToggleArmed;\n\n        if (p1PipboyToggle && pipboyModalActive) {\n            runtime.pipboyToggleArmed = false;\n            gLocalCoopModalControllerSlot = 0;\n            enqueueInputEvent(KEY_ESCAPE);\n            debugPrint("[PHOBOI INPUT] slot=0 global-ui=pipboy action=close\\n");\n        } else if (p1InventoryToggle && inventoryModalActive) {\n            runtime.inventoryToggleArmed = false;\n            gLocalCoopModalControllerSlot = 0;\n            enqueueInputEvent(KEY_ESCAPE);\n            debugPrint("[COOP INVENTORY] slot=0 global-ui=inventory action=close\\n");\n        } else if (canOwnGlobalUi && p1PipboyToggle) {\n            runtime.pipboyToggleArmed = false;\n            gLocalCoopModalControllerSlot = 0;\n            enqueueInputEvent(KEY_LOWERCASE_P);\n            modalActive = true;\n            debugPrint("[PHOBOI INPUT] slot=0 global-ui=pipboy action=open\\n");\n        } else if (canOwnGlobalUi && p1InventoryToggle) {\n            runtime.inventoryToggleArmed = false;\n            gLocalCoopModalControllerSlot = 0;\n            enqueueInputEvent(KEY_LOWERCASE_I);\n            modalActive = true;\n            debugPrint("[COOP INVENTORY] slot=0 global-ui=inventory action=open\\n");\n        } else if (canOpen && skilldexDown && !runtime.skilldexWasDown) {'''
if old_owner not in text:
    raise SystemExit('P1 global UI owner block anchor not found')
text = text.replace(old_owner, new_owner, 1)

path.write_text(text, encoding='utf-8')
print('Applied P1 single-press global UI toggle')
