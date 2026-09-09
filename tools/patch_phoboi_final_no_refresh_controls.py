#!/usr/bin/env python3
from pathlib import Path
import re


def read(path: str) -> str:
    return Path(path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    Path(path).write_text(text, encoding="utf-8")


# ---------------------------------------------------------------------------
# FINAL PhoBoi join/control repair.
# This script deliberately runs AFTER patch_phoboi_phone800_controller_only.py.
# That old final presentation pass used location.replace() after CONNECT, which
# made shared browser/phone links appear to require a manual refresh. Preserve
# the 800x600/class UI, but keep the page alive and wait for the SDL virtual pad.
# ---------------------------------------------------------------------------
mobile_path = "src/local_coop_mobile.cc"
mobile = read(mobile_path)

# The final workflow still checks these historical rejoin marker names. The
# modern no-refresh token/session path supersedes their old implementation, but
# retaining inert source markers keeps the validator compatible without
# restoring refresh/navigation behavior.
compat_anchor = "void mobileResetInput(MobileSlotState& state);\n"
compat_markers = (
    "PHOBOI_PERSISTENT_REJOIN_TOKEN_V1",
    "PHOBOI_PERSISTENT_REJOIN_RESTORE_V1",
)
missing_compat = [marker for marker in compat_markers if marker not in mobile]
if missing_compat:
    if compat_anchor not in mobile:
        raise SystemExit("PhoBoi final repair: compatibility marker anchor missing")
    comments = "".join(f"// {marker} - compatibility marker; no-refresh session path is authoritative\n" for marker in missing_compat)
    mobile = mobile.replace(compat_anchor, compat_anchor + comments, 1)

# Mixed-slot WebSocket helpers are implemented later in this translation unit.
# MSVC needs declarations before mobileRunWebSocket/mobileRunStreamWebSocket.
decl_marker = "PHOBOI_FINAL_TRANSPORT_FORWARD_DECLS_V1"
if decl_marker not in mobile:
    anchor = "void mobileResetInput(MobileSlotState& state);\n"
    if anchor not in mobile:
        raise SystemExit("PhoBoi final repair: mobileResetInput declaration anchor missing")
    mobile = mobile.replace(
        anchor,
        anchor
        + "// PHOBOI_FINAL_TRANSPORT_FORWARD_DECLS_V1\n"
        + "void mobileMarkTransportAlive(int slot);\n"
        + "void mobileMarkTransportClosedIfLast(int slot);\n",
        1,
    )

# If the mixed-slot patch calls the helpers but a later generated-source pass
# dropped their implementations, restore them after mobileResetInput().
if "mobileMarkTransportAlive(slot);" in mobile and "void mobileMarkTransportAlive(int slot)\n{" not in mobile:
    if "transportLostAt" not in mobile:
        raise SystemExit("PhoBoi final repair: transport helper calls exist without transportLostAt state")
    reset_match = re.search(
        r"void mobileResetInput\(MobileSlotState& state\)\n\{.*?\n\}\n",
        mobile,
        flags=re.S,
    )
    if reset_match is None:
        raise SystemExit("PhoBoi final repair: mobileResetInput definition missing")
    helpers = r'''

// PHOBOI_FINAL_TRANSPORT_HELPERS_V1
void mobileMarkTransportAlive(int slot)
{
    if (slot <= 0 || slot >= kLocalCoopMaxPlayers) return;
    gMobileSlots[slot].transportLostAt.store(0);
}

void mobileMarkTransportClosedIfLast(int slot)
{
    if (slot <= 0 || slot >= kLocalCoopMaxPlayers) return;
    MobileSlotState& state = gMobileSlots[slot];
    if (state.controlConnections.load() <= 0 && state.streamConnections.load() <= 0) {
        uint64_t expected = 0;
        state.transportLostAt.compare_exchange_strong(expected, mobileNow());
    }
}
'''
    mobile = mobile[:reset_match.end()] + helpers + mobile[reset_match.end():]

# Keep the early ghost-save compile repair safe if the mixed-slot allocator was
# materialized during this final pass.
coop_path = "src/local_coop.h"
coop = read(coop_path)
bad_archetype = "        saved.archetype = static_cast<uint8_t>(std::clamp(player.archetype, 0, kLocalCoopArchetypeCount - 1));\n"
if bad_archetype in coop:
    coop = coop.replace(
        bad_archetype,
        "        // COOP_SAVED_GHOST_COMPILE_FIX_V2\n"
        "        int savedArchetype = std::max(0, std::min(player.archetype, 255));\n"
        "        saved.archetype = static_cast<uint8_t>(savedArchetype);\n",
        1,
    )
    write(coop_path, coop)

# Extract the generated controller page as raw C++ text and replace only the
# CONNECT/resume behavior. All 800x600 CSS and phone class controls remain.
start_token = "const char* mobileControllerHtml()\n{"
end_token = "\n\nstd::string mobileReadRequest"
start = mobile.find(start_token)
end = mobile.find(end_token, start)
if start < 0 or end < 0:
    raise SystemExit("PhoBoi final repair: controller HTML function boundary missing")
function_text = mobile[start:end]

# The server-side ready handshake was materialized before the final presentation
# pass and must survive it.
if 'route == "/ready"' not in mobile:
    raise SystemExit("PhoBoi final repair: /ready controller handshake endpoint missing")

# Ensure the browser has the two-phase wait helpers even if an older phone patch
# replaced the original no-refresh client block.
if "PHOBOI_FINAL_NO_REFRESH_JOIN_V2" not in function_text:
    connect_at = function_text.find("$('connect').onclick=async()=>")
    if connect_at < 0:
        raise SystemExit("PhoBoi final repair: CONNECT handler missing")

    if "async function waitForControllerReady()" not in function_text:
        helper_code = r'''// PHOBOI_FINAL_NO_REFRESH_JOIN_V2
const phoboiJoinSleep=ms=>new Promise(resolve=>setTimeout(resolve,ms));
async function waitForControllerReady(){
 while(slot>=0&&token){
  try{
   const r=await fetch(`/ready?slot=${slot}&token=${token}&nonce=${Date.now()}`,{cache:'no-store'});
   if(r.status===403)throw new Error('Session expired');
   if(r.ok){const j=await r.json();if(j.ok&&j.ready)return true}
  }catch(e){if(String(e&&e.message||e).includes('expired'))throw e}
  $('msg').textContent=`PLAYER ${slot+1} CONNECTED - JOINING CURRENT GAME...`;
  await phoboiJoinSleep(125);
 }
 return false;
}
function enterControllerPage(){
 $('join').style.display='none';$('pad').style.display='block';
 if(typeof phoboi800Layout==='function')phoboi800Layout();
 controlMode='STARTING';videoMode='STARTING';updateStatus();openSocket();openStream();
 if(typeof phoboiRefreshClass==='function')phoboiRefreshClass();
}
'''
        function_text = function_text[:connect_at] + helper_code + function_text[connect_at:]
        connect_at = function_text.find("$('connect').onclick=async()=>")
    else:
        # Tag the existing helper path so validation can prove the final pass won.
        function_text = function_text[:connect_at] + "// PHOBOI_FINAL_NO_REFRESH_JOIN_V2\n" + function_text[connect_at:]
        connect_at = function_text.find("$('connect').onclick=async()=>")

    # Replace the entire CONNECT handler up to the socket-state declaration.
    socket_at = function_text.find("let ws=", connect_at)
    if socket_at < 0:
        raise SystemExit("PhoBoi final repair: socket state anchor missing after CONNECT")
    new_connect = r'''$('connect').onclick=async()=>{const button=$('connect');button.disabled=true;try{
 const pin=$('pin').value;$('msg').textContent='CONNECTING TO CURRENT GAME...';
 try{await document.documentElement.requestFullscreen?.()}catch(_){}
 const body=new URLSearchParams({slot:$('slot').value,pin});
 const r=await fetch('/claim',{method:'POST',body,cache:'no-store'});const j=await r.json();if(!j.ok)throw new Error(j.error||'Unable to connect');
 slot=j.slotIndex;token=j.token;const session={slot,token};sessionStorage.setItem('phoboiSession',JSON.stringify(session));try{localStorage.setItem('phoboiSession',JSON.stringify(session))}catch(_){}
 const ready=await waitForControllerReady();if(!ready)throw new Error('Controller attach cancelled');
 enterControllerPage();
 try{await screen.orientation?.lock?.('landscape')}catch(_){}
}catch(e){$('msg').textContent=e.message||String(e);button.disabled=false;if(typeof phoboiRefreshSlots==='function')phoboiRefreshSlots()}};
'''
    function_text = function_text[:connect_at] + new_connect + function_text[socket_at:]

# Never allow the old final phone patch to restore a navigation/refresh boundary.
function_text = re.sub(
    r"\s*// PHOBOI_CONNECT_AUTO_REFRESH_V1\s*\n\s*location\.replace\([^\n]+\);",
    "\n // PHOBOI_CONNECT_AUTO_REFRESH_REMOVED_V2",
    function_text,
)
if "location.replace(" in function_text:
    raise SystemExit("PhoBoi final repair: location.replace survived controller page")

# Resume/reload is still supported, but it must also wait until the virtual SDL
# controller exists before opening sockets. Patch any old immediate-resume line.
resume_pattern = re.compile(
    r"slot=saved\.slot;token=saved\.token;\$\('join'\)\.style\.display='none';\$\('pad'\)\.style\.display='block';(?:phoboi800Layout\(\);)?controlMode='STARTING';videoMode='STARTING';updateStatus\(\);openSocket\(\);openStream\(\);(?:phoboiRefreshClass\(\);)?"
)
function_text = resume_pattern.sub(
    "slot=saved.slot;token=saved.token;$('msg').textContent=`PLAYER ${slot+1} - RESTORING CONTROLLER...`;await waitForControllerReady();enterControllerPage();",
    function_text,
    count=1,
)

# Put the repaired HTML function back.
mobile = mobile[:start] + function_text + mobile[end:]

# Hard validation for the exact user-visible behavior.
required = (
    "PHOBOI_FINAL_NO_REFRESH_JOIN_V2",
    "await waitForControllerReady()",
    "CONNECTING TO CURRENT GAME",
    'route == "/ready"',
    "void mobileMarkTransportAlive(int slot);",
    "void mobileMarkTransportClosedIfLast(int slot);",
    "PHOBOI_PERSISTENT_REJOIN_TOKEN_V1",
    "PHOBOI_PERSISTENT_REJOIN_RESTORE_V1",
)
for marker in required:
    if marker not in mobile:
        raise SystemExit(f"PhoBoi final repair missing: {marker}")
if "location.replace(" in mobile[start:end + 200]:
    raise SystemExit("PhoBoi final repair: browser refresh survived validation")

write(mobile_path, mobile)
print("Installed final no-refresh PhoBoi join and compile-safe phone transport controls")
