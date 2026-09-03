from pathlib import Path
import re

path = Path("src/local_coop_mobile.cc")
text = path.read_text(encoding="utf-8")

marker = "PHOBOI_PUBLIC_FAILOVER_V6"
if marker in text:
    print("PhoBoi public failover V6 already applied")
    raise SystemExit(0)

for required in (
    "PHOBOI_CLOUDFLARE_ROUTE_V5",
    "PHOBOI_CLOUDFLARE_PROBE_DIAGNOSTICS_V5",
    "PHOBOI_CLOUDFLARE_RESET_V1",
    "PHOBOI_NO_VPN_QR_DURING_CLOUDFLARE_V3",
):
    if required not in text:
        raise SystemExit(f"PhoBoi public failover V6 requires {required}")

# ---------------------------------------------------------------------------
# Runtime state for a second public tunnel. Cloudflare Quick Tunnel remains the
# first choice. The alternate relay is only started after Windows proves that
# the assigned trycloudflare.com hostname cannot be resolved (WinHTTP 12007).
# ---------------------------------------------------------------------------
state_anchor = '''std::string gCloudflarePublicUrl;\nstd::string gCloudflareStatus = "OFF";\n'''
state_replacement = '''std::string gCloudflarePublicUrl;\nstd::string gCloudflareStatus = "OFF";\n// PHOBOI_PUBLIC_FAILOVER_V6\nHANDLE gPhoBoiFallbackProcess = nullptr;\nHANDLE gPhoBoiFallbackOutput = nullptr;\nstd::thread gPhoBoiFallbackOutputThread;\nstd::atomic<bool> gPhoBoiFallbackStarting { false };\nstd::atomic<DWORD> gPhoBoiPublicProbeLastError { ERROR_SUCCESS };\n'''
if state_anchor not in text:
    raise SystemExit("PhoBoi Cloudflare state anchor missing")
text = text.replace(state_anchor, state_replacement, 1)

# The verifier is earlier in the file than the alternate tunnel helpers.
# Declare the DNS-failure switch before the verifier so it can trigger the
# failover without moving large existing functions around.
forward_anchor = '''void mobileResetInput(MobileSlotState& state);\n'''
forward_replacement = '''void mobileResetInput(MobileSlotState& state);\n#ifdef _WIN32\nvoid mobileStartPublicDnsFallback();\n#endif\n'''
if forward_anchor not in text:
    raise SystemExit("PhoBoi reset-input declaration anchor missing")
text = text.replace(forward_anchor, forward_replacement, 1)

# ---------------------------------------------------------------------------
# Prefer a physical RFC1918 LAN address over 26.x VPN/virtual adapters. The
# user's phone is on Wi-Fi while the old helper selected 26.214.x.x, which a
# normal iPhone cannot route to. Keep a lower-scored non-loopback fallback for
# unusual networks.
# ---------------------------------------------------------------------------
host_pattern = re.compile(
    r'std::string mobileHostAddress\(\)\n\{.*?^\}\n\n(?=std::string mobilePairingUrl\(\))',
    re.S | re.M,
)
host_match = host_pattern.search(text)
if host_match is None:
    raise SystemExit("mobileHostAddress function block not found")

host_replacement = r'''// PHOBOI_LAN_ADDRESS_PREFERENCE_V6
std::string mobileHostAddress()
{
    char hostname[256] {};
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        return "127.0.0.1";
    }

    addrinfo hints {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* result = nullptr;
    if (getaddrinfo(hostname, nullptr, &hints, &result) != 0) {
        return "127.0.0.1";
    }

    std::string bestAddress = "127.0.0.1";
    int bestScore = -1;
    for (addrinfo* current = result; current != nullptr; current = current->ai_next) {
        if (current->ai_family != AF_INET || current->ai_addr == nullptr) {
            continue;
        }

        auto* ipv4 = reinterpret_cast<sockaddr_in*>(current->ai_addr);
        char buffer[INET_ADDRSTRLEN] {};
        if (inet_ntop(AF_INET, &ipv4->sin_addr, buffer, sizeof(buffer)) == nullptr) {
            continue;
        }

        std::string candidate(buffer);
        if (candidate == "127.0.0.1" || candidate.rfind("127.", 0) == 0) {
            continue;
        }

        int score = 100;
        if (candidate.rfind("192.168.", 0) == 0) {
            score = 500;
        } else if (candidate.rfind("10.", 0) == 0) {
            score = 450;
        } else if (candidate.rfind("172.", 0) == 0) {
            size_t dot = candidate.find('.', 4);
            int second = dot == std::string::npos ? -1 : std::atoi(candidate.substr(4, dot - 4).c_str());
            if (second >= 16 && second <= 31) {
                score = 425;
            }
        } else if (candidate.rfind("169.254.", 0) == 0) {
            score = 5;
        } else if (candidate.rfind("26.", 0) == 0) {
            // Radmin/Hamachi-style virtual/VPN address: useful for VPN peers,
            // but never beat a real same-Wi-Fi RFC1918 address for phone QR.
            score = 25;
        }

        if (score > bestScore) {
            bestScore = score;
            bestAddress = candidate;
        }
    }

    freeaddrinfo(result);
    return bestAddress;
}

'''
text = text[:host_match.start()] + host_replacement + text[host_match.end():]

# While a public tunnel is warming or has failed, do not blank the QR. With the
# LAN picker above, the fallback is now a useful same-Wi-Fi URL instead of the
# unroutable 26.x virtual-adapter address that V3 was protecting against.
old_lan_gate = r'''        // PHOBOI_NO_VPN_QR_DURING_CLOUDFLARE_V3
        if (gCloudflareStatus != "OFF") {
            return "";
        }
'''
new_lan_gate = r'''        // PHOBOI_NO_VPN_QR_DURING_CLOUDFLARE_V3
        // PHOBOI_LAN_QR_FALLBACK_V6
        // Public tunnel unavailable/warming: fall through to the preferred
        // physical LAN address so a same-Wi-Fi phone can connect immediately.
'''
if old_lan_gate not in text:
    raise SystemExit("V3 blank-QR public tunnel gate not found")
text = text.replace(old_lan_gate, new_lan_gate, 1)

# ---------------------------------------------------------------------------
# WinHTTP 12007 is ERROR_WINHTTP_NAME_NOT_RESOLVED. Record every verifier error
# and switch away from a trycloudflare hostname immediately when that exact DNS
# failure occurs. This matches both the new V5 log and Safari's server-not-found
# screen, rather than guessing at QUIC/TCP again.
# ---------------------------------------------------------------------------
send_old = r'''        if (!sent) {
            if (verbose) debugPrint("[PHOBOI PUBLIC PROBE] send failed host=%s winerr=%lu\n", host.c_str(), GetLastError());
        } else if (!WinHttpReceiveResponse(request, nullptr)) {'''
send_new = r'''        if (!sent) {
            DWORD winError = GetLastError();
            gPhoBoiPublicProbeLastError.store(winError);
            if (verbose) debugPrint("[PHOBOI PUBLIC PROBE] send failed host=%s winerr=%lu\n", host.c_str(), winError);
            // PHOBOI_TRYCLOUDFLARE_DNS_FAILOVER_V6
            if (winError == ERROR_WINHTTP_NAME_NOT_RESOLVED
                && host.find(".trycloudflare.com") != std::string::npos) {
                mobileStartPublicDnsFallback();
            }
        } else if (!WinHttpReceiveResponse(request, nullptr)) {'''
if send_old not in text:
    raise SystemExit("V5 WinHTTP send diagnostic block not found")
text = text.replace(send_old, send_new, 1)

receive_old = r'''        } else if (!WinHttpReceiveResponse(request, nullptr)) {
            if (verbose) debugPrint("[PHOBOI PUBLIC PROBE] receive failed host=%s winerr=%lu\n", host.c_str(), GetLastError());
        } else {'''
receive_new = r'''        } else if (!WinHttpReceiveResponse(request, nullptr)) {
            DWORD winError = GetLastError();
            gPhoBoiPublicProbeLastError.store(winError);
            if (verbose) debugPrint("[PHOBOI PUBLIC PROBE] receive failed host=%s winerr=%lu\n", host.c_str(), winError);
        } else {
            gPhoBoiPublicProbeLastError.store(ERROR_SUCCESS);'''
if receive_old not in text:
    raise SystemExit("V5 WinHTTP receive diagnostic block not found")
text = text.replace(receive_old, receive_new, 1)

# ---------------------------------------------------------------------------
# localhost.run fallback. It uses the Windows OpenSSH client, creates an HTTPS
# HTTP tunnel to the same 127.0.0.1:27888 origin, parses the assigned public URL
# from SSH output, then runs the same /health verifier before publishing it.
# Free localhost.run tunnels are intentionally ephemeral, which matches PhoBoi's
# existing NEW LINK behavior.
# ---------------------------------------------------------------------------
start_anchor = '''bool mobileStartCloudflareTunnel()\n{\n'''
if start_anchor not in text:
    raise SystemExit("Cloudflare start function anchor missing for V6 helpers")

helpers = r'''// PHOBOI_LOCALHOST_RUN_FALLBACK_V6
std::string mobileWindowsSshPath()
{
    char systemDirectory[MAX_PATH] {};
    UINT length = GetSystemDirectoryA(systemDirectory, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
        std::string candidate(systemDirectory, length);
        if (!candidate.empty() && candidate.back() != '\\') {
            candidate.push_back('\\');
        }
        candidate += "OpenSSH\\ssh.exe";
        if (GetFileAttributesA(candidate.c_str()) != INVALID_FILE_ATTRIBUTES) {
            return candidate;
        }
    }

    char resolved[MAX_PATH] {};
    DWORD found = SearchPathA(nullptr, "ssh.exe", nullptr, MAX_PATH, resolved, nullptr);
    if (found > 0 && found < MAX_PATH) {
        return std::string(resolved, found);
    }
    return "";
}

bool mobileExtractLocalhostRunUrl(const std::string& output, std::string& url)
{
    size_t start = 0;
    while ((start = output.find("https://", start)) != std::string::npos) {
        size_t end = start;
        while (end < output.size()) {
            unsigned char ch = static_cast<unsigned char>(output[end]);
            if (ch <= 32 || ch == '\"' || ch == '\'' || ch == '<' || ch == '>'
                || ch == '(' || ch == ')' || ch == '[' || ch == ']' || ch == 0x1B) {
                break;
            }
            end++;
        }
        std::string candidate = output.substr(start, end - start);
        while (!candidate.empty()
            && (candidate.back() == '.' || candidate.back() == ',' || candidate.back() == ';')) {
            candidate.pop_back();
        }
        if (candidate.find(".localhost.run") != std::string::npos) {
            url = candidate;
            return true;
        }
        start += 8;
    }
    return false;
}

void mobileReleasePublicFallbackHandles()
{
    if (gPhoBoiFallbackOutputThread.joinable()) {
        gPhoBoiFallbackOutputThread.join();
    }
    if (gPhoBoiFallbackOutput != nullptr) {
        CloseHandle(gPhoBoiFallbackOutput);
        gPhoBoiFallbackOutput = nullptr;
    }
    if (gPhoBoiFallbackProcess != nullptr) {
        CloseHandle(gPhoBoiFallbackProcess);
        gPhoBoiFallbackProcess = nullptr;
    }
}

void mobileStopPublicFallbackTunnel()
{
    if (gPhoBoiFallbackProcess != nullptr) {
        TerminateProcess(gPhoBoiFallbackProcess, 0);
        WaitForSingleObject(gPhoBoiFallbackProcess, 3000);
    }
    mobileReleasePublicFallbackHandles();
    gPhoBoiFallbackStarting.store(false);
}

bool mobileStartLocalhostRunTunnel()
{
    std::string ssh = mobileWindowsSshPath();
    if (ssh.empty()) {
        std::lock_guard<std::mutex> lock(gCloudflareStateMutex);
        gCloudflareStatus = "DNS FAIL - SSH FALLBACK MISSING";
        debugPrint("[PHOBOI FALLBACK] Windows OpenSSH client not found; same-Wi-Fi LAN QR remains available\n");
        gPhoBoiFallbackStarting.store(false);
        return false;
    }

    SECURITY_ATTRIBUTES attributes {};
    attributes.nLength = sizeof(attributes);
    attributes.bInheritHandle = TRUE;

    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &attributes, 0)) {
        debugPrint("[PHOBOI FALLBACK] CreatePipe failed winerr=%lu\n", GetLastError());
        gPhoBoiFallbackStarting.store(false);
        return false;
    }
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    std::string command = "\\\"" + ssh
        + "\\\" -T -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o ConnectTimeout=10"
          " -o ServerAliveInterval=30 -o ServerAliveCountMax=3 -o ExitOnForwardFailure=yes"
          " -R 80:127.0.0.1:27888 nokey@localhost.run";
    std::vector<char> commandBuffer(command.begin(), command.end());
    commandBuffer.push_back('\0');

    STARTUPINFOA startup {};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = writePipe;
    startup.hStdError = writePipe;

    PROCESS_INFORMATION process {};
    BOOL created = CreateProcessA(
        nullptr,
        commandBuffer.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &startup,
        &process);
    CloseHandle(writePipe);

    if (!created) {
        DWORD winError = GetLastError();
        CloseHandle(readPipe);
        debugPrint("[PHOBOI FALLBACK] ssh launch failed winerr=%lu\n", winError);
        std::lock_guard<std::mutex> lock(gCloudflareStateMutex);
        gCloudflareStatus = "DNS FAIL - FALLBACK LAUNCH FAILED";
        gPhoBoiFallbackStarting.store(false);
        return false;
    }

    CloseHandle(process.hThread);
    gPhoBoiFallbackProcess = process.hProcess;
    gPhoBoiFallbackOutput = readPipe;
    debugPrint("[PHOBOI FALLBACK] localhost.run SSH tunnel starting origin=http://127.0.0.1:27888\n");

    gPhoBoiFallbackOutputThread = std::thread([]() {
        std::string output;
        std::string candidateUrl;
        bool candidateHandled = false;
        char buffer[2048];
        DWORD count = 0;

        while (gPhoBoiFallbackOutput != nullptr
            && ReadFile(gPhoBoiFallbackOutput, buffer, sizeof(buffer), &count, nullptr)
            && count != 0) {
            std::string chunk(buffer, static_cast<size_t>(count));
            debugPrint("[PHOBOI FALLBACK SSH] %s\n", chunk.c_str());
            output.append(chunk);
            if (output.size() > 32768) {
                output.erase(0, output.size() - 32768);
            }

            if (!candidateHandled && mobileExtractLocalhostRunUrl(output, candidateUrl)) {
                candidateHandled = true;
                debugPrint("[PHOBOI FALLBACK] localhost.run candidate url=%s\n", candidateUrl.c_str());

                bool reachable = false;
                for (int attempt = 0; attempt < 20 && !reachable; attempt++) {
                    reachable = mobileVerifyCloudflarePublicHealth(candidateUrl);
                    if (!reachable) {
                        Sleep(500);
                    }
                }

                {
                    std::lock_guard<std::mutex> lock(gCloudflareStateMutex);
                    gCloudflarePublicUrl = candidateUrl;
                    gCloudflareStatus = reachable
                        ? "READY - LOCALHOST.RUN VERIFIED"
                        : "LOCALHOST.RUN - PUBLIC UNVERIFIED";
                }
                debugPrint("[PHOBOI FALLBACK] localhost.run public url=%s verified=%d\n",
                    candidateUrl.c_str(), reachable ? 1 : 0);
            }
        }

        DWORD exitCode = 0;
        if (gPhoBoiFallbackProcess != nullptr) {
            GetExitCodeProcess(gPhoBoiFallbackProcess, &exitCode);
        }
        if (!candidateHandled) {
            std::lock_guard<std::mutex> lock(gCloudflareStateMutex);
            if (gCloudflarePublicUrl.empty()) {
                gCloudflareStatus = "PUBLIC FALLBACK FAILED";
            }
            debugPrint("[PHOBOI FALLBACK] localhost.run ended before URL exit=%lu\n", exitCode);
        }
        gPhoBoiFallbackStarting.store(false);
    });

    return true;
}

void mobileStartPublicDnsFallback()
{
    bool expected = false;
    if (!gPhoBoiFallbackStarting.compare_exchange_strong(expected, true)) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(gCloudflareStateMutex);
        if (gCloudflarePublicUrl.find(".trycloudflare.com") != std::string::npos) {
            gCloudflarePublicUrl.clear();
        }
        gCloudflareStatus = "CLOUDFLARE DNS FAIL - SWITCHING";
    }

    debugPrint("[PHOBOI MOBILE] Cloudflare hostname DNS failed with WinHTTP 12007; switching to localhost.run public fallback; LAN QR stays active\n");
    std::thread([]() {
        mobileStartLocalhostRunTunnel();
    }).detach();
}

'''
text = text.replace(start_anchor, helpers + start_anchor, 1)

# Reset/new-link must stop both providers while preserving the already-reserved
# co-op character slots. The existing reset function already guarantees the
# character side; extend only the transport cleanup.
cleanup_anchor = '''    mobileReleaseCloudflareHandles();\n    std::lock_guard<std::mutex> lock(gCloudflareStateMutex);\n'''
cleanup_replacement = '''    mobileReleaseCloudflareHandles();\n    // PHOBOI_TUNNEL_RESET_ALL_V6\n    mobileStopPublicFallbackTunnel();\n    std::lock_guard<std::mutex> lock(gCloudflareStateMutex);\n'''
if cleanup_anchor not in text:
    raise SystemExit("Cloudflare stop cleanup anchor missing")
text = text.replace(cleanup_anchor, cleanup_replacement, 1)

for required in (
    "PHOBOI_PUBLIC_FAILOVER_V6",
    "PHOBOI_TRYCLOUDFLARE_DNS_FAILOVER_V6",
    "PHOBOI_LOCALHOST_RUN_FALLBACK_V6",
    "PHOBOI_LAN_ADDRESS_PREFERENCE_V6",
    "PHOBOI_LAN_QR_FALLBACK_V6",
    "PHOBOI_TUNNEL_RESET_ALL_V6",
):
    if required not in text:
        raise SystemExit(f"missing PhoBoi public failover V6 marker {required}")

# Hard fail if the generated source somehow loses the exact DNS trigger or the
# alternate origin mapping. These are the two pieces this runtime report proved
# we need.
if "ERROR_WINHTTP_NAME_NOT_RESOLVED" not in text:
    raise SystemExit("V6 DNS failover does not test WinHTTP name resolution")
if "-R 80:127.0.0.1:27888 nokey@localhost.run" not in text:
    raise SystemExit("V6 localhost.run tunnel command missing")

path.write_text(text, encoding="utf-8")
print("Applied PhoBoi V6: immediate 12007 DNS failover, localhost.run HTTPS fallback, physical-LAN QR preference, and all-provider reset")
