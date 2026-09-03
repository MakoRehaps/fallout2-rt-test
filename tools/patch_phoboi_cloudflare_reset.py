from pathlib import Path
import runpy

path = Path("src/local_coop_mobile.cc")
text = path.read_text(encoding="utf-8")

marker = "PHOBOI_CLOUDFLARE_RESET_V1"
if marker in text:
    print("PhoBoi Cloudflare reset controls already applied")
    runpy.run_path("tools/patch_phoboi_cloudflare_route_v3.py", run_name="__main__")
    runpy.run_path("tools/patch_phoboi_cloudflare_route_v4.py", run_name="__main__")
    raise SystemExit(0)

old_stop = r'''void mobileStopCloudflareTunnel()
{
    if (gCloudflareProcess != nullptr) {
        TerminateProcess(gCloudflareProcess, 0);
        WaitForSingleObject(gCloudflareProcess, 3000);
    }
    mobileReleaseCloudflareHandles();
    std::lock_guard<std::mutex> lock(gCloudflareStateMutex);
    gCloudflarePublicUrl.clear();
    gCloudflareStatus = "OFF";
}
'''

new_stop = r'''void mobileStopCloudflareTunnel()
{
    if (gCloudflareProcess != nullptr) {
        TerminateProcess(gCloudflareProcess, 0);
        WaitForSingleObject(gCloudflareProcess, 3000);
    }
    mobileReleaseCloudflareHandles();
    std::lock_guard<std::mutex> lock(gCloudflareStateMutex);
    gCloudflarePublicUrl.clear();
    gCloudflareStatus = "OFF";
}

// PHOBOI_CLOUDFLARE_RESET_V1
// Resetting the public transport must never release a co-op character slot.
// gMobileSlots and gLocalCoopPlayers are deliberately untouched here.
bool mobileResetCloudflareTunnel(bool requestFreshLink)
{
    debugPrint("[PHOBOI MOBILE] Cloudflare %s requested; preserving character slots\n",
        requestFreshLink ? "new-link reset" : "reconnect reset");

    mobileStopCloudflareTunnel();
    mobileSetCloudflareStatus(requestFreshLink ? "NEW LINK..." : "RECONNECTING...");

    // Quick Tunnels are ephemeral. Even a reconnect may receive a different
    // trycloudflare.com URL, so the host QR/link is redrawn from live state.
    bool started = mobileStartCloudflareTunnel();
    if (!started) {
        return false;
    }

    return true;
}
'''

if old_stop not in text:
    raise SystemExit("Cloudflare stop function anchor not found")
text = text.replace(old_stop, new_stop, 1)

old_host_line = 'snprintf(line, sizeof(line), "C: COPY LINK   T: CLOUDFLARE HTTPS [%s]", tunnelStatus.c_str());'
new_host_line = 'snprintf(line, sizeof(line), "C:COPY  T:START  R:RESET  N:NEW LINK  [%s]", tunnelStatus.c_str());'
if old_host_line not in text:
    raise SystemExit("Cloudflare host status line anchor not found")
text = text.replace(old_host_line, new_host_line, 1)

old_https_help = 'windowDrawText(gMobileHostWindow, "HTTPS TUNNEL HIDES YOUR IP AND NEEDS NO VPN", 575, 20, 350, _colorTable[992]);'
new_https_help = 'windowDrawText(gMobileHostWindow, "RESET/NEW LINK PRESERVES ALL CHARACTER SLOTS", 575, 20, 350, _colorTable[992]);'
if old_https_help not in text:
    raise SystemExit("Cloudflare help text anchor not found")
text = text.replace(old_https_help, new_https_help, 1)

old_t_key = r'''    if (keyCode == KEY_LOWERCASE_T || keyCode == KEY_UPPERCASE_T) {
#ifdef _WIN32
        mobileStartCloudflareTunnel();
#endif
        mobileDrawHostWindow();
        return true;
    }
'''
new_t_key = r'''    if (keyCode == KEY_LOWERCASE_T || keyCode == KEY_UPPERCASE_T) {
#ifdef _WIN32
        mobileStartCloudflareTunnel();
#endif
        mobileDrawHostWindow();
        return true;
    }

    // PHOBOI_CLOUDFLARE_RESET_KEYS_V1
    if (keyCode == KEY_LOWERCASE_R || keyCode == KEY_UPPERCASE_R) {
#ifdef _WIN32
        mobileResetCloudflareTunnel(false);
#endif
        mobileDrawHostWindow();
        return true;
    }

    if (keyCode == KEY_LOWERCASE_N || keyCode == KEY_UPPERCASE_N) {
#ifdef _WIN32
        mobileResetCloudflareTunnel(true);
#endif
        mobileDrawHostWindow();
        return true;
    }
'''
if old_t_key not in text:
    raise SystemExit("Cloudflare T-key anchor not found")
text = text.replace(old_t_key, new_t_key, 1)

path.write_text(text, encoding="utf-8")
print("Added Cloudflare reset/new-link controls without releasing character slots")

# Route V3 proves the local origin and fixes response/QR handling. Route V4
# then isolates Quick Tunnel from any user cloudflared config, lets cloudflared
# negotiate QUIC/HTTP2 automatically, keeps stdout draining while public DNS is
# warming up, and performs the longer public verification asynchronously.
runpy.run_path("tools/patch_phoboi_cloudflare_route_v3.py", run_name="__main__")
runpy.run_path("tools/patch_phoboi_cloudflare_route_v4.py", run_name="__main__")
