from pathlib import Path

path = Path("src/local_coop_mobile.cc")
text = path.read_text(encoding="utf-8")

marker = "PHOBOI_CLOUDFLARE_ROUTE_V3"
if marker in text:
    print("PhoBoi Cloudflare route V3 already applied")
    raise SystemExit(0)

# ---------------------------------------------------------------------------
# HTTP responses must be fully written. send() is allowed to return a partial
# write, especially when the receiver is a reverse proxy and the controller
# page has grown large. Local loopback tests can hide this bug.
# ---------------------------------------------------------------------------
old_send = r'''#ifdef _WIN32
    send(socket, bytes.c_str(), static_cast<int>(bytes.size()), 0);
#else
    send(socket, bytes.c_str(), bytes.size(), 0);
#endif
}'''
new_send = r'''    // PHOBOI_HTTP_SEND_ALL_V3
    size_t sent = 0;
    while (sent < bytes.size()) {
#ifdef _WIN32
        int count = send(socket, bytes.data() + sent, static_cast<int>(bytes.size() - sent), 0);
#else
        int count = static_cast<int>(send(socket, bytes.data() + sent, bytes.size() - sent, 0));
#endif
        if (count <= 0) {
            debugPrint("[PHOBOI HTTP] response send failed after %zu/%zu bytes\n", sent, bytes.size());
            break;
        }
        sent += static_cast<size_t>(count);
    }
}'''
if old_send not in text:
    raise SystemExit("PhoBoi HTTP response send anchor not found")
text = text.replace(old_send, new_send, 1)

# ---------------------------------------------------------------------------
# Verify the exact localhost origin that cloudflared will proxy to before we
# launch a public tunnel. This separates LOCAL ORIGIN failures from edge-route
# failures and prevents us from blaming Cloudflare for a dead listener.
# ---------------------------------------------------------------------------
start_anchor = "bool mobileStartCloudflareTunnel()\n{\n"
if start_anchor not in text:
    raise SystemExit("Cloudflare start function anchor not found")
local_helper = r'''// PHOBOI_LOCAL_ORIGIN_PREFLIGHT_V3
bool mobileVerifyLocalOriginHealth()
{
    HINTERNET session = WinHttpOpen(
        L"PhoBoi-Origin-Preflight/1.0",
        WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (session == nullptr) {
        return false;
    }
    WinHttpSetTimeouts(session, 750, 750, 1000, 1000);

    HINTERNET connection = WinHttpConnect(session, L"127.0.0.1", kMobilePort, 0);
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
            WINHTTP_FLAG_REFRESH);
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
            char buffer[128];
            DWORD count = 0;
            while (body.size() < 512
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
text = text.replace(start_anchor, local_helper + start_anchor, 1)

start_body_old = '''bool mobileStartCloudflareTunnel()\n{\n    if (gCloudflareProcess != nullptr) {'''
start_body_new = '''bool mobileStartCloudflareTunnel()\n{\n    // PHOBOI_CLOUDFLARE_ROUTE_V3\n    mobileSetCloudflareStatus("CHECKING LOCAL ORIGIN");\n    if (!mobileVerifyLocalOriginHealth()) {\n        mobileSetCloudflareStatus("LOCAL ORIGIN FAILED");\n        debugPrint("[PHOBOI MOBILE] Cloudflare V3 local origin preflight FAILED http://127.0.0.1:%d/health\\n", kMobilePort);\n        return false;\n    }\n    debugPrint("[PHOBOI MOBILE] Cloudflare V3 local origin preflight OK http://127.0.0.1:%d/health\\n", kMobilePort);\n\n    if (gCloudflareProcess != nullptr) {'''
if start_body_old not in text:
    raise SystemExit("Cloudflare start body anchor not found")
text = text.replace(start_body_old, start_body_new, 1)

# Keep Cloudflare on TCP/HTTP2 on this Windows controller build. VPN/virtual
# adapters are common for this project and UDP/QUIC can be unstable even after
# registration. Cloudflare documents HTTP2 as the supported TCP fallback.
old_command = '    std::string commandText = "\\\"" + executable + "\\\" tunnel --no-autoupdate --url http://127.0.0.1:27888";'
new_command = '    std::string commandText = "\\\"" + executable + "\\\" tunnel --no-autoupdate --protocol http2 --loglevel info --url http://127.0.0.1:27888"; // PHOBOI_CLOUDFLARE_HTTP2_V3'
if old_command not in text:
    raise SystemExit("Cloudflare command anchor not found")
text = text.replace(old_command, new_command, 1)

# Surface cloudflared warnings/errors into the normal Fallout debug log. The
# self-probe itself creates a public request, so a 502/origin dial failure will
# now leave the exact cloudflared reason in the user's paste instead of only 0.
output_anchor = '''            output.append(buffer, static_cast<size_t>(count));\n            if (output.size() > 32768) {'''
output_replacement = '''            output.append(buffer, static_cast<size_t>(count));\n            // PHOBOI_CLOUDFLARE_ERROR_LOG_V3\n            std::string cloudflareChunk(buffer, static_cast<size_t>(count));\n            if (cloudflareChunk.find("ERR") != std::string::npos\n                || cloudflareChunk.find("WRN") != std::string::npos\n                || cloudflareChunk.find("502") != std::string::npos\n                || cloudflareChunk.find("connection refused") != std::string::npos) {\n                debugPrint("[PHOBOI CLOUDFLARED] %s\\n", cloudflareChunk.c_str());\n            }\n            if (output.size() > 32768) {'''
if output_anchor not in text:
    raise SystemExit("Cloudflare output append anchor not found")
text = text.replace(output_anchor, output_replacement, 1)

# ---------------------------------------------------------------------------
# Never display/encode the VPN/LAN URL while a Cloudflare tunnel is being
# requested. The current machine resolves its first non-loopback IPv4 to a 26.x
# virtual-network address, which a normal iPhone cannot reach.
# ---------------------------------------------------------------------------
pair_old = '''#ifdef _WIN32\n    {\n        std::lock_guard<std::mutex> lock(gCloudflareStateMutex);\n        if (!gCloudflarePublicUrl.empty()) {\n            url << gCloudflarePublicUrl << "/?pin=" << gMobilePin;\n            return url.str();\n        }\n    }\n#endif\n    url << "http://" << mobileHostAddress() << ":" << kMobilePort << "/?pin=" << gMobilePin;'''
pair_new = '''#ifdef _WIN32\n    {\n        std::lock_guard<std::mutex> lock(gCloudflareStateMutex);\n        if (!gCloudflarePublicUrl.empty()) {\n            url << gCloudflarePublicUrl << "/?pin=" << gMobilePin;\n            return url.str();\n        }\n        // PHOBOI_NO_VPN_QR_DURING_CLOUDFLARE_V3\n        if (gCloudflareStatus != "OFF") {\n            return "";\n        }\n    }\n#endif\n    url << "http://" << mobileHostAddress() << ":" << kMobilePort << "/?pin=" << gMobilePin;'''
if pair_old not in text:
    raise SystemExit("PhoBoi pairing URL Cloudflare fallback anchor not found")
text = text.replace(pair_old, pair_new, 1)

host_old = '''    snprintf(line, sizeof(line), "OPEN OR SCAN: %s", address.c_str());\n    windowDrawText(gMobileHostWindow, line, 360, 20, 54, _colorTable[992]);'''
host_new = '''    // PHOBOI_PUBLIC_QR_WAIT_V3\n    if (address.empty()) {\n        snprintf(line, sizeof(line), "OPEN OR SCAN: WAITING FOR PUBLIC CLOUDFLARE LINK");\n    } else {\n        snprintf(line, sizeof(line), "OPEN OR SCAN: %s", address.c_str());\n    }\n    windowDrawText(gMobileHostWindow, line, 360, 20, 54, _colorTable[992]);'''
if host_old not in text:
    raise SystemExit("PhoBoi host address display anchor not found")
text = text.replace(host_old, host_new, 1)

qr_old = '    mobileDrawQrCode(gMobileHostWindow, address, 395, 145);'
qr_new = '''    if (!address.empty()) {\n        mobileDrawQrCode(gMobileHostWindow, address, 395, 145);\n    }'''
if qr_old not in text:
    raise SystemExit("PhoBoi QR draw anchor not found")
text = text.replace(qr_old, qr_new, 1)

copy_old = '''        std::string pairingUrl = mobilePairingUrl();\n        SDL_SetClipboardText(pairingUrl.c_str());'''
copy_new = '''        std::string pairingUrl = mobilePairingUrl();\n        if (!pairingUrl.empty()) {\n            SDL_SetClipboardText(pairingUrl.c_str());\n        }'''
if copy_old not in text:
    raise SystemExit("PhoBoi copy-link anchor not found")
text = text.replace(copy_old, copy_new, 1)

for required in (
    "PHOBOI_CLOUDFLARE_ROUTE_V3",
    "PHOBOI_HTTP_SEND_ALL_V3",
    "PHOBOI_LOCAL_ORIGIN_PREFLIGHT_V3",
    "PHOBOI_CLOUDFLARE_HTTP2_V3",
    "PHOBOI_CLOUDFLARE_ERROR_LOG_V3",
    "PHOBOI_NO_VPN_QR_DURING_CLOUDFLARE_V3",
    "PHOBOI_PUBLIC_QR_WAIT_V3",
):
    if required not in text:
        raise SystemExit(f"missing Cloudflare route V3 marker {required}")

path.write_text(text, encoding="utf-8")
print("Applied Cloudflare V3 origin preflight, HTTP2 transport, send-all responses, diagnostics, and safe QR gating")
