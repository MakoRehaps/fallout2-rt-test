from pathlib import Path

path = Path("src/local_coop_mobile.cc")
text = path.read_text(encoding="utf-8")

marker = "PHOBOI_PER_USER_ZOOM_V1"
zoom_css = r"""
/* PHOBOI_PER_USER_ZOOM_V1 */
#video{
  transform:scale(var(--phoboi-view-zoom,1))!important;
  transform-origin:50% 50%!important;
}
""".strip()

if marker not in text:
    if "</style>" not in text:
        raise SystemExit("PhoBoi style closing tag not found")
    text = text.replace("</style>", zoom_css + "\n</style>", 1)

old_zoom = "let phoneViewZoom=0;$('view').addEventListener('click',ev=>{ev.preventDefault();phoneViewZoom=(phoneViewZoom+1)%3;$('pad').classList.remove('view-zoom1','view-zoom2');if(phoneViewZoom===1)$('pad').classList.add('view-zoom1');if(phoneViewZoom===2)$('pad').classList.add('view-zoom2');updateStatus()});"
new_zoom = r"""// PHOBOI_PER_USER_ZOOM_V1
const phoneViewSteps=[1,1.25,1.5,1.75,2];
let phoneViewZoom=parseInt(localStorage.getItem('phoboiViewZoom')||'0',10);
if(!Number.isFinite(phoneViewZoom)||phoneViewZoom<0||phoneViewZoom>=phoneViewSteps.length)phoneViewZoom=0;
function phoneViewLabel(){return phoneViewZoom===0?'FIT':`${Math.round(phoneViewSteps[phoneViewZoom]*100)}%`}
function applyPhoneViewZoom(){
 document.documentElement.style.setProperty('--phoboi-view-zoom',String(phoneViewSteps[phoneViewZoom]));
 localStorage.setItem('phoboiViewZoom',String(phoneViewZoom));
}
applyPhoneViewZoom();
$('view').addEventListener('click',ev=>{ev.preventDefault();phoneViewZoom=(phoneViewZoom+1)%phoneViewSteps.length;applyPhoneViewZoom();updateStatus()});"""

if "const phoneViewSteps=[1,1.25,1.5,1.75,2];" not in text:
    if old_zoom not in text:
        raise SystemExit("PhoBoi old three-step zoom handler not found")
    text = text.replace(old_zoom, new_zoom, 1)

# Support either the old detailed status or the compact text-readability status,
# depending on which patch ran first. The label always reflects the local phone.
status_old = "$('top').textContent=`P${slot+1} | CTRL ${controlMode} | VIDEO ${videoMode} | VIEW ${phoneViewZoom===0?'FIT':phoneViewZoom===1?'1.3X':'1.6X'}${link}`;"
status_compact = "$('top').textContent=`P${slot+1}  •  ${videoMode}  •  ${phoneViewZoom===0?'FIT':phoneViewZoom===1?'1.3X':'1.6X'}${link}`; // PHOBOI_COMPACT_STATUS_TEXT_V1"
status_new = "$('top').textContent=`P${slot+1}  •  ${videoMode}  •  ${phoneViewLabel()}${link}`; // PHOBOI_COMPACT_STATUS_TEXT_V1 PHOBOI_PER_USER_ZOOM_STATUS_V1"
if "PHOBOI_PER_USER_ZOOM_STATUS_V1" not in text:
    if status_compact in text:
        text = text.replace(status_compact, status_new, 1)
    elif status_old in text:
        text = text.replace(status_old, status_new, 1)
    else:
        raise SystemExit("PhoBoi status line not found for zoom label")

path.write_text(text, encoding="utf-8")
print("Added per-phone FIT/125/150/175/200 percent persistent game zoom")
