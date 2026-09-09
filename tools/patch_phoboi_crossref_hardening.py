from pathlib import Path
import re

path = Path("src/local_coop_mobile.cc")
text = path.read_text(encoding="utf-8")

# ---------------------------------------------------------------------------
# Reconstruct the embedded page before editing it. Earlier phone patches may
# already have split the C++ raw literal for MSVC. The final rejoin patch runs
# patch_phoboi_msvc_final_split.py after this script, so one temporary raw
# string here is safe and keeps HTML transformations deterministic.
# ---------------------------------------------------------------------------
function_start_token = "const char* mobileControllerHtml()\n{"
next_function_token = "\n\nstd::string mobileReadRequest"
start = text.find(function_start_token)
end = text.find(next_function_token, start)
if start == -1 or end == -1:
    raise SystemExit("PhoBoi controller HTML function boundaries not found")
function_text = text[start:end]
chunks = re.findall(r'R"PHOBOI\((.*?)\)PHOBOI"', function_text, flags=re.S)
if not chunks:
    raise SystemExit("PhoBoi controller HTML chunks not found")
html = "".join(chunks)
if "<!doctype html>" not in html or "</html>" not in html:
    raise SystemExit("PhoBoi controller HTML reconstruction failed")

# ---------------------------------------------------------------------------
# Reserved-character recovery: session PIN admits a phone to the host, while a
# second per-character code proves ownership when browser localStorage/token is
# gone. Automatic token resume remains the fast path and needs no extra entry.
# ---------------------------------------------------------------------------
if "PHOBOI_REJOIN_CODE_UI_V1" not in html:
    old = '<div class="row"><input id="pin" inputmode="numeric" maxlength="6" placeholder="Session PIN"></div>'
    new = old + '\n<!-- PHOBOI_REJOIN_CODE_UI_V1 --><div class="row"><input id="rejoin" inputmode="numeric" maxlength="6" placeholder="Rejoin code (reserved character)"></div>'
    if old not in html:
        raise SystemExit("PhoBoi join PIN row anchor not found")
    html = html.replace(old, new, 1)

    old = "const body=new URLSearchParams({slot:$('slot').value,pin:$('pin').value});"
    new = "const body=new URLSearchParams({slot:$('slot').value,pin:$('pin').value,rejoin:$('rejoin').value}); // PHOBOI_REJOIN_CODE_CLAIM_V1"
    if old not in html:
        raise SystemExit("PhoBoi claim form anchor not found")
    html = html.replace(old, new, 1)

    old = "slot=j.slotIndex;token=j.token;sessionStorage.setItem('phoboiSession',JSON.stringify({slot,token}));"
    new = "slot=j.slotIndex;token=j.token;if(j.rejoinCode)$('rejoin').value=String(j.rejoinCode).padStart(6,'0');sessionStorage.setItem('phoboiSession',JSON.stringify({slot,token}));"
    if old not in html:
        raise SystemExit("PhoBoi claim success anchor not found")
    html = html.replace(old, new, 1)

# ---------------------------------------------------------------------------
# Retina/high-DPI renderer. Incoming RGBA stays at its real stream resolution;
# a source canvas owns those pixels, and the visible canvas gets a backing store
# sized for this phone's devicePixelRatio and local zoom. This prevents Safari
# from repeatedly magnifying a small backing bitmap into a Retina viewport.
# ---------------------------------------------------------------------------
if "PHOBOI_HIDPI_CANVAS_RENDERER_V1" not in html:
    pattern = re.compile(r"async function inflateFrame\(buf\)\{.*?\n\}\nfunction openStream\(\)\{", re.S)
    match = pattern.search(html)
    if match is None:
        raise SystemExit("PhoBoi inflateFrame block not found")
    replacement = r'''// PHOBOI_HIDPI_CANVAS_RENDERER_V1
const phoboiSource=document.createElement('canvas');
const phoboiSourceCtx=phoboiSource.getContext('2d',{alpha:false});
let phoboiFrameReady=false;
function phoboiRenderVisible(){
 if(!phoboiFrameReady)return;
 const c=$('video');
 const cssW=Math.max(1,c.clientWidth||Math.round(c.getBoundingClientRect().width)||1);
 const cssH=Math.max(1,c.clientHeight||Math.round(c.getBoundingClientRect().height)||1);
 const dpr=Math.max(1,Math.min(3,Number(window.devicePixelRatio)||1));
 const localZoom=(typeof phoneViewSteps!=='undefined'&&typeof phoneViewZoom!=='undefined')?phoneViewSteps[phoneViewZoom]:1;
 let scale=dpr*Math.max(1,localZoom||1);
 scale=Math.min(scale,2560/cssW,1440/cssH);
 scale=Math.max(1,scale);
 const outW=Math.max(1,Math.round(cssW*scale)),outH=Math.max(1,Math.round(cssH*scale));
 if(c.width!==outW||c.height!==outH){c.width=outW;c.height=outH}
 const ctx=c.getContext('2d',{alpha:false});
 ctx.imageSmoothingEnabled=true;
 if('imageSmoothingQuality'in ctx)ctx.imageSmoothingQuality='high';
 ctx.fillStyle='#000';ctx.fillRect(0,0,outW,outH);
 const fit=Math.min(outW/phoboiSource.width,outH/phoboiSource.height);
 const dw=Math.max(1,Math.round(phoboiSource.width*fit)),dh=Math.max(1,Math.round(phoboiSource.height*fit));
 const dx=Math.floor((outW-dw)/2),dy=Math.floor((outH-dh)/2);
 ctx.drawImage(phoboiSource,0,0,phoboiSource.width,phoboiSource.height,dx,dy,dw,dh);
}
async function inflateFrame(buf){
 const u=new Uint8Array(buf);if(u.length<12||u[0]!==80||u[1]!==72||u[2]!==79||(u[3]!==66&&u[3]!==82))return;
 const dv=new DataView(buf);const w=dv.getUint16(4,true),h=dv.getUint16(6,true);if(!w||!h||w>1920||h>1080)return;
 const payload=buf.slice(12);let raw;
 if(u[3]===82){raw=payload}else{if(!('DecompressionStream'in window))return;const ds=new DecompressionStream('deflate');raw=await new Response(new Blob([payload]).stream().pipeThrough(ds)).arrayBuffer()}
 const rgba=new Uint8ClampedArray(raw);if(rgba.length!==w*h*4)return;
 if(phoboiSource.width!==w||phoboiSource.height!==h){phoboiSource.width=w;phoboiSource.height=h}
 phoboiSourceCtx.putImageData(new ImageData(rgba,w,h),0,0);phoboiFrameReady=true;phoboiRenderVisible();lastFrameAt=performance.now();videoMode='OK';updateStatus();
}
function openStream(){'''
    html = html[:match.start()] + replacement + html[match.end():]

    # Re-render the existing source image when Safari's viewport settles or the
    # user's local zoom changes, without waiting for another network frame.
    zoom_anchor = "localStorage.setItem('phoboiViewZoom',String(phoneViewZoom));"
    if zoom_anchor in html:
        html = html.replace(zoom_anchor, zoom_anchor + "if(typeof phoboiRenderVisible==='function')requestAnimationFrame(phoboiRenderVisible);", 1)
    else:
        raise SystemExit("PhoBoi per-user zoom persistence anchor not found")

    listener_anchor = "document.addEventListener('visibilitychange',()=>{if(!document.hidden&&slot>=0){openSocket();openStream()}});"
    if listener_anchor not in html:
        raise SystemExit("PhoBoi visibility lifecycle anchor not found")
    hi_dpi_listeners = r'''
// PHOBOI_HIDPI_LAYOUT_REFRESH_V1
window.addEventListener('resize',()=>requestAnimationFrame(phoboiRenderVisible),{passive:true});
window.addEventListener('orientationchange',()=>setTimeout(phoboiRenderVisible,120),{passive:true});
window.visualViewport?.addEventListener('resize',()=>requestAnimationFrame(phoboiRenderVisible),{passive:true});
'''.strip()
    html = html.replace(listener_anchor, listener_anchor + "\n" + hi_dpi_listeners, 1)

# Regular co-op is isometric-only. Do not leave an FPS/ISO control visible on
# the phone when the corresponding gameplay mode is intentionally parked.
if "PHOBOI_REGULAR_ISOMETRIC_PHONE_UI_V1" not in html:
    css = "/* PHOBOI_REGULAR_ISOMETRIC_PHONE_UI_V1 */\n#fps{display:none!important}"
    if "</style>" not in html:
        raise SystemExit("PhoBoi style close missing for FPS hide")
    html = html.replace("</style>", css + "\n</style>", 1)
    html = html.replace(",'fps',7", ",'fps',7")  # harmless compatibility no-op for unusual formatting
    html = html.replace("['fps',7],", "")

# Write the reconstructed HTML function back as one temporary literal. The
# character-rejoin patch invokes the final 6KB MSVC splitter later in the build.
new_function = '''const char* mobileControllerHtml()\n{\n    static const std::string html = R"PHOBOI(''' + html + ''')PHOBOI";\n    return html.c_str();\n}'''
text = text[:start] + new_function + text[end:]

# ---------------------------------------------------------------------------
# Native character recovery code. The saved controllerGuid already belongs to
# the fixed-size co-op save payload, so deriving the code from it keeps old save
# byte layout intact. Session PIN + six-digit character code are both required
# for a new device to take over a locked character.
# ---------------------------------------------------------------------------
if "PHOBOI_CHARACTER_REJOIN_CODE_V1" not in text:
    anchor = '''uint32_t mobileNextToken()\n{\n    static std::atomic<uint32_t> seed { 0x50484F42u };\n    uint32_t value = seed.fetch_add(0x9E3779B9u);\n    value ^= static_cast<uint32_t>(mobileNow());\n    value ^= value << 13;\n    value ^= value >> 17;\n    value ^= value << 5;\n    return value != 0 ? value : 1;\n}\n'''
    if anchor not in text:
        raise SystemExit("PhoBoi token helper anchor not found")
    addition = r'''

// PHOBOI_CHARACTER_REJOIN_CODE_V1
int mobileRejoinCodeForSlot(int slot)
{
    if (slot <= 0 || slot >= kLocalCoopMaxPlayers) {
        return 0;
    }

    const LocalCoopCharacterSlotState& saved = localCoopCharacterStateGetConst().slots[slot];
    const char* identity = saved.controllerGuid;
    if (identity == nullptr || *identity == '\0') {
        identity = gLocalCoopPlayers[slot].controllerGuid;
    }

    char fallback[32] {};
    if (identity == nullptr || *identity == '\0') {
        snprintf(fallback, sizeof(fallback), "PHOBOI-MOBILE-%d", slot + 1);
        identity = fallback;
    }

    uint32_t hash = 2166136261u ^ (0x50484F42u + static_cast<uint32_t>(slot) * 16777619u);
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(identity); *p != 0; ++p) {
        hash ^= *p;
        hash *= 16777619u;
    }
    hash ^= 0x43484152u; // "CHAR"
    hash *= 16777619u;
    return 100000 + static_cast<int>(hash % 900000u);
}
'''
    text = text.replace(anchor, anchor + addition, 1)

# Public health endpoint MUST be handled before player-slot validation.
if "PHOBOI_PUBLIC_HEALTH_ENDPOINT_V1" not in text:
    anchor = '''    if (method == "GET" && route == "/") {\n        mobileSendResponse(client, "200 OK", "text/html; charset=utf-8", mobileControllerHtml());\n        return;\n    }\n\n    int slot = mobileValue(values, "slot", -1);'''
    replacement = '''    if (method == "GET" && route == "/") {\n        mobileSendResponse(client, "200 OK", "text/html; charset=utf-8", mobileControllerHtml());\n        return;\n    }\n\n    // PHOBOI_PUBLIC_HEALTH_ENDPOINT_V1\n    // Cloudflare READY is based on reaching this exact native host endpoint\n    // through the public HTTPS URL, not merely on cloudflared printing a name.\n    if (method == "GET" && route == "/health") {\n        mobileSendResponse(client, "200 OK", "text/plain; charset=utf-8", "PHOBOI_OK_V1");\n        return;\n    }\n\n    int slot = mobileValue(values, "slot", -1);'''
    if anchor not in text:
        raise SystemExit("PhoBoi root route/slot validation anchor not found")
    text = text.replace(anchor, replacement, 1)

# Protect already-created character slots from a fresh device that knows only
# the global host PIN. Token resume remains unaffected and bypasses /claim.
if "PHOBOI_RESERVED_CHARACTER_CLAIM_GUARD_V1" not in text:
    anchor = '''        if (pin != gMobilePin) {\n            mobileSendResponse(client, "403 Forbidden", "application/json", "{\\\"ok\\\":false,\\\"error\\\":\\\"Wrong session PIN\\\"}");\n            return;\n        }\n        bool expected = false;'''
    replacement = '''        if (pin != gMobilePin) {\n            mobileSendResponse(client, "403 Forbidden", "application/json", "{\\\"ok\\\":false,\\\"error\\\":\\\"Wrong session PIN\\\"}");\n            return;\n        }\n\n        // PHOBOI_RESERVED_CHARACTER_CLAIM_GUARD_V1\n        // The host/save owns a character slot. A new browser/device must prove\n        // which reserved character it is reclaiming in addition to session PIN.\n        if (gLocalCoopPlayers[slot].slotLocked) {\n            int suppliedRejoin = mobileValue(values, "rejoin", -1);\n            int expectedRejoin = mobileRejoinCodeForSlot(slot);\n            if (suppliedRejoin != expectedRejoin) {\n                mobileSendResponse(client, "403 Forbidden", "application/json", "{\\\"ok\\\":false,\\\"error\\\":\\\"Reserved character: enter the REJOIN code shown on the host\\\"}");\n                return;\n            }\n        }\n        bool expected = false;'''
    if anchor not in text:
        raise SystemExit("PhoBoi claim PIN anchor not found")
    text = text.replace(anchor, replacement, 1)

    old = '''        body << "{\\\"ok\\\":true,\\\"slotIndex\\\":" << slot\n             << ",\\\"player\\\":" << slot + 1\n             << ",\\\"token\\\":" << token << "}";'''
    new = '''        body << "{\\\"ok\\\":true,\\\"slotIndex\\\":" << slot\n             << ",\\\"player\\\":" << slot + 1\n             << ",\\\"token\\\":" << token\n             << ",\\\"rejoinCode\\\":" << mobileRejoinCodeForSlot(slot) << "}";'''
    if old not in text:
        raise SystemExit("PhoBoi successful claim JSON anchor not found")
    text = text.replace(old, new, 1)

# Host window shows the recovery code only for an already-created character.
if "PHOBOI_HOST_REJOIN_CODE_DISPLAY_V1" not in text:
    old = '''        snprintf(line, sizeof(line), "PLAYER %d: %s", slot + 1, status);\n        windowDrawText(gMobileHostWindow, line, 340, 20, 150 + (slot - 1) * 34, _colorTable[992]);'''
    new = '''        // PHOBOI_HOST_REJOIN_CODE_DISPLAY_V1\n        if (gLocalCoopPlayers[slot].slotLocked) {\n            snprintf(line, sizeof(line), "PLAYER %d: %s  REJOIN %06d",\n                slot + 1, status, mobileRejoinCodeForSlot(slot));\n        } else {\n            snprintf(line, sizeof(line), "PLAYER %d: %s", slot + 1, status);\n        }\n        windowDrawText(gMobileHostWindow, line, 360, 20, 150 + (slot - 1) * 34, _colorTable[992]);'''
    if old not in text:
        raise SystemExit("PhoBoi host player status anchor not found")
    text = text.replace(old, new, 1)

# ---------------------------------------------------------------------------
# Cloudflare READY verification using WinHTTP. Quick Tunnel hostnames are
# ephemeral; seeing one in cloudflared output does not prove a remote player can
# reach PhoBoi. Verify /health over public HTTPS before publishing URL/QR.
# ---------------------------------------------------------------------------
if "PHOBOI_CLOUDFLARE_PUBLIC_VERIFY_V1" not in text:
    include_anchor = '#include <windows.h>\n'
    if include_anchor not in text:
        raise SystemExit("Windows include anchor not found")
    text = text.replace(include_anchor, include_anchor + '#include <winhttp.h>\n', 1)

    anchor = '''bool mobileFindCloudflareUrl(const std::string& output, std::string* url)\n{\n    size_t start = output.find("https://");\n    while (start != std::string::npos) {\n        size_t end = output.find(".trycloudflare.com", start);\n        if (end != std::string::npos) {\n            end += std::strlen(".trycloudflare.com");\n            std::string candidate = output.substr(start, end - start);\n            if (candidate.size() < 256) {\n                *url = candidate;\n                return true;\n            }\n        }\n        start = output.find("https://", start + 8);\n    }\n    return false;\n}\n'''
    if anchor not in text:
        raise SystemExit("Cloudflare URL parser anchor not found")
    addition = r'''

// PHOBOI_CLOUDFLARE_PUBLIC_VERIFY_V1
bool mobileVerifyCloudflarePublicHealth(const std::string& publicUrl)
{
    constexpr const char* prefix = "https://";
    if (publicUrl.rfind(prefix, 0) != 0) {
        return false;
    }
    std::string host = publicUrl.substr(std::strlen(prefix));
    size_t slash = host.find('/');
    if (slash != std::string::npos) {
        host.resize(slash);
    }
    if (host.empty()) {
        return false;
    }

    int wideLength = MultiByteToWideChar(CP_UTF8, 0, host.c_str(), -1, nullptr, 0);
    if (wideLength <= 1) {
        return false;
    }
    std::wstring wideHost(static_cast<size_t>(wideLength), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, host.c_str(), -1, wideHost.data(), wideLength);

    HINTERNET session = WinHttpOpen(
        L"PhoBoi-Coop/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (session == nullptr) {
        return false;
    }
    WinHttpSetTimeouts(session, 2500, 2500, 2500, 3500);

    HINTERNET connection = WinHttpConnect(session, wideHost.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET request = nullptr;
    bool ok = false;
    if (connection != nullptr) {
        request = WinHttpOpenRequest(
            connection,
            L"GET",
            L"/health",
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE | WINHTTP_FLAG_REFRESH);
    }
    if (request != nullptr
        && WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
        && WinHttpReceiveResponse(request, nullptr)) {
        DWORD status = 0;
        DWORD statusSize = sizeof(status);
        if (WinHttpQueryHeaders(
                request,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX,
                &status,
                &statusSize,
                WINHTTP_NO_HEADER_INDEX)
            && status == 200) {
            std::string body;
            char buffer[256];
            DWORD count = 0;
            while (body.size() < 1024
                && WinHttpReadData(request, buffer, sizeof(buffer), &count)
                && count != 0) {
                body.append(buffer, static_cast<size_t>(count));
            }
            ok = body.find("PHOBOI_OK_V1") != std::string::npos;
        }
    }

    if (request != nullptr) WinHttpCloseHandle(request);
    if (connection != nullptr) WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return ok;
}
'''
    text = text.replace(anchor, anchor + addition, 1)

    old = '''    gCloudflareOutputThread = std::thread([outputRead]() {\n        std::string output;\n        char buffer[2048];\n        DWORD count = 0;\n        while (ReadFile(outputRead, buffer, sizeof(buffer), &count, nullptr) && count != 0) {\n            output.append(buffer, static_cast<size_t>(count));\n            if (output.size() > 32768) {\n                output.erase(0, output.size() - 16384);\n            }\n            std::string publicUrl;\n            if (mobileFindCloudflareUrl(output, &publicUrl)) {\n                std::lock_guard<std::mutex> lock(gCloudflareStateMutex);\n                gCloudflarePublicUrl = publicUrl;\n                gCloudflareStatus = "ONLINE";\n            }\n        }'''
    new = '''    gCloudflareOutputThread = std::thread([outputRead]() {\n        std::string output;\n        std::string checkedCandidate;\n        char buffer[2048];\n        DWORD count = 0;\n        while (ReadFile(outputRead, buffer, sizeof(buffer), &count, nullptr) && count != 0) {\n            output.append(buffer, static_cast<size_t>(count));\n            if (output.size() > 32768) {\n                output.erase(0, output.size() - 16384);\n            }\n            std::string publicUrl;\n            if (mobileFindCloudflareUrl(output, &publicUrl) && publicUrl != checkedCandidate) {\n                checkedCandidate = publicUrl;\n                {\n                    std::lock_guard<std::mutex> lock(gCloudflareStateMutex);\n                    gCloudflarePublicUrl.clear();\n                    gCloudflareStatus = "VERIFYING PUBLIC LINK";\n                }\n\n                // PHOBOI_CLOUDFLARE_READY_ONLY_QR_V1\n                bool reachable = false;\n                for (int attempt = 0; attempt < 10 && !reachable; attempt++) {\n                    reachable = mobileVerifyCloudflarePublicHealth(publicUrl);\n                    if (!reachable) Sleep(500);\n                }\n                {\n                    std::lock_guard<std::mutex> lock(gCloudflareStateMutex);\n                    if (reachable) {\n                        gCloudflarePublicUrl = publicUrl;\n                        gCloudflareStatus = "READY";\n                    } else {\n                        gCloudflarePublicUrl.clear();\n                        gCloudflareStatus = "UNREACHABLE - PRESS R";\n                    }\n                }\n                debugPrint("[PHOBOI MOBILE] public tunnel verification url=%s reachable=%d\\n",\n                    publicUrl.c_str(), reachable ? 1 : 0);\n            }\n        }'''
    if old not in text:
        raise SystemExit("Cloudflare output thread ONLINE anchor not found")
    text = text.replace(old, new, 1)

path.write_text(text, encoding="utf-8")

# WinHTTP is a Windows system library; wire it only for the Windows target.
cmake = Path("CMakeLists.txt")
cmake_text = cmake.read_text(encoding="utf-8")
if "PHOBOI_WINHTTP_PUBLIC_VERIFY_V1" not in cmake_text:
    old = '''    target_link_libraries(${EXECUTABLE_NAME}\n        winmm\n        ws2_32\n    )'''
    new = '''    # PHOBOI_WINHTTP_PUBLIC_VERIFY_V1\n    target_link_libraries(${EXECUTABLE_NAME}\n        winmm\n        ws2_32\n        winhttp\n    )'''
    if old not in cmake_text:
        raise SystemExit("Windows system library link anchor not found")
    cmake_text = cmake_text.replace(old, new, 1)
    cmake.write_text(cmake_text, encoding="utf-8")

# Hard regression guards.
source = path.read_text(encoding="utf-8")
for marker in (
    "PHOBOI_REJOIN_CODE_UI_V1",
    "PHOBOI_HIDPI_CANVAS_RENDERER_V1",
    "PHOBOI_HIDPI_LAYOUT_REFRESH_V1",
    "PHOBOI_REGULAR_ISOMETRIC_PHONE_UI_V1",
    "PHOBOI_CHARACTER_REJOIN_CODE_V1",
    "PHOBOI_PUBLIC_HEALTH_ENDPOINT_V1",
    "PHOBOI_RESERVED_CHARACTER_CLAIM_GUARD_V1",
    "PHOBOI_HOST_REJOIN_CODE_DISPLAY_V1",
    "PHOBOI_CLOUDFLARE_PUBLIC_VERIFY_V1",
    "PHOBOI_CLOUDFLARE_READY_ONLY_QR_V1",
):
    if marker not in source:
        raise SystemExit(f"missing PhoBoi cross-reference hardening marker {marker}")
if "PHOBOI_WINHTTP_PUBLIC_VERIFY_V1" not in cmake.read_text(encoding="utf-8"):
    raise SystemExit("missing WinHTTP build marker")

print("Applied high-DPI phone rendering, reserved-character recovery, and verified Cloudflare READY")
