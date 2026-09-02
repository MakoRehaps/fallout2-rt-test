from pathlib import Path

# Controller-owned co-op runs inside a Fallout background ticker as well as the
# main frame loop. Blocking stock screens (PipBoy, Skilldex, Character) must not
# be entered from inside that ticker, otherwise the re-entrancy guard prevents
# the ticker from servicing controller input while the modal is open. Queue one
# proven stock game key and consume it in mainLoop after localCoopRuntimeTick()
# has completely returned.

# ---------------------------------------------------------------------------
# Shared deferred request state.
# ---------------------------------------------------------------------------
path = Path("src/local_coop.h")
text = path.read_text(encoding="utf-8")
if "COOP_DEFERRED_GLOBAL_UI_REQUEST_V1" not in text:
    anchor = 'inline bool gLocalCoopSystemMenuActive = false;\n'
    if anchor not in text:
        raise SystemExit("local_coop deferred UI anchor missing")
    addition = '''\n// COOP_DEFERRED_GLOBAL_UI_REQUEST_V1\n// Stock global screens are blocking loops. Request them during the co-op tick,\n// but dispatch them only after that tick has unwound back into mainLoop.\ninline int gLocalCoopDeferredGameKey = -1;\ninline int gLocalCoopDeferredGameSlot = -1;\n\ninline void localCoopRequestDeferredGameKey(int keyCode, int slot)\n{\n    if (gLocalCoopDeferredGameKey == -1) {\n        gLocalCoopDeferredGameKey = keyCode;\n        gLocalCoopDeferredGameSlot = slot;\n    }\n}\n\ninline int localCoopTakeDeferredGameKey(int* slot)\n{\n    int keyCode = gLocalCoopDeferredGameKey;\n    if (slot != nullptr) {\n        *slot = gLocalCoopDeferredGameSlot;\n    }\n    gLocalCoopDeferredGameKey = -1;\n    gLocalCoopDeferredGameSlot = -1;\n    return keyCode;\n}\n'''
    text = text.replace(anchor, anchor + addition, 1)
path.write_text(text, encoding="utf-8")

# ---------------------------------------------------------------------------
# D-pad-left: replace the regression patch's direct blocking phoboiOpen call.
# ---------------------------------------------------------------------------
path = Path("src/local_coop_runtime.h")
text = path.read_text(encoding="utf-8")
if "COOP_DEFERRED_PIPBOY_REQUEST_V1" not in text:
    old = '''        // COOP_P1_DIRECT_PIPBOY_HOTKEY_V1\n        // Only slot 0 owns Pip-Boy.  Open the real backend directly instead of\n        // queueing P, which is intentionally discarded by controller-only P1.\n        if (canOwnGlobalUi && p1PipboyEdge) {\n            gLocalCoopModalControllerSlot = 0;\n            phoboiOpen(PIPBOY_OPEN_INTENT_WORLD_MAP);\n            modalActive = true;\n            debugPrint("[PHOBOI INPUT] slot=0 global-ui=pipboy action=open-direct\\n");\n        } else if (canOpen && skilldexDown && !runtime.skilldexWasDown) {'''
    new = '''        // COOP_P1_DIRECT_PIPBOY_HOTKEY_V1\n        // COOP_DEFERRED_PIPBOY_REQUEST_V1\n        // Do not enter phoboiOpen while localCoopRuntimeTick owns its re-entry\n        // guard. Let mainLoop dispatch the proven stock P route after this tick\n        // returns, so the ticker can continue servicing the modal controller.\n        if (canOwnGlobalUi && p1PipboyEdge) {\n            gLocalCoopModalControllerSlot = 0;\n            localCoopRequestDeferredGameKey(KEY_LOWERCASE_P, 0);\n            modalActive = true;\n            debugPrint("[PHOBOI INPUT] slot=0 global-ui=pipboy action=deferred-stock-route\\n");\n        } else if (canOpen && skilldexDown && !runtime.skilldexWasDown) {'''
    if old not in text:
        raise SystemExit("runtime direct PipBoy block from regression patch not found")
    text = text.replace(old, new, 1)
path.write_text(text, encoding="utf-8")

# ---------------------------------------------------------------------------
# P1 system menu: use the same deferred stock dispatcher for blocking screens.
# Shared Bag stays custom/non-stock and is not changed.
# ---------------------------------------------------------------------------
path = Path("src/local_coop_system_menu.h")
text = path.read_text(encoding="utf-8")
if "COOP_DEFERRED_SYSTEM_MENU_UI_V1" not in text:
    old = '''    case LocalCoopSystemMenuAction::PipBoy:\n        // COOP_P1_DIRECT_PIPBOY_V1\n        // Pip-Boy is a P1-only global device. Call it directly because the\n        // controller-owned main loop intentionally discards keyboard letters.\n        gLocalCoopModalControllerSlot = 0;\n        phoboiOpen(PIPBOY_OPEN_INTENT_WORLD_MAP);\n        break;\n    case LocalCoopSystemMenuAction::Skilldex:\n        gLocalCoopSkilldexInvokerSlot = 0;\n        gLocalCoopModalControllerSlot = 0;\n        skilldexOpen();\n        break;\n    case LocalCoopSystemMenuAction::Character:\n        gLocalCoopModalControllerSlot = 0;\n        characterEditorShow(false);\n        break;'''
    new = '''    case LocalCoopSystemMenuAction::PipBoy:\n        // COOP_P1_DIRECT_PIPBOY_V1\n        // COOP_DEFERRED_SYSTEM_MENU_UI_V1\n        gLocalCoopModalControllerSlot = 0;\n        localCoopRequestDeferredGameKey(KEY_LOWERCASE_P, 0);\n        break;\n    case LocalCoopSystemMenuAction::Skilldex:\n        gLocalCoopSkilldexInvokerSlot = 0;\n        gLocalCoopModalControllerSlot = 0;\n        localCoopRequestDeferredGameKey(KEY_LOWERCASE_S, 0);\n        break;\n    case LocalCoopSystemMenuAction::Character:\n        gLocalCoopModalControllerSlot = 0;\n        localCoopRequestDeferredGameKey(KEY_LOWERCASE_C, 0);\n        break;'''
    if old not in text:
        raise SystemExit("system-menu direct blocking UI action block not found")
    text = text.replace(old, new, 1)
path.write_text(text, encoding="utf-8")

# ---------------------------------------------------------------------------
# Main loop consumer: dispatch after the co-op runtime has returned. This uses
# gameHandleKey so Fallout's interface-enabled checks, sounds, combat refusal,
# and established PipBoy/Skilldex/Character entry paths all remain authoritative.
# ---------------------------------------------------------------------------
path = Path("src/main.cc")
text = path.read_text(encoding="utf-8")
if "COOP_DEFERRED_GLOBAL_UI_MAIN_CONSUMER_V1" not in text:
    anchor = '''        localCoopRuntimeTick();\n\n        // SFALL: MainLoopHook.'''
    replacement = '''        localCoopRuntimeTick();\n\n        // COOP_DEFERRED_GLOBAL_UI_MAIN_CONSUMER_V1\n        // Blocking Fallout screens must begin only after localCoopRuntimeTick\n        // released gLocalCoopRuntimeInsideTick. Their inner input loops can then\n        // call the co-op ticker normally for controller/phone navigation/close.\n        int deferredGameSlot = -1;\n        int deferredGameKey = localCoopTakeDeferredGameKey(&deferredGameSlot);\n        if (deferredGameKey != -1) {\n            gLocalCoopModalControllerSlot = deferredGameSlot;\n            gameHandleKey(deferredGameKey, false);\n        }\n\n        // SFALL: MainLoopHook.'''
    if anchor not in text:
        raise SystemExit("mainLoop post-runtime anchor missing")
    text = text.replace(anchor, replacement, 1)
path.write_text(text, encoding="utf-8")

for p, marker in (
    ("src/local_coop.h", "COOP_DEFERRED_GLOBAL_UI_REQUEST_V1"),
    ("src/local_coop_runtime.h", "COOP_DEFERRED_PIPBOY_REQUEST_V1"),
    ("src/local_coop_system_menu.h", "COOP_DEFERRED_SYSTEM_MENU_UI_V1"),
    ("src/main.cc", "COOP_DEFERRED_GLOBAL_UI_MAIN_CONSUMER_V1"),
):
    if marker not in Path(p).read_text(encoding="utf-8"):
        raise SystemExit(f"missing deferred global UI marker {marker} in {p}")

print("Deferred blocking co-op global UI dispatch outside the runtime ticker")
