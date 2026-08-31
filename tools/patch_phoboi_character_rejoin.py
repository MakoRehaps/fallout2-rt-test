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

# Persist the phone-side resume credential across Safari tab/app restarts. The
# character remains identified by its numbered co-op slot; this token only
# proves that this phone may resume the transport without claiming a new body.
old_store = "sessionStorage.setItem('phoboiSession',JSON.stringify({slot,token}));"
new_store = "localStorage.setItem('phoboiSession',JSON.stringify({slot,token}));sessionStorage.setItem('phoboiSession',JSON.stringify({slot,token})); // PHOBOI_PERSISTENT_REJOIN_TOKEN_V1"
if "PHOBOI_PERSISTENT_REJOIN_TOKEN_V1" not in text:
    if old_store not in text:
        raise SystemExit("PhoBoi session storage anchor not found")
    text = text.replace(old_store, new_store, 1)

old_restore = "const saved=JSON.parse(sessionStorage.getItem('phoboiSession')||'null');if(!saved)return;"
new_restore = "const saved=JSON.parse(localStorage.getItem('phoboiSession')||sessionStorage.getItem('phoboiSession')||'null');if(!saved)return; // PHOBOI_PERSISTENT_REJOIN_RESTORE_V1"
if "PHOBOI_PERSISTENT_REJOIN_RESTORE_V1" not in text:
    if old_restore not in text:
        raise SystemExit("PhoBoi session restore anchor not found")
    text = text.replace(old_restore, new_restore, 1)

# Explicit release/kick intentionally remains the escape hatch: it clears the
# token so another controller can take over the same saved character slot.
path.write_text(text, encoding="utf-8")
print("Reserved co-op character slots across phone/network disconnects with automatic rejoin")

# This patch is the last PhoBoi HTML mutator in the final build. Re-split the
# completed controller page now, after readability/zoom/rejoin additions, so no
# individual C++ string literal can exceed MSVC's C2026 limit.
runpy.run_path("tools/patch_phoboi_msvc_final_split.py", run_name="__main__")
