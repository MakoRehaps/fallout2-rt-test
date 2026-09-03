from pathlib import Path

path = Path("src/local_coop_mobile.cc")
text = path.read_text(encoding="utf-8")

# iPhone Safari must never be gated by orientation. Keep the compatibility
# markers used by older workflow validators, but make the rule unconditional:
# there is no rotate overlay and all controls/video stay visible in both
# portrait and landscape.
old_gate = "@media (orientation:portrait){#pad:before{content:'ROTATE PHONE TO LANDSCAPE';position:absolute;inset:0;display:flex;align-items:center;justify-content:center;background:#07100b;color:#ffd56a;font-size:18px;z-index:15}#video,.stick,.btn,.small,.dpad,.top{visibility:hidden}}"
legacy_nonblocking = "/* PHOBOI_NONBLOCKING_LANDSCAPE_HINT_V1 */\\n@media (orientation:portrait){#pad:before{content:'ROTATE PHONE TO LANDSCAPE';position:absolute;left:50%;top:max(4px,env(safe-area-inset-top));transform:translateX(-50%);display:block;width:max-content;max-width:92vw;padding:5px 10px;border:1px solid #ffd56a;border-radius:6px;background:#07100bcc;color:#ffd56a;font-size:12px;line-height:1.1;z-index:15;pointer-events:none}#video,.stick,.btn,.small,.dpad,.top{visibility:visible}}"
v2_no_gate_escaped = "/* PHOBOI_NONBLOCKING_LANDSCAPE_HINT_V1 PHOBOI_NO_LANDSCAPE_GATE_V2 */\\n@media (orientation:portrait){#pad:before{content:none!important;display:none!important}#video,.stick,.btn,.small,.dpad,.top{visibility:visible!important}}"
v2_no_gate_real = "/* PHOBOI_NONBLOCKING_LANDSCAPE_HINT_V1 PHOBOI_NO_LANDSCAPE_GATE_V2 */\n@media (orientation:portrait){#pad:before{content:none!important;display:none!important}#video,.stick,.btn,.small,.dpad,.top{visibility:visible!important}}"

no_gate = """/* PHOBOI_NONBLOCKING_LANDSCAPE_HINT_V1 PHOBOI_NO_LANDSCAPE_GATE_V2 PHOBOI_IOS_ORIENTATION_AGNOSTIC_V3 */
#pad::before{content:none!important;display:none!important;visibility:hidden!important;pointer-events:none!important}
#video,.stick,.btn,.small,.dpad,.top{visibility:visible!important}
@media (orientation:portrait){#pad::before{content:none!important;display:none!important;visibility:hidden!important}#video,.stick,.btn,.small,.dpad,.top{visibility:visible!important}}
""".strip()

if "PHOBOI_IOS_ORIENTATION_AGNOSTIC_V3" not in text:
    replaced = False
    for candidate in (old_gate, legacy_nonblocking, v2_no_gate_escaped, v2_no_gate_real):
        if candidate in text:
            text = text.replace(candidate, no_gate, 1)
            replaced = True
            break
    if not replaced:
        # Last-resort safety: if an old rotate phrase survived in a slightly
        # different CSS block, fail the build instead of shipping it again.
        if "ROTATE PHONE TO LANDSCAPE" in text:
            raise SystemExit("Unrecognized PhoBoi rotate-phone gate still present")
        style_end = "</style>"
        if style_end not in text:
            raise SystemExit("PhoBoi style closing tag not found")
        text = text.replace(style_end, no_gate + "\n" + style_end, 1)
    print("Made PhoBoi portrait/landscape orientation agnostic")
else:
    print("PhoBoi orientation-agnostic CSS already applied")

# iPhone Safari does not provide a dependable orientation lock for this kind of
# controller page. Never request landscape; portrait and landscape are both
# supported layouts.
orientation_lock = " if(screen.orientation?.lock)screen.orientation.lock('landscape').catch(()=>{});\n"
if orientation_lock in text:
    text = text.replace(orientation_lock, " // PHOBOI_IOS_NO_ORIENTATION_LOCK_V1\n", 1)
elif "PHOBOI_IOS_NO_ORIENTATION_LOCK_V1" not in text:
    # Accept formatting without the leading space too.
    orientation_lock = "if(screen.orientation?.lock)screen.orientation.lock('landscape').catch(()=>{});\n"
    if orientation_lock in text:
        text = text.replace(orientation_lock, "// PHOBOI_IOS_NO_ORIENTATION_LOCK_V1\n", 1)
    else:
        raise SystemExit("PhoBoi orientation-lock call not found")

# Edge-to-edge Safari layout with safe-area env() values working correctly.
viewport_old = '<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">'
viewport_new = '<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no,viewport-fit=cover">'
if viewport_old in text:
    text = text.replace(viewport_old, viewport_new, 1)

# Safari can restore a controller page from its back/forward page cache. If that
# happens, force one real reload so an iPhone cannot keep an old rotate-gated
# HTML document alive after the host has been updated.
old_lifecycle = "['online','pageshow','orientationchange'].forEach(name=>window.addEventListener(name,()=>{if(slot>=0){openSocket();openStream()}}));"
new_lifecycle = """// PHOBOI_IOS_BFCACHE_REFRESH_V1
['online','orientationchange'].forEach(name=>window.addEventListener(name,()=>{if(slot>=0){openSocket();openStream()}}));
window.addEventListener('pageshow',ev=>{if(ev.persisted){location.reload();return}if(slot>=0){openSocket();openStream()}});"""
if old_lifecycle in text:
    text = text.replace(old_lifecycle, new_lifecycle, 1)
elif "PHOBOI_IOS_BFCACHE_REFRESH_V1" not in text:
    raise SystemExit("PhoBoi lifecycle handler anchor not found")

# Strengthen the already-present no-store response for mobile Safari and other
# WebKit clients. This also makes intermediary proxies less likely to replay an
# old controller HTML page.
cache_old = '             << "Cache-Control: no-store\\r\\n"\n'
cache_new = '''             // PHOBOI_IOS_CACHE_BUST_V1
             << "Cache-Control: no-store, no-cache, must-revalidate, max-age=0\\r\\n"
             << "Pragma: no-cache\\r\\n"
             << "Expires: 0\\r\\n"
'''
if cache_old in text:
    text = text.replace(cache_old, cache_new, 1)
elif "PHOBOI_IOS_CACHE_BUST_V1" not in text:
    raise SystemExit("PhoBoi Cache-Control response anchor not found")

# There must be no visible rotate instruction left anywhere in the compiled
# page. Treat any remaining copy as a build-breaking regression.
if "ROTATE PHONE TO LANDSCAPE" in text:
    raise SystemExit("PhoBoi rotate-phone text survived orientation fix")

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

if "PHOBOI_TRANSPARENT_CONTROLLER_OVERLAY_V1" not in text:
    anchor = "</style>"
    if anchor not in text:
        raise SystemExit("PhoBoi style closing tag not found")
    text = text.replace(anchor, overlay_css + "\n" + anchor, 1)
    print("Applied transparent PhoBoi controller overlay")
else:
    print("PhoBoi transparent controller overlay already applied")

# MSVC rejects very large individual string literals (C2026). The embedded
# controller page has grown beyond that limit, so materialize it as several
# runtime-concatenated raw-string chunks instead of one giant raw literal.
cpp_split_marker = "PHOBOI_CPP_SPLIT_HTML_V1"
if cpp_split_marker not in text:
    start_token = '    return R"PHOBOI('
    end_token = ')PHOBOI";'
    start = text.find(start_token)
    if start == -1:
        raise SystemExit("PhoBoi controller HTML raw-string start not found")
    body_start = start + len(start_token)
    end = text.find(end_token, body_start)
    if end == -1:
        raise SystemExit("PhoBoi controller HTML raw-string end not found")

    body = text[body_start:end]
    chunks = []
    max_chunk = 12000
    while body:
        if len(body) <= max_chunk:
            chunks.append(body)
            break
        cut = body.rfind("\n", 0, max_chunk)
        if cut < max_chunk // 2:
            cut = max_chunk
        else:
            cut += 1
        chunks.append(body[:cut])
        body = body[cut:]

    lines = [
        f"    // {cpp_split_marker}",
        "    static const std::string html =",
        f'        std::string(R"PHOBOI({chunks[0]})PHOBOI")',
    ]
    for chunk in chunks[1:]:
        lines.append(f'        + R"PHOBOI({chunk})PHOBOI"')
    lines[-1] += ";"
    lines.append("    return html.c_str();")
    replacement = "\n".join(lines)
    text = text[:start] + replacement + text[end + len(end_token):]
    print(f"Split PhoBoi controller HTML into {len(chunks)} MSVC-safe literals")
else:
    print("PhoBoi controller HTML already split for MSVC")

path.write_text(text, encoding="utf-8")
