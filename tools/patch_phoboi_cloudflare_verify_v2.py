from pathlib import Path

path = Path("src/local_coop_mobile.cc")
text = path.read_text(encoding="utf-8")

marker = "PHOBOI_CLOUDFLARE_PUBLIC_VERIFY_V2"
if marker in text:
    print("PhoBoi Cloudflare public verification V2 already applied")
    raise SystemExit(0)

# The V1 verifier used WinHTTP's default proxy path. On machines with VPN/proxy
# software that can make a perfectly healthy Quick Tunnel fail only from the
# host's own self-check. The public browser path should not depend on WinHTTP
# proxy configuration, so use a direct request and shorter probe timeouts.
old_proxy = "        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,"
new_proxy = "        WINHTTP_ACCESS_TYPE_NO_PROXY, // PHOBOI_CLOUDFLARE_DIRECT_SELF_PROBE_V2"
if old_proxy not in text:
    raise SystemExit("Cloudflare WinHTTP proxy anchor not found")
text = text.replace(old_proxy, new_proxy, 1)

old_timeouts = "    WinHttpSetTimeouts(session, 2500, 2500, 2500, 3500);"
new_timeouts = "    WinHttpSetTimeouts(session, 1000, 1000, 1500, 1500);"
if old_timeouts not in text:
    raise SystemExit("Cloudflare WinHTTP timeout anchor not found")
text = text.replace(old_timeouts, new_timeouts, 1)

# V1 treated the first failed probe as final by storing checkedCandidate before
# cloudflared had necessarily finished registering with the edge. That creates
# a permanent UNREACHABLE false-negative. V2 waits for cloudflared's own
# 'Registered tunnel connection' evidence, publishes that connected URL so the
# QR is usable, then performs a best-effort public /health self-probe. A failed
# host-side self-probe is diagnostic only; it never hides an edge-connected URL.
old = '''    gCloudflareOutputThread = std::thread([outputRead]() {\n        std::string output;\n        std::string checkedCandidate;\n        char buffer[2048];\n        DWORD count = 0;\n        while (ReadFile(outputRead, buffer, sizeof(buffer), &count, nullptr) && count != 0) {\n            output.append(buffer, static_cast<size_t>(count));\n            if (output.size() > 32768) {\n                output.erase(0, output.size() - 16384);\n            }\n            std::string publicUrl;\n            if (mobileFindCloudflareUrl(output, &publicUrl) && publicUrl != checkedCandidate) {\n                checkedCandidate = publicUrl;\n                {\n                    std::lock_guard<std::mutex> lock(gCloudflareStateMutex);\n                    gCloudflarePublicUrl.clear();\n                    gCloudflareStatus = "VERIFYING PUBLIC LINK";\n                }\n\n                // PHOBOI_CLOUDFLARE_READY_ONLY_QR_V1\n                bool reachable = false;\n                for (int attempt = 0; attempt < 10 && !reachable; attempt++) {\n                    reachable = mobileVerifyCloudflarePublicHealth(publicUrl);\n                    if (!reachable) Sleep(500);\n                }\n                {\n                    std::lock_guard<std::mutex> lock(gCloudflareStateMutex);\n                    if (reachable) {\n                        gCloudflarePublicUrl = publicUrl;\n                        gCloudflareStatus = "READY";\n                    } else {\n                        gCloudflarePublicUrl.clear();\n                        gCloudflareStatus = "UNREACHABLE - PRESS R";\n                    }\n                }\n                debugPrint("[PHOBOI MOBILE] public tunnel verification url=%s reachable=%d\\n",\n                    publicUrl.c_str(), reachable ? 1 : 0);\n            }\n        }'''

new = '''    gCloudflareOutputThread = std::thread([outputRead]() {\n        // PHOBOI_CLOUDFLARE_PUBLIC_VERIFY_V2\n        std::string output;\n        std::string candidateUrl;\n        bool edgeRegistered = false;\n        bool candidateHandled = false;\n        char buffer[2048];\n        DWORD count = 0;\n        while (ReadFile(outputRead, buffer, sizeof(buffer), &count, nullptr) && count != 0) {\n            output.append(buffer, static_cast<size_t>(count));\n            if (output.size() > 32768) {\n                output.erase(0, output.size() - 16384);\n            }\n\n            std::string publicUrl;\n            if (mobileFindCloudflareUrl(output, &publicUrl) && publicUrl != candidateUrl) {\n                candidateUrl = publicUrl;\n                candidateHandled = false;\n                std::lock_guard<std::mutex> lock(gCloudflareStateMutex);\n                gCloudflarePublicUrl.clear();\n                gCloudflareStatus = "URL ASSIGNED - CONNECTING EDGE";\n            }\n\n            if (output.find("Registered tunnel connection") != std::string::npos\n                || output.find("registered tunnel connection") != std::string::npos) {\n                edgeRegistered = true;\n            }\n\n            if (!candidateHandled && edgeRegistered && !candidateUrl.empty()) {\n                candidateHandled = true;\n                {\n                    // PHOBOI_CLOUDFLARE_EDGE_READY_FALLBACK_V2\n                    // Cloudflare documents a registered connector as the tunnel\n                    // being active. Publish the URL now instead of letting a\n                    // host-side WinHTTP/VPN/proxy false-negative hide the QR.\n                    std::lock_guard<std::mutex> lock(gCloudflareStateMutex);\n                    gCloudflarePublicUrl = candidateUrl;\n                    gCloudflareStatus = "READY - EDGE CONNECTED";\n                }\n\n                bool reachable = false;\n                for (int attempt = 0; attempt < 8 && !reachable; attempt++) {\n                    // Let TryCloudflare DNS/route propagation settle after the\n                    // connector registration before each bounded self-probe.\n                    if (attempt != 0) Sleep(750);\n                    reachable = mobileVerifyCloudflarePublicHealth(candidateUrl);\n                }\n\n                {\n                    std::lock_guard<std::mutex> lock(gCloudflareStateMutex);\n                    // PHOBOI_CLOUDFLARE_NONBLOCKING_SELF_CHECK_V2\n                    // A successful /health is stronger evidence. A failed host\n                    // self-check is explicitly inconclusive and must not turn a\n                    // registered Quick Tunnel into permanent UNREACHABLE.\n                    gCloudflarePublicUrl = candidateUrl;\n                    gCloudflareStatus = reachable\n                        ? "READY - PUBLIC VERIFIED"\n                        : "READY - EDGE CONNECTED";\n                }\n                debugPrint("[PHOBOI MOBILE] public tunnel V2 url=%s edge=1 self_probe=%d\\n",\n                    candidateUrl.c_str(), reachable ? 1 : 0);\n            }\n        }'''

if old not in text:
    raise SystemExit("Cloudflare V1 verification state-machine anchor not found")
text = text.replace(old, new, 1)

if 'gCloudflareStatus = "UNREACHABLE - PRESS R";' in text:
    raise SystemExit("Cloudflare permanent UNREACHABLE state survived V2 patch")

for required in (
    "PHOBOI_CLOUDFLARE_PUBLIC_VERIFY_V2",
    "PHOBOI_CLOUDFLARE_DIRECT_SELF_PROBE_V2",
    "PHOBOI_CLOUDFLARE_EDGE_READY_FALLBACK_V2",
    "PHOBOI_CLOUDFLARE_NONBLOCKING_SELF_CHECK_V2",
):
    if required not in text:
        raise SystemExit(f"missing Cloudflare V2 marker {required}")

path.write_text(text, encoding="utf-8")
print("Fixed Cloudflare public verification false-negative state machine")
