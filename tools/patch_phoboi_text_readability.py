from pathlib import Path

path = Path("src/local_coop_mobile.cc")
text = path.read_text(encoding="utf-8")

# PHOBOI_TEXT_READABILITY_V2
# Fallout's tiny bitmap UI becomes hard to read when a low-resolution remote
# frame is enlarged with CSS nearest-neighbour scaling. On phones, preserve the
# game pixels in the stream and let WebKit/Chromium smoothly downsample them to
# the physical display. Controller labels remain DOM text and can stay crisp.
text = text.replace("image-rendering:pixelated", "image-rendering:auto")

readability_css = r"""
/* PHOBOI_TEXT_READABILITY_V2 */
#video{
  image-rendering:auto!important;
  -webkit-transform-style:preserve-3d;
}
.top,.btn,.small,.control-label,.stick-label,.dpad button{
  font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Arial,sans-serif!important;
  -webkit-font-smoothing:antialiased;
  text-rendering:optimizeLegibility;
  text-shadow:0 1px 2px #000,0 0 4px #000;
  letter-spacing:0!important;
}
.top{font-weight:700!important;color:#fff!important;background:rgba(7,16,11,.52)!important}
.btn,.small,.dpad button{font-weight:800!important;color:#f2fff4!important}
.control-label,.stick-label{font-weight:650!important;color:#f1fff3!important;opacity:.98!important}
""".strip()

if "PHOBOI_TEXT_READABILITY_V2" not in text:
    if "</style>" not in text:
        raise SystemExit("PhoBoi style closing tag not found")
    text = text.replace("</style>", readability_css + "\n</style>", 1)
    print("Enabled smooth phone video scaling and readable controller text")
else:
    print("PhoBoi text readability CSS already applied")

# The adaptive phone patch deliberately raised the old 256x144 emergency
# ladder, but even 480/640-wide frames can destroy Fallout's tiny glyph strokes.
# Preserve native 1280x720 pixels on healthy links and reduce FPS before
# reducing resolution. Controls are on a separate socket, so visual frame rate
# can safely yield first.
old_ladder = """    // PHOBOI_READABLE_STREAM_LADDER_V1
    int width = 480;
    int height = 270;
    int fps = 4;
    // Keep Fallout UI/text legible. On weaker links reduce frame rate first;
    // do not collapse the image to 256x144 and then enlarge the blur on-phone.
    if (worstRtt <= 55 && worstJitter <= 15 && worstInterval <= 26) {
        width = 960; height = 540; fps = 15;
    } else if (worstRtt <= 110 && worstJitter <= 30 && worstInterval <= 40) {
        width = 800; height = 450; fps = 12;
    } else if (worstRtt <= 180 && worstJitter <= 55 && worstInterval <= 70) {
        width = 640; height = 360; fps = 10;
    } else if (worstRtt <= 300 && worstJitter <= 100 && worstInterval <= 120) {
        width = 640; height = 360; fps = 6;
    } else {
        width = 480; height = 270; fps = 4;
    }
"""

new_ladder = """    // PHOBOI_READABLE_STREAM_LADDER_V2 PHOBOI_NATIVE_TEXT_STREAM_V1
    int width = 640;
    int height = 360;
    int fps = 3;
    // Text-first remote play: preserve glyph strokes and sacrifice video FPS
    // before resolution. 1280x720 is sent unscaled when the host surface and
    // connection can support it; Safari then downsamples once to its viewport.
    if (worstRtt <= 55 && worstJitter <= 15 && worstInterval <= 26) {
        width = 1280; height = 720; fps = 8;
    } else if (worstRtt <= 110 && worstJitter <= 30 && worstInterval <= 40) {
        width = 960; height = 540; fps = 8;
    } else if (worstRtt <= 180 && worstJitter <= 55 && worstInterval <= 70) {
        width = 800; height = 450; fps = 6;
    } else if (worstRtt <= 300 && worstJitter <= 100 && worstInterval <= 120) {
        width = 640; height = 360; fps = 5;
    } else {
        width = 640; height = 360; fps = 3;
    }

    // Never upscale the host framebuffer before streaming it. Upscaling here
    // creates fake pixels and makes small lettering worse. Let the browser fit
    // the native-or-downscaled image to the phone exactly once.
    if (captureSurface->w < width || captureSurface->h < height) {
        width = captureSurface->w;
        height = captureSurface->h;
    }
"""

if "PHOBOI_READABLE_STREAM_LADDER_V2" not in text:
    if old_ladder not in text:
        raise SystemExit("PhoBoi readable stream V1 ladder not found")
    text = text.replace(old_ladder, new_ladder, 1)
    print("Upgraded PhoBoi stream to text-first native-resolution ladder")
else:
    print("PhoBoi text-first stream ladder already applied")

# Give Safari a compact status line instead of a long diagnostic sentence
# crossing the shoulder buttons. Full diagnostics remain represented by mode
# values, but the visible HUD is easier to parse on a small screen.
old_status = "$('top').textContent=`P${slot+1} | CTRL ${controlMode} | VIDEO ${videoMode} | VIEW ${phoneViewZoom===0?'FIT':phoneViewZoom===1?'1.3X':'1.6X'}${link}`;"
new_status = "$('top').textContent=`P${slot+1}  •  ${videoMode}  •  ${phoneViewZoom===0?'FIT':phoneViewZoom===1?'1.3X':'1.6X'}${link}`; // PHOBOI_COMPACT_STATUS_TEXT_V1"
if "PHOBOI_COMPACT_STATUS_TEXT_V1" not in text:
    if old_status in text:
        text = text.replace(old_status, new_status, 1)
    else:
        raise SystemExit("PhoBoi status text anchor not found")

path.write_text(text, encoding="utf-8")
