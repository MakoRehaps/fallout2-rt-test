from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def patch_group_room():
    path = ROOT / "src" / "local_coop_group_room.h"
    text = path.read_text(encoding="utf-8")
    if '#include "local_coop_mobile.h"' not in text:
        text = text.replace('#include "local_coop.h"\n', '#include "local_coop.h"\n#include "local_coop_mobile.h"\n', 1)
    old = "        inputGetInput();\n        localCoopRefreshControllers();"
    new = "        inputGetInput();\n        // COOP_READY_ROOM_MOBILE_TICK_V1\n        // Keep phone claims materialized as SDL virtual controllers while the\n        // pre-game room is open; otherwise a phone can claim a slot on the web\n        // page without the ready room ever seeing the controller.\n        localCoopMobileTick();\n        localCoopRefreshControllers();"
    if "COOP_READY_ROOM_MOBILE_TICK_V1" not in text:
        if old not in text:
            raise SystemExit("group-room input anchor not found")
        text = text.replace(old, new, 1)
    path.write_text(text, encoding="utf-8")


def patch_runtime():
    path = ROOT / "src" / "local_coop_runtime.h"
    text = path.read_text(encoding="utf-8")
    if '#include "local_coop_mobile.h"' not in text:
        text = text.replace('#include "local_coop_fps.h"\n', '#include "local_coop_fps.h"\n#include "local_coop_mobile.h"\n', 1)
    old = "    localCoopRealtimeAiInstall();\n    localCoopRuntimeEnsureTicker();\n    localCoopPollControllers();"
    new = "    localCoopRealtimeAiInstall();\n    localCoopRuntimeEnsureTicker();\n    // COOP_RUNTIME_MOBILE_BEFORE_SPAWN_V1\n    // Materialize claimed phone slots before controller polling and before the\n    // ready-room handoff creates P2-P4 actors. This removes the one-frame/race\n    // dependency between the web controller server and live-map spawning.\n    localCoopMobileTick();\n    localCoopPollControllers();"
    if "COOP_RUNTIME_MOBILE_BEFORE_SPAWN_V1" not in text:
        if old not in text:
            raise SystemExit("runtime controller anchor not found")
        text = text.replace(old, new, 1)
    path.write_text(text, encoding="utf-8")


def patch_spawn():
    path = ROOT / "src" / "local_coop.h"
    text = path.read_text(encoding="utf-8")
    old = '''        if (!gLocalCoopPrejoinedSlots[slot]\n            || !player.connected\n            || player.controller == nullptr\n            || player.actor != nullptr) {\n            continue;\n        }\n\n        debugPrint("[COOP PREJOIN] slot=%d spawning on live map\\n", slot);'''
    new = '''        // COOP_PREJOIN_AUTHORITATIVE_SPAWN_V2\n        // The ready vote is the authority for whether this slot exists. Do not\n        // require the controller pointer to still be attached on the exact first\n        // live-map frame; phone/USB devices can reconnect a frame later. Create\n        // the reserved actor now and let normal connection logic unhide/control it.\n        if (!gLocalCoopPrejoinedSlots[slot] || player.actor != nullptr) {\n            continue;\n        }\n\n        debugPrint("[COOP PREJOIN] slot=%d spawning on live map connected=%d controller=%p locked=%d\\n",\n            slot, player.connected ? 1 : 0, static_cast<void*>(player.controller), player.slotLocked ? 1 : 0);'''
    if "COOP_PREJOIN_AUTHORITATIVE_SPAWN_V2" not in text:
        if old not in text:
            raise SystemExit("prejoin spawn anchor not found")
        text = text.replace(old, new, 1)
    path.write_text(text, encoding="utf-8")


patch_group_room()
patch_runtime()
patch_spawn()
print("patched regular co-op ready-room -> live-map spawn handoff")
