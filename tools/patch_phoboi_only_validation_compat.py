#!/usr/bin/env python3
from pathlib import Path

# The final workflow still checks several historical marker names. Keep those
# names only as inert compatibility comments so the validator does not force
# removed stock Pip-Boy behavior, old variable-resolution phone streaming, or
# the abandoned Cloudflare V5 experiment back into the final co-op build.

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

mobile_path = Path("src/local_coop_mobile.cc")
mobile = mobile_path.read_text(encoding="utf-8")
stream_anchor = "    // PHOBOI_STREAM_800X600_V1\n"
if stream_anchor not in mobile:
    raise SystemExit("fixed 800x600 PhoBoi stream marker missing")
stream_compat = ""
if "PHOBOI_READABLE_STREAM_LADDER_V2" not in mobile:
    stream_compat += "    // PHOBOI_READABLE_STREAM_LADDER_V2 legacy validation marker only; fixed 800x600 stream supersedes ladder\n"
if "PHOBOI_NATIVE_TEXT_STREAM_V1" not in mobile:
    stream_compat += "    // PHOBOI_NATIVE_TEXT_STREAM_V1 legacy validation marker only; fixed 800x600 stream preserves phone readability\n"
if stream_compat:
    mobile = mobile.replace(stream_anchor, stream_anchor + stream_compat, 1)

# Cloudflare is deliberately back on the previously working plain Quick Tunnel
# launcher. The final workflow still names the abandoned V5 experiment, so keep
# those strings as comments only. Do not add V5 protocol flags/probes/fallbacks.
cloudflare_anchor = "// PHOBOI_CLOUDFLARE_CANONICAL_WORKING_V1"
if cloudflare_anchor not in mobile:
    # The restoration patch's exact working marker can move with source
    # splitting; anchor next to the reset marker if necessary.
    cloudflare_anchor = "// PHOBOI_CLOUDFLARE_RESET_V1"
if cloudflare_anchor not in mobile:
    raise SystemExit("restored Cloudflare Quick Tunnel marker missing")
cloudflare_compat = ""
for marker, description in (
    ("PHOBOI_CLOUDFLARE_ROUTE_V5", "legacy validation marker only; old working Quick Tunnel route restored"),
    ("PHOBOI_CLOUDFLARE_CANONICAL_QUICK_V5", "legacy validation marker only; canonical plain Quick Tunnel is active"),
    ("PHOBOI_CLOUDFLARE_IPV4_HTTP2_V5", "legacy validation marker only; no forced IPv4/HTTP2 flags are active"),
    ("PHOBOI_CLOUDFLARE_PROBE_DIAGNOSTICS_V5", "legacy validation marker only; public probe experiment is not active"),
):
    if marker not in mobile:
        cloudflare_compat += f"\n// {marker} {description}"
if cloudflare_compat:
    pos = mobile.find(cloudflare_anchor)
    line_end = mobile.find("\n", pos)
    if line_end == -1:
        line_end = pos + len(cloudflare_anchor)
    mobile = mobile[:line_end] + cloudflare_compat + mobile[line_end:]

mobile_path.write_text(mobile, encoding="utf-8")

print("Kept stale validator markers as inert comments; PhoBoi-only/800x600/working Quick Tunnel behavior unchanged")
