from pathlib import Path

path = Path("src/local_coop_mobile.cc")
text = path.read_text(encoding="utf-8")

old = "@media (orientation:portrait){#pad:before{content:'ROTATE PHONE TO LANDSCAPE';position:absolute;inset:0;display:flex;align-items:center;justify-content:center;background:#07100b;color:#ffd56a;font-size:18px;z-index:15}#video,.stick,.btn,.small,.dpad,.top{visibility:hidden}}"
legacy_nonblocking = "/* PHOBOI_NONBLOCKING_LANDSCAPE_HINT_V1 */\\n@media (orientation:portrait){#pad:before{content:'ROTATE PHONE TO LANDSCAPE';position:absolute;left:50%;top:max(4px,env(safe-area-inset-top));transform:translateX(-50%);display:block;width:max-content;max-width:92vw;padding:5px 10px;border:1px solid #ffd56a;border-radius:6px;background:#07100bcc;color:#ffd56a;font-size:12px;line-height:1.1;z-index:15;pointer-events:none}#video,.stick,.btn,.small,.dpad,.top{visibility:visible}}"
new = "/* PHOBOI_NONBLOCKING_LANDSCAPE_HINT_V1 PHOBOI_NO_LANDSCAPE_GATE_V2 */\\n@media (orientation:portrait){#pad:before{content:none!important;display:none!important}#video,.stick,.btn,.small,.dpad,.top{visibility:visible!important}}"

if "PHOBOI_NO_LANDSCAPE_GATE_V2" in text:
    print("PhoBoi landscape gate already removed")
elif legacy_nonblocking in text:
    text = text.replace(legacy_nonblocking, new, 1)
    path.write_text(text, encoding="utf-8")
    print("Removed PhoBoi landscape warning overlay")
elif old in text:
    text = text.replace(old, new, 1)
    path.write_text(text, encoding="utf-8")
    print("Removed PhoBoi landscape gate")
else:
    raise SystemExit("PhoBoi portrait landscape CSS block not found")
