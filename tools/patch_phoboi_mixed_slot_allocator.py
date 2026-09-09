#!/usr/bin/env python3
from pathlib import Path
import re
import runpy


def read(path):
    return Path(path).read_text(encoding="utf-8")


def write(path, text):
    Path(path).write_text(text, encoding="utf-8")


# ---------------------------------------------------------------------------
# Final PhoBoi slot ownership model:
#   - four saved co-op character slots
#   - physical XInput and phones share P2-P4 dynamically
#   - a transport disconnect never deletes the saved character
#   - disconnected P2-P4 actors remain hidden/non-blocking ghosts
#   - stale phone transports release quickly instead of blocking a slot 60 sec
# ---------------------------------------------------------------------------
mobile_path = "src/local_coop_mobile.cc"
text = read(mobile_path)

if "PHOBOI_MIXED_SLOT_ALLOCATOR_V1" not in text:
    # Remember when the LAST live websocket disappeared. Unlike lastSeen this is
    # never started by a successful /claim before its first sockets have opened,
    # so slow Cloudflare handshakes cannot be mistaken for a disconnect.
    state_anchor = '''    std::atomic<int> controlConnections { 0 };
    std::atomic<int> streamConnections { 0 };
'''
    state_replacement = '''    std::atomic<int> controlConnections { 0 };
    std::atomic<int> streamConnections { 0 };
    // PHOBOI_STALE_TRANSPORT_RELEASE_V1
    std::atomic<uint64_t> transportLostAt { 0 };
'''
    if state_anchor not in text:
        raise SystemExit("PhoBoi websocket-count state anchor missing")
    text = text.replace(state_anchor, state_replacement, 1)

    # Add final ownership helpers after mobileResetInput, where all structures
    # and the input reset helper are available.
    reset_pattern = re.compile(
        r'''void mobileResetInput\(MobileSlotState& state\)\n\{.*?\n\}\n\nvoid mobileHandleClient''',
        re.S,
    )
    reset_match = reset_pattern.search(text)
    if reset_match is None:
        raise SystemExit("PhoBoi mobileResetInput/mobileHandleClient boundary missing")
    reset_block = reset_match.group(0)
    reset_function = reset_block[:-len("\n\nvoid mobileHandleClient")]
    helpers = r'''

// PHOBOI_MIXED_SLOT_ALLOCATOR_V1
constexpr uint64_t kMobileTransportReleaseGraceMs = 1200;

bool mobilePhoneTransportLive(int slot)
{
    if (slot <= 0 || slot >= kLocalCoopMaxPlayers) return false;
    const MobileSlotState& state = gMobileSlots[slot];
    return state.controlConnections.load() > 0 || state.streamConnections.load() > 0;
}

bool mobileSlotOwnedByPhysicalController(int slot)
{
    if (slot <= 0 || slot >= kLocalCoopMaxPlayers) return false;
    const bool mobileAttached = gMobileDevices[slot].deviceIndex >= 0;
    return gLocalCoopPlayers[slot].connected && !mobileAttached;
}

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

bool mobileReleaseStaleTransportClaim(int slot, uint64_t now)
{
    if (slot <= 0 || slot >= kLocalCoopMaxPlayers) return false;
    MobileSlotState& state = gMobileSlots[slot];
    if (!state.claimed.load() || mobilePhoneTransportLive(slot)) return false;

    uint64_t lostAt = state.transportLostAt.load();
    if (lostAt == 0 || now < lostAt || now - lostAt < kMobileTransportReleaseGraceMs) {
        return false;
    }

    state.claimed.store(false);
    mobileResetInput(state);
    debugPrint("[PHOBOI SLOT] player %d phone transport released; saved character remains reserved ghost\n", slot + 1);
    return true;
}

bool mobileSlotPhoneClaimable(int slot)
{
    if (slot <= 0 || slot >= kLocalCoopMaxPlayers) return false;
    if (mobileSlotOwnedByPhysicalController(slot) || mobilePhoneTransportLive(slot)) return false;

    const MobileSlotState& state = gMobileSlots[slot];
    if (!state.claimed.load()) return true;
    uint64_t lostAt = state.transportLostAt.load();
    uint64_t now = mobileNow();
    return lostAt != 0 && now >= lostAt && now - lostAt >= kMobileTransportReleaseGraceMs;
}

const char* mobileSlotOwnershipLabel(int slot)
{
    if (mobileSlotOwnedByPhysicalController(slot)) return "XINPUT";
    if (mobilePhoneTransportLive(slot) || gMobileSlots[slot].claimed.load()) return "PHONE";
    if (gLocalCoopPlayers[slot].slotLocked) return "RESERVED";
    return "OPEN";
}
'''
    text = text[:reset_match.start()] + reset_function + helpers + "\n\nvoid mobileHandleClient" + text[reset_match.end():]

    # A websocket opening cancels the disconnect grace timer. Closing the last
    # control/stream socket starts it.
    control_open = '''    state.controlConnections.fetch_add(1);
    while (gMobileServerRunning.load()
'''
    control_open_new = '''    state.controlConnections.fetch_add(1);
    mobileMarkTransportAlive(slot);
    while (gMobileServerRunning.load()
'''
    if control_open not in text:
        raise SystemExit("PhoBoi control websocket open anchor missing")
    text = text.replace(control_open, control_open_new, 1)

    control_close = '''    int remaining = state.controlConnections.fetch_sub(1) - 1;
    uint64_t age = mobileNow() - state.lastSeen.load();
'''
    control_close_new = '''    int remaining = state.controlConnections.fetch_sub(1) - 1;
    mobileMarkTransportClosedIfLast(slot);
    uint64_t age = mobileNow() - state.lastSeen.load();
'''
    if control_close not in text:
        raise SystemExit("PhoBoi control websocket close anchor missing")
    text = text.replace(control_close, control_close_new, 1)

    stream_open = '''    MobileSlotState& streamState = gMobileSlots[slot];
    streamState.streamConnections.fetch_add(1);
'''
    stream_open_new = '''    MobileSlotState& streamState = gMobileSlots[slot];
    streamState.streamConnections.fetch_add(1);
    mobileMarkTransportAlive(slot);
'''
    if stream_open not in text:
        raise SystemExit("PhoBoi stream websocket open anchor missing")
    text = text.replace(stream_open, stream_open_new, 1)

    stream_close = '''    int remainingStreams = streamState.streamConnections.fetch_sub(1) - 1;
    debugPrint("[PHOBOI WS] stream closed slot=%d remaining=%d\\n", slot + 1, remainingStreams);
'''
    stream_close_new = '''    int remainingStreams = streamState.streamConnections.fetch_sub(1) - 1;
    mobileMarkTransportClosedIfLast(slot);
    debugPrint("[PHOBOI WS] stream closed slot=%d remaining=%d\\n", slot + 1, remainingStreams);
'''
    if stream_close not in text:
        raise SystemExit("PhoBoi stream websocket close anchor missing")
    text = text.replace(stream_close, stream_close_new, 1)

    # Slot status is deliberately before per-player slot validation: the join
    # page needs to ask which P2-P4 positions are available without claiming one.
    slot_parse_anchor = '''    int slot = mobileValue(values, "slot", -1);
    if (slot <= 0 || slot >= kLocalCoopMaxPlayers) {
'''
    slot_endpoint = r'''    // PHOBOI_SLOT_STATUS_ENDPOINT_V1
    if (method == "GET" && route == "/slots") {
        uint64_t now = mobileNow();
        for (int candidate = 1; candidate < kLocalCoopMaxPlayers; candidate++) {
            mobileReleaseStaleTransportClaim(candidate, now);
        }

        std::ostringstream body;
        body << "{\"ok\":true,\"slots\":[";
        for (int candidate = 1; candidate < kLocalCoopMaxPlayers; candidate++) {
            if (candidate > 1) body << ',';
            body << "{\"slot\":" << candidate
                 << ",\"player\":" << candidate + 1
                 << ",\"status\":\"" << mobileSlotOwnershipLabel(candidate) << "\""
                 << ",\"claimable\":" << (mobileSlotPhoneClaimable(candidate) ? "true" : "false")
                 << ",\"reserved\":" << (gLocalCoopPlayers[candidate].slotLocked ? "true" : "false")
                 << '}';
        }
        body << "]}";
        mobileSendResponse(client, "200 OK", "application/json", body.str());
        return;
    }

    int slot = mobileValue(values, "slot", -1);
    if (slot <= 0 || slot >= kLocalCoopMaxPlayers) {
'''
    if slot_parse_anchor not in text:
        raise SystemExit("PhoBoi slot parsing anchor missing")
    text = text.replace(slot_parse_anchor, slot_endpoint, 1)

    # Replace the generated /claim block as one unit. The session PIN remains
    # host admission. A saved/locked character additionally requires its rejoin
    # code. Physical controllers and genuinely live phone sockets always win.
    claim_pattern = re.compile(
        r'''    if \(method == "POST" && route == "/claim"\) \{.*?\n    \}\n\n    uint32_t token = mobileUnsignedValue\(values, "token", 0\);''',
        re.S,
    )
    claim_match = claim_pattern.search(text)
    if claim_match is None:
        raise SystemExit("generated PhoBoi /claim block not found")
    claim_block = r'''    if (method == "POST" && route == "/claim") {
        int pin = mobileValue(values, "pin", -1);
        if (pin != gMobilePin) {
            mobileSendResponse(client, "403 Forbidden", "application/json", "{\"ok\":false,\"error\":\"Wrong session PIN\"}");
            return;
        }

        if (mobileSlotOwnedByPhysicalController(slot)) {
            mobileSendResponse(client, "409 Conflict", "application/json", "{\"ok\":false,\"error\":\"That player slot is using an XInput controller\"}");
            return;
        }

        if (gLocalCoopPlayers[slot].slotLocked) {
            int suppliedRejoin = mobileValue(values, "rejoin", -1);
            int expectedRejoin = mobileRejoinCodeForSlot(slot);
            if (suppliedRejoin != expectedRejoin) {
                mobileSendResponse(client, "403 Forbidden", "application/json", "{\"ok\":false,\"error\":\"Reserved character: enter the REJOIN code shown on the host\"}");
                return;
            }
        }

        uint64_t now = mobileNow();
        mobileReleaseStaleTransportClaim(slot, now);
        if (state.claimed.load()) {
            const char* error = mobilePhoneTransportLive(slot)
                ? "That player slot already has a phone connected"
                : "That player slot is reconnecting; retry in a moment";
            std::ostringstream body;
            body << "{\"ok\":false,\"error\":\"" << error << "\"}";
            mobileSendResponse(client, "409 Conflict", "application/json", body.str());
            return;
        }

        bool expected = false;
        if (!state.claimed.compare_exchange_strong(expected, true)) {
            mobileSendResponse(client, "409 Conflict", "application/json", "{\"ok\":false,\"error\":\"That player slot was just claimed\"}");
            return;
        }

        // A new claim has not lost transport yet; websocket open/close owns this timer.
        state.transportLostAt.store(0);
        uint32_t token = mobileNextToken();
        state.token.store(token);
        state.lastSeen.store(now);
        mobileResetInput(state);

        std::ostringstream body;
        body << "{\"ok\":true,\"slotIndex\":" << slot
             << ",\"player\":" << slot + 1
             << ",\"token\":" << token
             << ",\"rejoinCode\":" << mobileRejoinCodeForSlot(slot) << "}";
        mobileSendResponse(client, "200 OK", "application/json", body.str());
        debugPrint("[PHOBOI SLOT] player %d claimed by phone reserved=%d\n",
            slot + 1, gLocalCoopPlayers[slot].slotLocked ? 1 : 0);
        return;
    }

    uint32_t token = mobileUnsignedValue(values, "token", 0);'''
    text = text[:claim_match.start()] + claim_block + text[claim_match.end():]

    # Resume and websocket token reclaim clear any pending transport-loss timer.
    resume_anchor = '''            state.claimed.store(true);
            state.lastSeen.store(mobileNow());
            mobileResetInput(state);
'''
    resume_new = '''            state.claimed.store(true);
            state.transportLostAt.store(0);
            state.lastSeen.store(mobileNow());
            mobileResetInput(state);
'''
    # There are up to three generated copies (resume/ws/stream reclaim).
    if resume_anchor not in text:
        raise SystemExit("PhoBoi token-resume state anchor missing")
    text = text.replace(resume_anchor, resume_new)

    release_anchor = '''        state.claimed.store(false);
        state.token.store(0);
        mobileSendResponse(client, "200 OK", "application/json", "{\\\"ok\\\":true}");
'''
    release_new = '''        state.claimed.store(false);
        state.token.store(0);
        state.transportLostAt.store(0);
        mobileSendResponse(client, "200 OK", "application/json", "{\\\"ok\\\":true}");
'''
    if release_anchor in text:
        text = text.replace(release_anchor, release_new, 1)

    # Run the short disconnect release before occupancy/attach decisions. The
    # existing 60-second timeout remains only as a defensive dead-transport cap.
    tick_anchor = '''        bool mobileAttached = gMobileDevices[slot].deviceIndex >= 0;
'''
    tick_new = '''        // PHOBOI_STALE_TRANSPORT_RELEASE_V1
        mobileReleaseStaleTransportClaim(slot, now);

        bool mobileAttached = gMobileDevices[slot].deviceIndex >= 0;
'''
    if tick_anchor not in text:
        raise SystemExit("PhoBoi mobile tick attachment anchor missing")
    text = text.replace(tick_anchor, tick_new, 1)

    # Host kick is an explicit transport release. It never clears slotLocked or
    # destroys the character actor; the subsequent detach hides the ghost.
    kick_anchor = '''        gMobileSlots[slot].claimed.store(false);
        gMobileSlots[slot].token.store(0);
        mobileDrawHostWindow();
'''
    kick_new = '''        gMobileSlots[slot].claimed.store(false);
        gMobileSlots[slot].token.store(0);
        gMobileSlots[slot].transportLostAt.store(0);
        mobileDrawHostWindow();
'''
    if kick_anchor not in text:
        raise SystemExit("PhoBoi host-kick anchor missing")
    text = text.replace(kick_anchor, kick_new, 1)

    write(mobile_path, text)


# ---------------------------------------------------------------------------
# Phone join page: show live P2-P4 ownership instead of blindly offering busy
# slots. P1 remains the host controller slot and is intentionally absent.
# ---------------------------------------------------------------------------
text = read(mobile_path)
function_start_token = "const char* mobileControllerHtml()\n{"
next_function_token = "\n\nstd::string mobileReadRequest"
start = text.find(function_start_token)
end = text.find(next_function_token, start)
if start == -1 or end == -1:
    raise SystemExit("PhoBoi controller HTML function boundaries missing")
function_text = text[start:end]
chunks = re.findall(r'R"PHOBOI\((.*?)\)PHOBOI"', function_text, flags=re.S)
if not chunks:
    raise SystemExit("PhoBoi controller HTML chunks missing")
html = "".join(chunks)

if "PHOBOI_PHONE_SLOT_STATUS_UI_V1" not in html:
    js_anchor = "const $=id=>document.getElementById(id);\n"
    if js_anchor not in html:
        raise SystemExit("PhoBoi JS helper anchor missing")
    slot_js = r'''// PHOBOI_PHONE_SLOT_STATUS_UI_V1
let phoboiSlotRefreshBusy=false;
async function phoboiRefreshSlots(){
 if(phoboiSlotRefreshBusy)return;phoboiSlotRefreshBusy=true;
 try{
  const r=await fetch('/slots',{cache:'no-store'});const j=await r.json();if(!j.ok||!Array.isArray(j.slots))return;
  const sel=$('slot');let first=null;
  for(const s of j.slots){
   const opt=sel.querySelector(`option[value="${s.slot}"]`);if(!opt)continue;
   opt.textContent=`Player ${s.player} — ${s.status}`;
   opt.disabled=!s.claimable;
   opt.dataset.status=s.status;
   if(s.claimable&&first===null)first=String(s.slot);
  }
  const current=sel.options[sel.selectedIndex];
  if((!current||current.disabled)&&first!==null)sel.value=first;
  if(first===null&&$('join').style.display!=='none')$('msg').textContent='All co-op slots are currently controlled.';
 }catch(_){
 }finally{phoboiSlotRefreshBusy=false}
}
phoboiRefreshSlots();setInterval(phoboiRefreshSlots,1000);
'''
    html = html.replace(js_anchor, js_anchor + slot_js, 1)

    # Refresh ownership immediately after a failed claim so a just-disconnected
    # phone can turn from PHONE to RESERVED/OPEN without a page reload.
    catch_anchor = "}catch(e){$('msg').textContent=e.message}};"
    catch_new = "}catch(e){$('msg').textContent=e.message;phoboiRefreshSlots()}};"
    if catch_anchor not in html:
        raise SystemExit("PhoBoi connect error anchor missing")
    html = html.replace(catch_anchor, catch_new, 1)

    new_function = '''const char* mobileControllerHtml()\n{\n    static const std::string html = R"PHOBOI(''' + html + ''')PHOBOI";\n    return html.c_str();\n}'''
    text = text[:start] + new_function + text[end:]
    write(mobile_path, text)

    # Re-split the completed page so this final UI cannot reintroduce MSVC C2026.
    runpy.run_path("tools/patch_phoboi_msvc_final_split.py", run_name="__main__")


# ---------------------------------------------------------------------------
# XInput scanner: do NOT stop just because there is no never-used free slot.
# localCoopOpenController already knows how to reclaim a disconnected reserved
# character by GUID; the old premature break prevented that code from running.
# ---------------------------------------------------------------------------
coop_path = "src/local_coop.h"
coop = read(coop_path)
if "COOP_RECONNECT_RESERVED_CONTROLLER_SCAN_V1" not in coop:
    old_scan = '''    int joystickCount = SDL_NumJoysticks();
    for (int deviceIndex = 0; deviceIndex < joystickCount; deviceIndex++) {
        if (localCoopFindFreeControllerSlot() == -1) {
            break;
        }
        localCoopOpenController(deviceIndex);
    }
'''
    new_scan = '''    int joystickCount = SDL_NumJoysticks();
    for (int deviceIndex = 0; deviceIndex < joystickCount; deviceIndex++) {
        // COOP_RECONNECT_RESERVED_CONTROLLER_SCAN_V1
        // Always let localCoopOpenController inspect the device GUID first.
        // It may belong to a saved ghost even when no never-used slot exists.
        localCoopOpenController(deviceIndex);
    }
'''
    if old_scan not in coop:
        raise SystemExit("localCoopRefreshControllers free-slot break anchor missing")
    coop = coop.replace(old_scan, new_scan, 1)

if "COOP_SAVED_GHOST_SLOT_V1" not in coop:
    ghost_anchor = '''    if (player.slotLocked && player.slot > 0 && player.actor != nullptr) {
        reg_anim_clear(player.actor);
        player.actor->flags |= OBJECT_HIDDEN | OBJECT_NO_BLOCK;
        debugPrint("[COOP JOIN] slot=%d disconnected; ghost reserved\\n", player.slot);
    }
'''
    ghost_new = '''    if (player.slotLocked && player.slot > 0 && player.actor != nullptr) {
        // COOP_SAVED_GHOST_SLOT_V1
        // Controller/phone ownership is transport only. Keep this character
        // identity in the custom save roster and keep its live actor as an
        // invisible, non-blocking ghost until the same slot is reclaimed.
        LocalCoopCharacterSlotState& saved = localCoopCharacterStateGet().slots[player.slot];
        saved.locked = 1;
        saved.archetype = static_cast<uint8_t>(std::clamp(player.archetype, 0, kLocalCoopArchetypeCount - 1));
        saved.gender = static_cast<uint8_t>(player.gender != 0 ? 1 : 0);
        snprintf(saved.controllerGuid, sizeof(saved.controllerGuid), "%s", player.controllerGuid);
        reg_anim_clear(player.actor);
        player.actor->flags |= OBJECT_HIDDEN | OBJECT_NO_BLOCK;
        debugPrint("[COOP JOIN] slot=%d disconnected; saved character ghost reserved\\n", player.slot);
    }
'''
    if ghost_anchor not in coop:
        raise SystemExit("localCoopClearController ghost anchor missing")
    coop = coop.replace(ghost_anchor, ghost_new, 1)

write(coop_path, coop)


# Hard guards: this final pass must own the generated behavior.
for filename, marker in (
    (mobile_path, "PHOBOI_MIXED_SLOT_ALLOCATOR_V1"),
    (mobile_path, "PHOBOI_STALE_TRANSPORT_RELEASE_V1"),
    (mobile_path, "PHOBOI_SLOT_STATUS_ENDPOINT_V1"),
    (mobile_path, "PHOBOI_PHONE_SLOT_STATUS_UI_V1"),
    (coop_path, "COOP_RECONNECT_RESERVED_CONTROLLER_SCAN_V1"),
    (coop_path, "COOP_SAVED_GHOST_SLOT_V1"),
):
    if marker not in read(filename):
        raise SystemExit(f"missing mixed-slot final marker {marker} in {filename}")

final_mobile = read(mobile_path)
if "That player slot is busy" in final_mobile:
    raise SystemExit("legacy generic busy-slot claim response survived mixed allocator")
if "localCoopFindFreeControllerSlot() == -1) {\n            break;" in read(coop_path):
    raise SystemExit("premature XInput free-slot break survived")

print("Installed mixed XInput/phone allocator with reclaimable saved ghost characters")
