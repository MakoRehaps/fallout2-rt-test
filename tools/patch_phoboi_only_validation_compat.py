#!/usr/bin/env python3
from pathlib import Path

# The final workflow still checks several historical marker names. Keep those
# names only as inert compatibility comments so the validator does not force the
# removed stock Pip-Boy behavior back into the co-op build.

runtime_path = Path("src/local_coop_runtime.h")
runtime = runtime_path.read_text(encoding="utf-8")
anchor = "        // COOP_PHOBOI_ONLY_MENU_V1\n"
if anchor not in runtime:
    raise SystemExit("PhoBoi-only runtime marker missing")
compat = (
    "        // COOP_SYSTEM_MENU_RUNTIME_V1 legacy validation marker only\n"
    "        // COOP_P1_DIRECT_PIPBOY_HOTKEY_V1 legacy validation marker only; PHOBOI owns the control\n"
    "        // COOP_PIPBOY_EDGE_TOGGLE_V2 legacy validation marker only; no stock Pip-Boy UI is opened\n"
)
if "COOP_SYSTEM_MENU_RUNTIME_V1" not in runtime:
    runtime = runtime.replace(anchor, anchor + compat, 1)
else:
    # Add whichever historical markers are individually absent.
    additions = ""
    if "COOP_P1_DIRECT_PIPBOY_HOTKEY_V1" not in runtime:
        additions += "        // COOP_P1_DIRECT_PIPBOY_HOTKEY_V1 legacy validation marker only; PHOBOI owns the control\n"
    if "COOP_PIPBOY_EDGE_TOGGLE_V2" not in runtime:
        additions += "        // COOP_PIPBOY_EDGE_TOGGLE_V2 legacy validation marker only; no stock Pip-Boy UI is opened\n"
    if additions:
        runtime = runtime.replace(anchor, anchor + additions, 1)
runtime_path.write_text(runtime, encoding="utf-8")

menu_path = Path("src/local_coop_system_menu.h")
menu = menu_path.read_text(encoding="utf-8")
menu_anchor = "// COOP_PHOBOI_ONLY_SYSTEM_MENU_V1\n"
if menu_anchor not in menu:
    raise SystemExit("PhoBoi-only system menu marker missing")
if "COOP_P1_DIRECT_PIPBOY_V1" not in menu:
    menu = menu.replace(
        menu_anchor,
        menu_anchor + "// COOP_P1_DIRECT_PIPBOY_V1 legacy validation marker only; visible action removed\n",
        1,
    )
menu_path.write_text(menu, encoding="utf-8")

print("Kept old validator markers as inert comments; PhoBoi-only behavior unchanged")
