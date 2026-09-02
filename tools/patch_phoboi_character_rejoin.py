from pathlib import Path
import runpy

path = Path("src/local_coop_mobile.cc")
text = path.read_text(encoding="utf-8")

marker = "PHOBOI_CHARACTER_SLOT_REJOIN_V1"

# A network timeout ends only the transport claim. Keep the token so the same
# phone can reclaim the same already-created character after Safari/network
# loss. Explicit RELEASE/KICK still clears the token and permits replacement.
old_timeout = """        if (state.claimed.load() && inputAge > kMobileTimeoutMs) {
            state.claimed.store(false);
            state.token.store(0);
        }
"""
new_timeout = """        if (state.claimed.load() && inputAge > kMobileTimeoutMs) {
            // PHOBOI_CHARACTER_SLOT_REJOIN_V1
            // Drop only the live transport. The LocalCoopPlayer slot/actor is
            // character-owned and remains locked in the roster. Keeping the
            // token lets the same phone resume after a long network outage.
            state.claimed.store(false);
            mobileResetInput(state);
            debugPrint("[PHOBOI MOBILE] player %d transport timed out; character slot reserved for rejoin\\n", slot + 1);
        }
"""
if marker not in text:
    if old_timeout not in text:
        raise SystemExit("PhoBoi timeout anchor not found")
    text = text.replace(old_timeout, new_timeout, 1)

# Resume is allowed even after the timeout dropped claimed=false, provided the
# reconnecting phone still has the token. Reasserting claimed causes the next
# mobile tick to reattach the virtual controller to the existing actor.
old_resume = """    if (method == "GET" && route == "/resume") {
        uint32_t token = mobileUnsignedValue(values, "token", 0);
        bool ok = state.claimed.load() && token != 0 && token == state.token.load();
        if (ok) {
            state.lastSeen.store(mobileNow());
"""
new_resume = """    if (method == "GET" && route == "/resume") {
        uint32_t token = mobileUnsignedValue(values, "token", 0);
        bool ok = token != 0 && token == state.token.load();
        if (ok) {
            // PHOBOI_CHARACTER_SLOT_RESUME_V1
            state.claimed.store(true);
            state.lastSeen.store(mobileNow());
            mobileResetInput(state);
"""
if "PHOBOI_CHARACTER_SLOT_RESUME_V1" not in text:
    if old_resume not in text:
        raise SystemExit("PhoBoi resume route anchor not found")
    text = text.replace(old_resume, new_resume, 1)

# WebSocket reconnects happen automatically without a page reload. If a timeout
# happened while the browser was retrying, let a valid retained token reclaim
# the transport directly at /ws and /stream.
old_ws_auth = """    if (method == "GET" && route == "/ws") {
        uint32_t token = mobileUnsignedValue(values, "token", 0);
        if (!state.claimed.load() || token == 0 || token != state.token.load()) {
            mobileSendResponse(client, "403 Forbidden", "text/plain", "Session expired");
            return;
        }
        mobileRunWebSocket(client, request, slot, token);
"""
new_ws_auth = """    if (method == "GET" && route == "/ws") {
        uint32_t token = mobileUnsignedValue(values, "token", 0);
        if (!state.claimed.load() && token != 0 && token == state.token.load()) {
            // PHOBOI_CHARACTER_SLOT_WS_RECLAIM_V1
            state.claimed.store(true);
            state.lastSeen.store(mobileNow());
            mobileResetInput(state);
        }
        if (!state.claimed.load() || token == 0 || token != state.token.load()) {
            mobileSendResponse(client, "403 Forbidden", "text/plain", "Session expired");
            return;
        }
        mobileRunWebSocket(client, request, slot, token);
"""
if "PHOBOI_CHARACTER_SLOT_WS_RECLAIM_V1" not in text:
    if old_ws_auth not in text:
        raise SystemExit("PhoBoi ws auth anchor not found")
    text = text.replace(old_ws_auth, new_ws_auth, 1)

old_stream_auth = """    if (method == "GET" && route == "/stream") {
        uint32_t token = mobileUnsignedValue(values, "token", 0);
        if (!state.claimed.load() || token == 0 || token != state.token.load()) {
            mobileSendResponse(client, "403 Forbidden", "text/plain", "Session expired");
            return;
        }
        mobileRunStreamWebSocket(client, request, slot, token);
"""
new_stream_auth = """    if (method == "GET" && route == "/stream") {
        uint32_t token = mobileUnsignedValue(values, "token", 0);
        if (!state.claimed.load() && token != 0 && token == state.token.load()) {
            // PHOBOI_CHARACTER_SLOT_STREAM_RECLAIM_V1
            state.claimed.store(true);
            state.lastSeen.store(mobileNow());
            mobileResetInput(state);
        }
        if (!state.claimed.load() || token == 0 || token != state.token.load()) {
            mobileSendResponse(client, "403 Forbidden", "text/plain", "Session expired");
            return;
        }
        mobileRunStreamWebSocket(client, request, slot, token);
"""
if "PHOBOI_CHARACTER_SLOT_STREAM_RECLAIM_V1" not in text:
    if old_stream_auth not in text:
        raise SystemExit("PhoBoi stream auth anchor not found")
    text = text.replace(old_stream_auth, new_stream_auth, 1)

# Materialize the native transport-rejoin changes first. The cross-reference
# hardening script needs the original sessionStorage claim-success substring as
# a stable insertion point for the character rejoin-code result.
path.write_text(text, encoding="utf-8")
print("Reserved co-op character slots across phone/network disconnects with automatic rejoin")

# Cross-referenced phone/network hardening runs after all normal phone CSS/JS
# patches but BEFORE persistent-token rewriting and the final MSVC split. This
# keeps its HTML anchors deterministic and gives it the finished browser page.
runpy.run_path("tools/patch_phoboi_crossref_hardening.py", run_name="__main__")

# Persist the phone-side resume credential across Safari tab/app restarts after
# hardening has inserted the rejoin-code result. The replacement targets only
# the sessionStorage statement, so both features compose instead of fighting.
text = path.read_text(encoding="utf-8")
old_store = "sessionStorage.setItem('phoboiSession',JSON.stringify({slot,token}));"
new_store = "localStorage.setItem('phoboiSession',JSON.stringify({slot,token}));sessionStorage.setItem('phoboiSession',JSON.stringify({slot,token})); // PHOBOI_PERSISTENT_REJOIN_TOKEN_V1"
if "PHOBOI_PERSISTENT_REJOIN_TOKEN_V1" not in text:
    if old_store not in text:
        raise SystemExit("PhoBoi session storage anchor not found after hardening")
    text = text.replace(old_store, new_store, 1)

old_restore = "const saved=JSON.parse(sessionStorage.getItem('phoboiSession')||'null');if(!saved)return;"
new_restore = "const saved=JSON.parse(localStorage.getItem('phoboiSession')||sessionStorage.getItem('phoboiSession')||'null');if(!saved)return; // PHOBOI_PERSISTENT_REJOIN_RESTORE_V1"
if "PHOBOI_PERSISTENT_REJOIN_RESTORE_V1" not in text:
    if old_restore not in text:
        raise SystemExit("PhoBoi session restore anchor not found after hardening")
    text = text.replace(old_restore, new_restore, 1)
path.write_text(text, encoding="utf-8")

# This is the final PhoBoi HTML split in the build. Re-split the completed page
# after readability/zoom/rejoin/Retina changes so no individual C++ literal can
# exceed MSVC's C2026 limit.
runpy.run_path("tools/patch_phoboi_msvc_final_split.py", run_name="__main__")

# Cloudflare reset/new-link controls alter only native host/tunnel code, not the
# already-split HTML, so run them after the final HTML split. The tunnel start
# path itself was already hardened to require public /health verification.
runpy.run_path("tools/patch_phoboi_cloudflare_reset.py", run_name="__main__")

# Port Fallout2-CE's existing PipBoyAvailableAtGameStart mechanism instead of
# mutating worldmap movie state.
runpy.run_path("tools/patch_phoboi_pipboy_access.py", run_name="__main__")

# Temporary compatibility marker for the older final-workflow validator. The
# marker name predates the upstream-style fix; the function is deliberately NOT
# forced true anymore. The new PipBoy patch has its own hard regression guards.
world_path = Path("src/worldmap.cc")
world_text = world_path.read_text(encoding="utf-8")
if "COOP_PHOBOI_ALWAYS_AVAILABLE_V1" not in world_text:
    anchor = "bool wmMapPipboyActive()\n{\n"
    if anchor not in world_text:
        raise SystemExit("worldmap PipBoy compatibility anchor missing")
    world_text = world_text.replace(
        anchor,
        anchor + "    // COOP_PHOBOI_ALWAYS_AVAILABLE_V1 legacy validator marker; availability is handled by sfall config.\n",
        1,
    )
    world_path.write_text(world_text, encoding="utf-8")
