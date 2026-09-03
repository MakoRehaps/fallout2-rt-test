from pathlib import Path
import re

path = Path("src/local_coop_mobile.cc")
text = path.read_text(encoding="utf-8")

marker = "PHOBOI_CLOUDFLARE_ROUTE_V5"
if marker in text:
    print("PhoBoi Cloudflare route V5 already applied")
    raise SystemExit(0)

for required in (
    "PHOBOI_CLOUDFLARE_ROUTE_V4",
    "PHOBOI_CLOUDFLARE_PUBLIC_VERIFY_V1",
):
    if required not in text:
        raise SystemExit(f"Cloudflare V5 requires {required}")

# ---------------------------------------------------------------------------
# V4 tried to isolate Quick Tunnel from a possible user config by passing an
# intentionally empty --config file. cloudflared 2026.8.3 explicitly reports
# that as an ERR, while Cloudflare's Quick Tunnel docs say no config is needed.
# The user's runtime log also proved that no default config.yml/config.yaml was
# present, so use the normal zero-config Quick Tunnel path directly.
#
# cloudflared 2026.4 changed edge IP selection from IPv4 to auto. The failing
# host selected an IPv6 SEA edge despite the game origin and VPN being IPv4.
# Pin the edge to IPv4. Keep HTTP/2 for this controller build because PhoBoi
# relies on WebSocket upgrades and it avoids the current QUIC WebSocket path.
# ---------------------------------------------------------------------------
old_command = r'''    // PHOBOI_CLOUDFLARE_AUTO_PROTOCOL_V4
    std::string quickConfig = mobileCloudflaredQuickConfigPath();
    std::string commandText = "\\\"" + executable + "\\\" tunnel --no-autoupdate --protocol auto --loglevel info";
    if (!quickConfig.empty()) {
        commandText += " --config \\\"" + quickConfig + "\\\"";
    }
    commandText += " --url http://127.0.0.1:27888";
    debugPrint("[PHOBOI MOBILE] Cloudflare V4 transport=auto isolated_config=%d\\n", quickConfig.empty() ? 0 : 1);'''
new_command = r'''    // PHOBOI_CLOUDFLARE_ROUTE_V5
    // PHOBOI_CLOUDFLARE_CANONICAL_QUICK_V5
    // PHOBOI_CLOUDFLARE_IPV4_HTTP2_V5
    std::string commandText = "\\\"" + executable
        + "\\\" tunnel --edge-ip-version 4 --protocol http2 --no-autoupdate --loglevel info --url http://127.0.0.1:27888";
    debugPrint("[PHOBOI MOBILE] Cloudflare V5 zero-config quick tunnel edge_ip=4 protocol=http2 origin=http://127.0.0.1:27888\\n");'''
if old_command not in text:
    raise SystemExit("Cloudflare V4 command block not found")
text = text.replace(old_command, new_command, 1)

# ---------------------------------------------------------------------------
# Replace the opaque bool-only WinHTTP verifier with a diagnostic verifier.
# Until now every DNS/TLS/HTTP failure collapsed to self_probe=0, which made
# real device failures impossible to distinguish. Keep the same boolean API,
# but report the actual WinHTTP stage/error or HTTP status at a bounded rate.
# ---------------------------------------------------------------------------
pattern = re.compile(
    r'// PHOBOI_CLOUDFLARE_PUBLIC_VERIFY_V1\nbool mobileVerifyCloudflarePublicHealth\(const std::string& publicUrl\)\n\{.*?^\}\n',
    re.S | re.M,
)
match = pattern.search(text)
if match is None:
    raise SystemExit("Cloudflare public verifier function not found")

new_verifier = r'''// PHOBOI_CLOUDFLARE_PUBLIC_VERIFY_V1
// PHOBOI_CLOUDFLARE_PROBE_DIAGNOSTICS_V5
bool mobileVerifyCloudflarePublicHealth(const std::string& publicUrl)
{
    static std::atomic<unsigned int> probeSequence { 0 };
    unsigned int sequence = probeSequence.fetch_add(1) + 1;
    bool verbose = sequence <= 3 || sequence % 10 == 0;

    constexpr const char* prefix = "https://";
    if (publicUrl.rfind(prefix, 0) != 0) {
        if (verbose) debugPrint("[PHOBOI PUBLIC PROBE] invalid url=%s\n", publicUrl.c_str());
        return false;
    }

    std::string host = publicUrl.substr(std::strlen(prefix));
    size_t slash = host.find('/');
    if (slash != std::string::npos) {
        host.resize(slash);
    }
    if (host.empty()) {
        if (verbose) debugPrint("[PHOBOI PUBLIC PROBE] empty host url=%s\n", publicUrl.c_str());
        return false;
    }

    int wideLength = MultiByteToWideChar(CP_UTF8, 0, host.c_str(), -1, nullptr, 0);
    if (wideLength <= 1) {
        if (verbose) debugPrint("[PHOBOI PUBLIC PROBE] utf8 conversion failed host=%s winerr=%lu\n", host.c_str(), GetLastError());
        return false;
    }
    std::wstring wideHost(static_cast<size_t>(wideLength), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, host.c_str(), -1, wideHost.data(), wideLength);

    HINTERNET session = WinHttpOpen(
        L"PhoBoi-Coop/1.0",
        WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (session == nullptr) {
        if (verbose) debugPrint("[PHOBOI PUBLIC PROBE] WinHttpOpen failed host=%s winerr=%lu\n", host.c_str(), GetLastError());
        return false;
    }
    WinHttpSetTimeouts(session, 1500, 1500, 2500, 2500);

    HINTERNET connection = WinHttpConnect(session, wideHost.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET request = nullptr;
    bool ok = false;
    if (connection == nullptr) {
        if (verbose) debugPrint("[PHOBOI PUBLIC PROBE] WinHttpConnect failed host=%s winerr=%lu\n", host.c_str(), GetLastError());
    } else {
        request = WinHttpOpenRequest(
            connection,
            L"GET",
            L"/health",
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE | WINHTTP_FLAG_REFRESH);
    }

    if (request == nullptr && connection != nullptr) {
        if (verbose) debugPrint("[PHOBOI PUBLIC PROBE] WinHttpOpenRequest failed host=%s winerr=%lu\n", host.c_str(), GetLastError());
    }

    if (request != nullptr) {
        BOOL sent = WinHttpSendRequest(
            request,
            WINHTTP_NO_ADDITIONAL_HEADERS,
            0,
            WINHTTP_NO_REQUEST_DATA,
            0,
            0,
            0);
        if (!sent) {
            if (verbose) debugPrint("[PHOBOI PUBLIC PROBE] send failed host=%s winerr=%lu\n", host.c_str(), GetLastError());
        } else if (!WinHttpReceiveResponse(request, nullptr)) {
            if (verbose) debugPrint("[PHOBOI PUBLIC PROBE] receive failed host=%s winerr=%lu\n", host.c_str(), GetLastError());
        } else {
            DWORD status = 0;
            DWORD statusSize = sizeof(status);
            if (!WinHttpQueryHeaders(
                    request,
                    WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                    WINHTTP_HEADER_NAME_BY_INDEX,
                    &status,
                    &statusSize,
                    WINHTTP_NO_HEADER_INDEX)) {
                if (verbose) debugPrint("[PHOBOI PUBLIC PROBE] status query failed host=%s winerr=%lu\n", host.c_str(), GetLastError());
            } else if (status != 200) {
                if (verbose) debugPrint("[PHOBOI PUBLIC PROBE] HTTP status=%lu host=%s\n", status, host.c_str());
            } else {
                std::string body;
                char buffer[256];
                DWORD count = 0;
                while (body.size() < 1024
                    && WinHttpReadData(request, buffer, sizeof(buffer), &count)
                    && count != 0) {
                    body.append(buffer, static_cast<size_t>(count));
                }
                ok = body.find("PHOBOI_OK_V1") != std::string::npos;
                if (!ok && verbose) {
                    debugPrint("[PHOBOI PUBLIC PROBE] HTTP 200 but health marker missing host=%s bytes=%zu\n", host.c_str(), body.size());
                }
            }
        }
    }

    if (request != nullptr) WinHttpCloseHandle(request);
    if (connection != nullptr) WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return ok;
}
'''
text = text[:match.start()] + new_verifier + text[match.end():]

for required in (
    "PHOBOI_CLOUDFLARE_ROUTE_V5",
    "PHOBOI_CLOUDFLARE_CANONICAL_QUICK_V5",
    "PHOBOI_CLOUDFLARE_IPV4_HTTP2_V5",
    "PHOBOI_CLOUDFLARE_PROBE_DIAGNOSTICS_V5",
):
    if required not in text:
        raise SystemExit(f"missing Cloudflare V5 marker {required}")

# The V5 command must not pass an empty config and must not leave edge IP auto.
command_window = text[text.find("PHOBOI_CLOUDFLARE_CANONICAL_QUICK_V5"):text.find("PHOBOI_CLOUDFLARE_CANONICAL_QUICK_V5") + 900]
if "--config" in command_window:
    raise SystemExit("Cloudflare V5 command still passes --config")
if "--edge-ip-version 4" not in command_window:
    raise SystemExit("Cloudflare V5 command is not IPv4-pinned")
if "--protocol http2" not in command_window:
    raise SystemExit("Cloudflare V5 command is not HTTP2-pinned")

path.write_text(text, encoding="utf-8")
print("Applied Cloudflare V5 zero-config IPv4/HTTP2 Quick Tunnel path with detailed public-probe diagnostics")
