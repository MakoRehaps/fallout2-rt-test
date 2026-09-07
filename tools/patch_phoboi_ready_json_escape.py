from pathlib import Path

# Kept as a dedicated repair/validation pass so the browser handshake cannot
# regress to over-escaped JSON during later materializer runs or Windows builds.
path = Path("src/local_coop_mobile.cc")
text = path.read_text(encoding="utf-8")

server_anchor = "    // PHOBOI_NO_REFRESH_JOIN_V1\n    // The browser must not guess when the game thread has finished creating its\n"
resume_anchor = '    if (method == "GET" && route == "/resume") {'

start = text.find(server_anchor)
if start < 0:
    raise SystemExit("PhoBoi ready JSON repair: server handshake marker not found")
end = text.find(resume_anchor, start)
if end < 0:
    raise SystemExit("PhoBoi ready JSON repair: resume anchor not found")

block = text[start:end]
# The first materializer accidentally emitted three C++ backslashes before each
# JSON quote. That produces literal backslashes on the wire, so response.json()
# fails even though the C++ compiles. Keep exactly one C++ escape before quotes.
fixed = block.replace(r'\\\"', r'\"')
if fixed != block:
    text = text[:start] + fixed + text[end:]
    path.write_text(text, encoding="utf-8")
    print("Repaired PhoBoi /ready JSON escaping")
else:
    print("PhoBoi /ready JSON escaping already correct")

check = path.read_text(encoding="utf-8")[start:path.read_text(encoding="utf-8").find(resume_anchor, start)]
if r'\\\"' in check:
    raise SystemExit("PhoBoi ready JSON repair: over-escaped JSON remains")
if r'{\"ok\":true,\"ready\":' not in check:
    raise SystemExit("PhoBoi ready JSON repair: valid ready JSON signature missing")
