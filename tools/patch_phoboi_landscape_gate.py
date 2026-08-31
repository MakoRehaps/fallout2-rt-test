from pathlib import Path

path = Path("src/local_coop_mobile.cc")
text = path.read_text(encoding="utf-8")

old = "@media (orientation:portrait){#pad:before{content:'ROTATE PHONE TO LANDSCAPE';position:absolute;inset:0;display:flex;align-items:center;justify-content:center;background:#07100b;color:#ffd56a;font-size:18px;z-index:15}#video,.stick,.btn,.small,.dpad,.top{visibility:hidden}}"
legacy_nonblocking = "/* PHOBOI_NONBLOCKING_LANDSCAPE_HINT_V1 */\\n@media (orientation:portrait){#pad:before{content:'ROTATE PHONE TO LANDSCAPE';position:absolute;left:50%;top:max(4px,env(safe-area-inset-top));transform:translateX(-50%);display:block;width:max-content;max-width:92vw;padding:5px 10px;border:1px solid #ffd56a;border-radius:6px;background:#07100bcc;color:#ffd56a;font-size:12px;line-height:1.1;z-index:15;pointer-events:none}#video,.stick,.btn,.small,.dpad,.top{visibility:visible}}"
no_gate = "/* PHOBOI_NONBLOCKING_LANDSCAPE_HINT_V1 PHOBOI_NO_LANDSCAPE_GATE_V2 */\\n@media (orientation:portrait){#pad:before{content:none!important;display:none!important}#video,.stick,.btn,.small,.dpad,.top{visibility:visible!important}}"

overlay_css = r"""
/* PHOBOI_TRANSPARENT_CONTROLLER_OVERLAY_V1 */
#pad{background:#000}
#video{position:absolute;inset:0;width:100%;height:100%;z-index:1}
.top{background:rgba(7,16,11,.30);border:1px solid rgba(121,255,145,.18)}
.stick{background:rgba(7,16,11,.16);border-color:rgba(121,255,145,.55);opacity:.58}
.nub{background:rgba(104,221,130,.50)}
.btn,.small{background:rgba(16,45,25,.42);border-color:rgba(121,255,145,.62);opacity:.60}
.dpad{opacity:.60}
.dpad button{background:rgba(16,45,25,.42);border-color:rgba(121,255,145,.62)}
.btn:active,.small:active,.dpad button:active{opacity:.96;background:rgba(16,45,25,.78)}
.stick:active{opacity:.82}
""".strip()

if "PHOBOI_NO_LANDSCAPE_GATE_V2" not in text:
    if legacy_nonblocking in text:
        text = text.replace(legacy_nonblocking, no_gate, 1)
        print("Removed PhoBoi landscape warning overlay")
    elif old in text:
        text = text.replace(old, no_gate, 1)
        print("Removed PhoBoi landscape gate")
    else:
        raise SystemExit("PhoBoi portrait landscape CSS block not found")
else:
    print("PhoBoi landscape gate already removed")

if "PHOBOI_TRANSPARENT_CONTROLLER_OVERLAY_V1" not in text:
    anchor = "</style>"
    if anchor not in text:
        raise SystemExit("PhoBoi style closing tag not found")
    text = text.replace(anchor, overlay_css + "\n" + anchor, 1)
    print("Applied transparent PhoBoi controller overlay")
else:
    print("PhoBoi transparent controller overlay already applied")

path.write_text(text, encoding="utf-8")
