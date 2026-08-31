from pathlib import Path

path = Path("src/local_coop_mobile.cc")
text = path.read_text(encoding="utf-8")

marker = "PHOBOI_ADAPTIVE_PHONE_VIEWPORT_V1"

adaptive_css = r"""
/* PHOBOI_ADAPTIVE_PHONE_VIEWPORT_V1
   The controller is laid out against the phone's actual VisualViewport, not
   against the host PC's 1280x720 window. In portrait Safari we rotate the
   controller surface itself into a logical landscape canvas, so an iPhone
   with orientation lock enabled still gets a usable landscape game view. */
#pad{
  --phoboi-ui:1;
  width:var(--phoboi-w,100dvw)!important;
  height:var(--phoboi-h,100dvh)!important;
  max-width:none!important;
  max-height:none!important;
  transform-origin:50% 50%;
}
#video{
  inset:0!important;
  width:100%!important;
  height:100%!important;
  object-fit:contain!important;
  image-rendering:pixelated;
}
.top{
  left:18%!important;right:18%!important;
  top:max(calc(3px * var(--phoboi-ui)),env(safe-area-inset-top))!important;
  height:calc(28px * var(--phoboi-ui))!important;
  line-height:calc(28px * var(--phoboi-ui))!important;
  font-size:calc(13px * var(--phoboi-ui))!important;
}
.stick{
  width:calc(150px * var(--phoboi-ui))!important;
  height:calc(150px * var(--phoboi-ui))!important;
  bottom:max(calc(10px * var(--phoboi-ui)),env(safe-area-inset-bottom))!important;
}
#ls{left:max(calc(10px * var(--phoboi-ui)),env(safe-area-inset-left))!important}
#rs{right:calc(max(calc(10px * var(--phoboi-ui)),env(safe-area-inset-right)) + calc(164px * var(--phoboi-ui)))!important}
.btn{
  width:calc(66px * var(--phoboi-ui))!important;
  height:calc(66px * var(--phoboi-ui))!important;
  font-size:calc(15px * var(--phoboi-ui))!important;
}
#ba{right:max(calc(10px * var(--phoboi-ui)),env(safe-area-inset-right))!important;bottom:calc(60px * var(--phoboi-ui))!important}
#bb{right:calc(62px * var(--phoboi-ui))!important;bottom:max(calc(8px * var(--phoboi-ui)),env(safe-area-inset-bottom))!important}
#bx{right:calc(62px * var(--phoboi-ui))!important;bottom:calc(112px * var(--phoboi-ui))!important}
#by{right:calc(114px * var(--phoboi-ui))!important;bottom:calc(60px * var(--phoboi-ui))!important}
.small{
  width:calc(76px * var(--phoboi-ui))!important;
  height:calc(42px * var(--phoboi-ui))!important;
  font-size:calc(13px * var(--phoboi-ui))!important;
}
#lb{left:max(calc(8px * var(--phoboi-ui)),env(safe-area-inset-left))!important;top:max(calc(4px * var(--phoboi-ui)),env(safe-area-inset-top))!important}
#lt{left:calc(86px * var(--phoboi-ui))!important;top:max(calc(4px * var(--phoboi-ui)),env(safe-area-inset-top))!important}
#rb{right:calc(86px * var(--phoboi-ui))!important;top:max(calc(4px * var(--phoboi-ui)),env(safe-area-inset-top))!important}
#rt{right:max(calc(8px * var(--phoboi-ui)),env(safe-area-inset-right))!important;top:max(calc(4px * var(--phoboi-ui)),env(safe-area-inset-top))!important}
#back{left:calc(50% - calc(112px * var(--phoboi-ui)))!important;top:calc(36px * var(--phoboi-ui))!important}
#fps{left:calc(50% - calc(38px * var(--phoboi-ui)))!important;top:calc(36px * var(--phoboi-ui))!important}
#start{right:calc(50% - calc(112px * var(--phoboi-ui)))!important;top:calc(36px * var(--phoboi-ui))!important}
#skill{left:calc(50% - calc(38px * var(--phoboi-ui)))!important;top:calc(82px * var(--phoboi-ui))!important}
#view{left:calc(50% - calc(44px * var(--phoboi-ui)))!important;top:calc(128px * var(--phoboi-ui))!important;width:calc(88px * var(--phoboi-ui))!important}
.dpad{
  left:calc(max(calc(10px * var(--phoboi-ui)),env(safe-area-inset-left)) + calc(166px * var(--phoboi-ui)))!important;
  bottom:max(calc(10px * var(--phoboi-ui)),env(safe-area-inset-bottom))!important;
  grid-template-columns:repeat(3,calc(34px * var(--phoboi-ui)))!important;
  grid-template-rows:repeat(3,calc(34px * var(--phoboi-ui)))!important;
}
.control-label{font-size:calc(10px * var(--phoboi-ui))!important}
.stick-label{font-size:calc(11px * var(--phoboi-ui))!important}

/* iPhone fallback for portrait-locked Safari: emulate landscape in CSS. */
@media (orientation:portrait){
  #pad{
    inset:auto!important;
    left:50%!important;
    top:50%!important;
    width:var(--phoboi-h,100dvh)!important;
    height:var(--phoboi-w,100dvw)!important;
    transform:translate(-50%,-50%) rotate(90deg)!important;
  }
}
@media (orientation:landscape){
  #pad{
    inset:0!important;
    left:0!important;
    top:0!important;
    width:var(--phoboi-w,100dvw)!important;
    height:var(--phoboi-h,100dvh)!important;
    transform:none!important;
  }
}
""".strip()

if marker not in text:
    if "</style>" not in text:
        raise SystemExit("PhoBoi style closing tag not found")
    text = text.replace("</style>", adaptive_css + "\n</style>", 1)
    print("Applied adaptive phone viewport and portrait landscape emulation")
else:
    print("Adaptive PhoBoi viewport already applied")

layout_js = r"""
// PHOBOI_VISUAL_VIEWPORT_SCALE_V1
function phoboiLayout(){
 const vv=window.visualViewport;
 const w=Math.max(280,Math.round(vv?vv.width:window.innerWidth));
 const h=Math.max(280,Math.round(vv?vv.height:window.innerHeight));
 // Logical controller height is the short edge because portrait is rendered
 // as a rotated landscape surface. 720 logical pixels is the original target.
 const logicalHeight=Math.min(w,h);
 const ui=Math.max(.72,Math.min(1.28,logicalHeight/720));
 const root=document.documentElement.style;
 root.setProperty('--phoboi-w',`${w}px`);
 root.setProperty('--phoboi-h',`${h}px`);
 root.setProperty('--phoboi-ui',ui.toFixed(3));
}
phoboiLayout();
window.addEventListener('resize',phoboiLayout,{passive:true});
window.addEventListener('orientationchange',()=>setTimeout(phoboiLayout,80),{passive:true});
window.visualViewport?.addEventListener('resize',phoboiLayout,{passive:true});
window.visualViewport?.addEventListener('scroll',phoboiLayout,{passive:true});
""".strip()

if "PHOBOI_VISUAL_VIEWPORT_SCALE_V1" not in text:
    anchor = "const $=id=>document.getElementById(id);"
    if anchor not in text:
        raise SystemExit("PhoBoi JS bootstrap anchor not found")
    text = text.replace(anchor, anchor + "\n" + layout_js, 1)
    print("Applied VisualViewport-based phone scaling")

# Prefer readable pixels over a tiny high-FPS feed. The previous emergency
# ladder could collapse all the way to 256x144, which is unreadable when blown
# up on a modern phone. Control traffic uses its own socket, so on weak links we
# lower video FPS before sacrificing the game image this aggressively.
old_quality = """    int width = 320;
    int height = 180;
    int fps = 8;
    // Controls always win over picture quality. zlib RGBA is intentionally
    // capped well below the old 960x540@24 mode to avoid tunnel bufferbloat.
    if (worstRtt <= 55 && worstJitter <= 15 && worstInterval <= 26) {
        width = 640; height = 360; fps = 15;
    } else if (worstRtt <= 110 && worstJitter <= 30 && worstInterval <= 40) {
        width = 480; height = 270; fps = 12;
    } else if (worstRtt <= 180 && worstJitter <= 55 && worstInterval <= 70) {
        width = 400; height = 225; fps = 10;
    } else if (worstRtt <= 300 && worstJitter <= 100 && worstInterval <= 120) {
        width = 320; height = 180; fps = 6;
    } else {
        width = 256; height = 144; fps = 4;
    }
"""
new_quality = """    // PHOBOI_READABLE_STREAM_LADDER_V1
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
if "PHOBOI_READABLE_STREAM_LADDER_V1" not in text:
    if old_quality not in text:
        raise SystemExit("PhoBoi stream quality ladder anchor not found")
    text = text.replace(old_quality, new_quality, 1)
    print("Raised PhoBoi readable stream quality ladder")

path.write_text(text, encoding="utf-8")
