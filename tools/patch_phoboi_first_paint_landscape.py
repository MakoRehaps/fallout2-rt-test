from pathlib import Path

path = Path("src/local_coop_mobile.cc")
text = path.read_text(encoding="utf-8")

marker = "PHOBOI_FIRST_PAINT_LANDSCAPE_V1"

css = r"""
/* PHOBOI_FIRST_PAINT_LANDSCAPE_V1
   Safari can report stale orientation/VisualViewport dimensions during the
   first few frames. These JS-owned classes override the media-query fallback
   so the controller surface snaps to the correct logical landscape state as
   soon as we can measure the real viewport. */
html.phoboi-force-landscape #pad{
  inset:auto!important;
  left:50%!important;
  top:50%!important;
  width:var(--phoboi-h,100dvh)!important;
  height:var(--phoboi-w,100dvw)!important;
  transform:translate(-50%,-50%) rotate(90deg)!important;
}
html.phoboi-native-landscape #pad{
  inset:0!important;
  left:0!important;
  top:0!important;
  width:var(--phoboi-w,100dvw)!important;
  height:var(--phoboi-h,100dvh)!important;
  transform:none!important;
}
""".strip()

if marker not in text:
    if "</style>" not in text:
        raise SystemExit("PhoBoi style closing tag not found")
    text = text.replace("</style>", css + "\n</style>", 1)

js_marker = "PHOBOI_FIRST_PAINT_LAYOUT_BURST_V1"
js = r"""
// PHOBOI_FIRST_PAINT_LAYOUT_BURST_V1
function phoboiForceImmediateLayout(){
 const vv=window.visualViewport;
 const w=Math.max(1,Math.round(vv&&vv.width?vv.width:window.innerWidth||document.documentElement.clientWidth||screen.width));
 const h=Math.max(1,Math.round(vv&&vv.height?vv.height:window.innerHeight||document.documentElement.clientHeight||screen.height));
 const portrait=h>w;
 const root=document.documentElement;
 root.classList.toggle('phoboi-force-landscape',portrait);
 root.classList.toggle('phoboi-native-landscape',!portrait);
 if(typeof phoboiLayout==='function')phoboiLayout();
}
function phoboiBootLayoutBurst(){
 phoboiForceImmediateLayout();
 requestAnimationFrame(()=>{
  phoboiForceImmediateLayout();
  requestAnimationFrame(phoboiForceImmediateLayout);
 });
 [40,100,180,320,550,900].forEach(ms=>setTimeout(phoboiForceImmediateLayout,ms));
}
phoboiBootLayoutBurst();
document.addEventListener('DOMContentLoaded',phoboiBootLayoutBurst,{once:true});
window.addEventListener('load',phoboiBootLayoutBurst,{once:true});
window.addEventListener('pageshow',phoboiBootLayoutBurst,{passive:true});
window.addEventListener('resize',phoboiForceImmediateLayout,{passive:true});
window.addEventListener('orientationchange',phoboiBootLayoutBurst,{passive:true});
window.visualViewport?.addEventListener('resize',phoboiForceImmediateLayout,{passive:true});
window.visualViewport?.addEventListener('scroll',phoboiForceImmediateLayout,{passive:true});
""".strip()

if js_marker not in text:
    anchor = "window.visualViewport?.addEventListener('scroll',phoboiLayout,{passive:true});"
    if anchor not in text:
        raise SystemExit("PhoBoi adaptive viewport JS anchor not found")
    text = text.replace(anchor, anchor + "\n" + js, 1)

# Ship a hard regression guard: first-load landscape behavior must no longer
# depend solely on @media orientation state.
for required in (marker, js_marker, "phoboi-force-landscape", "phoboi-native-landscape"):
    if required not in text:
        raise SystemExit(f"PhoBoi first-paint landscape marker missing: {required}")

path.write_text(text, encoding="utf-8")
print("Applied immediate first-paint PhoBoi landscape stabilization")
