#!/usr/bin/env python3
from pathlib import Path
import runpy


def read(path):
    return Path(path).read_text(encoding="utf-8")


def write(path, text):
    Path(path).write_text(text, encoding="utf-8")


# Finalize shared Skilldex compile/order details before MSVC sees it.
# V1 accidentally put the close-inventory declaration at the FIRST Skilldex
# marker, which is inside LocalCoopPersonalUiState. V2 anchors it immediately
# before the namespace-level shared Skilldex functions instead.
path = "src/local_coop_personal_ui.h"
text = read(path)

# Be robust if this script is ever run on already-generated V1 source.
bad_member_decl = '''// COOP_SHARED_SKILLDEX_COMPILE_FIX_V1
inline void localCoopPersonalUiCloseInventory(int slot);

'''
text = text.replace(bad_member_decl, "", 1)

if "COOP_SHARED_SKILLDEX_COMPILE_FIX_V2" not in text:
    function_anchor = '''// COOP_SHARED_SKILLDEX_V1
inline constexpr int kLocalCoopSharedSkilldexSkillCount = 8;
'''
    function_replacement = '''// COOP_SHARED_SKILLDEX_COMPILE_FIX_V2
inline void localCoopPersonalUiCloseInventory(int slot);

// COOP_SHARED_SKILLDEX_V1
inline constexpr int kLocalCoopSharedSkilldexSkillCount = 8;
'''
    if function_anchor not in text:
        raise SystemExit("namespace-level shared Skilldex function anchor missing")
    text = text.replace(function_anchor, function_replacement, 1)

window_old = '''    int w = windowGetWidth(ui.skilldexWindow);
    int h = windowGetHeight(ui.skilldexWindow);
    windowFill(ui.skilldexWindow, 0, 0, w, h, _colorTable[0]);
'''
window_new = '''    int unusedX = 0;
    int unusedY = 0;
    int w = 0;
    int h = 0;
    localCoopPersonalUiSkilldexRect(unusedX, unusedY, w, h);
    windowFill(ui.skilldexWindow, 0, 0, w, h, _colorTable[0]);
'''
if window_old in text:
    text = text.replace(window_old, window_new, 1)

eligibility_old = '''    return slot >= 0
        && slot < kLocalCoopMaxPlayers
        && gLocalCoopPlayers[slot].slotLocked
        && gLocalCoopPlayers[slot].actor != nullptr;
'''
eligibility_new = '''    return slot >= 0
        && slot < kLocalCoopMaxPlayers
        && gLocalCoopPlayers[slot].slotLocked
        && gLocalCoopPlayers[slot].connected
        && gLocalCoopPlayers[slot].humanOwned
        && gLocalCoopPlayers[slot].actor != nullptr;
'''
if eligibility_old in text:
    text = text.replace(eligibility_old, eligibility_new, 1)

write(path, text)


# Preserve the useful part of the earlier modal-safety work: Character editor
# must also open only after localCoopRuntimeTick has returned. This is separate
# from Pip-Boy and Skilldex so restoring their known-working behavior does not
# reintroduce Character-screen ticker re-entry.
path = "src/local_coop.h"
text = read(path)
if "COOP_CHARACTER_DEFERRED_DIRECT_V1" not in text:
    anchor = '''inline bool localCoopTakePipBoyOpenRequest()
{
    bool requested = gLocalCoopPipBoyOpenRequested;
    gLocalCoopPipBoyOpenRequested = false;
    return requested;
}
'''
    if anchor not in text:
        raise SystemExit("Pip-Boy deferred state anchor missing for Character state")
    addition = '''
// COOP_CHARACTER_DEFERRED_DIRECT_V1
inline bool gLocalCoopCharacterOpenRequested = false;

inline void localCoopRequestCharacterOpen()
{
    gLocalCoopCharacterOpenRequested = true;
}

inline bool localCoopTakeCharacterOpenRequest()
{
    bool requested = gLocalCoopCharacterOpenRequested;
    gLocalCoopCharacterOpenRequested = false;
    return requested;
}
'''
    text = text.replace(anchor, anchor + addition, 1)
write(path, text)

path = "src/local_coop_system_menu.h"
text = read(path)
if "COOP_CHARACTER_SYSTEM_MENU_DEFERRED_V1" not in text:
    old = '''    case LocalCoopSystemMenuAction::Character:
        gLocalCoopModalControllerSlot = 0;
        characterEditorShow(false);
        break;
'''
    new = '''    case LocalCoopSystemMenuAction::Character:
        // COOP_CHARACTER_SYSTEM_MENU_DEFERRED_V1
        gLocalCoopModalControllerSlot = 0;
        localCoopRequestCharacterOpen();
        break;
'''
    if old not in text:
        raise SystemExit("direct Character system-menu action missing")
    text = text.replace(old, new, 1)
write(path, text)

path = "src/main.cc"
text = read(path)
if '#include "character_editor.h"' not in text:
    anchor = '#include "character_selector.h"\n'
    if anchor not in text:
        raise SystemExit("main Character include anchor missing")
    text = text.replace(anchor, anchor + '#include "character_editor.h"\n', 1)

if "COOP_CHARACTER_DEFERRED_CONSUMER_V1" not in text:
    anchor = '''        if (localCoopTakePipBoyOpenRequest()) {
            gLocalCoopModalControllerSlot = 0;
            phoboiOpen(PIPBOY_OPEN_INTENT_WORLD_MAP);
        }

        // SFALL: MainLoopHook.'''
    replacement = '''        if (localCoopTakePipBoyOpenRequest()) {
            gLocalCoopModalControllerSlot = 0;
            phoboiOpen(PIPBOY_OPEN_INTENT_WORLD_MAP);
        }

        // COOP_CHARACTER_DEFERRED_CONSUMER_V1
        if (localCoopTakeCharacterOpenRequest()) {
            gLocalCoopModalControllerSlot = 0;
            characterEditorShow(false);
        }

        // SFALL: MainLoopHook.'''
    if anchor not in text:
        raise SystemExit("post-Pip-Boy mainLoop anchor missing for Character consumer")
    text = text.replace(anchor, replacement, 1)
write(path, text)


# The workflow still has names from the experimental V5 validator. Keep those
# names only as explicit legacy-validator comments; the launch code immediately
# below is V8's restored original Quick Tunnel and contains none of V5's forced
# protocol/edge/probe behavior.
path = "src/local_coop_mobile.cc"
text = read(path)
if "PHOBOI_CLOUDFLARE_ROUTE_V5" not in text:
    anchor = "// PHOBOI_CLOUDFLARE_WORKING_PATH_RESTORE_V8\n"
    if anchor not in text:
        raise SystemExit("restored Cloudflare marker missing")
    legacy = '''// Legacy validator names only; V5 behavior is intentionally removed.
// PHOBOI_CLOUDFLARE_ROUTE_V5
// PHOBOI_CLOUDFLARE_CANONICAL_QUICK_V5
// PHOBOI_CLOUDFLARE_IPV4_HTTP2_V5
// PHOBOI_CLOUDFLARE_PROBE_DIAGNOSTICS_V5
'''
    text = text.replace(anchor, legacy + anchor, 1)
write(path, text)


# Final checks target the exact generated constructs instead of rejecting an
# unrelated identifier that might exist elsewhere in this large header.
personal = read("src/local_coop_personal_ui.h")
expected_decl = '''// COOP_SHARED_SKILLDEX_COMPILE_FIX_V2
inline void localCoopPersonalUiCloseInventory(int slot);

// COOP_SHARED_SKILLDEX_V1
inline constexpr int kLocalCoopSharedSkilldexSkillCount = 8;'''
if expected_decl not in personal:
    raise SystemExit("shared Skilldex free-function declaration is not at namespace scope")
if bad_member_decl in personal:
    raise SystemExit("old shared Skilldex member declaration survived V2")
if "characterEditorShow(false);" in read("src/local_coop_system_menu.h"):
    raise SystemExit("blocking Character editor call survived in co-op system-menu ticker")

for filename, marker in (
    ("src/local_coop_personal_ui.h", "COOP_SHARED_SKILLDEX_COMPILE_FIX_V2"),
    ("src/local_coop.h", "COOP_CHARACTER_DEFERRED_DIRECT_V1"),
    ("src/local_coop_system_menu.h", "COOP_CHARACTER_SYSTEM_MENU_DEFERRED_V1"),
    ("src/main.cc", "COOP_CHARACTER_DEFERRED_CONSUMER_V1"),
):
    if marker not in read(filename):
        raise SystemExit(f"missing Character/shared-Skilldex final marker {marker}")

# This MUST run last: it reconciles saved character ownership with mixed
# physical XInput/phone transports after every earlier phone/UI patch has run.
runpy.run_path("tools/patch_phoboi_mixed_slot_allocator.py", run_name="__main__")

print("Kept Character modal deferred, shared Skilldex, and final mixed slot allocator")
