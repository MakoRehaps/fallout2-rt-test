from pathlib import Path
import re

path = Path("src/local_coop_mobile.cc")
text = path.read_text(encoding="utf-8")

function_start_token = "const char* mobileControllerHtml()\n{"
next_function_token = "\n\nstd::string mobileReadRequest"
start = text.find(function_start_token)
if start == -1:
    raise SystemExit("PhoBoi controller HTML function not found")
end = text.find(next_function_token, start)
if end == -1:
    raise SystemExit("PhoBoi controller HTML function end not found")

function_text = text[start:end]
chunks = re.findall(r'R"PHOBOI\((.*?)\)PHOBOI"', function_text, flags=re.S)
if not chunks:
    raise SystemExit("PhoBoi controller HTML raw-string chunks not found")

body = "".join(chunks)
if "<!doctype html>" not in body or "</html>" not in body:
    raise SystemExit("PhoBoi reconstructed HTML is incomplete")

# MSVC C2026 is triggered by one oversized literal. Keep every individual raw
# literal comfortably below the compiler limit even as the phone UI grows.
max_chunk = 6000
safe_chunks = []
remaining = body
while remaining:
    if len(remaining) <= max_chunk:
        safe_chunks.append(remaining)
        break
    cut = remaining.rfind("\n", 0, max_chunk)
    if cut < max_chunk // 2:
        cut = max_chunk
    else:
        cut += 1
    safe_chunks.append(remaining[:cut])
    remaining = remaining[cut:]

lines = [
    "const char* mobileControllerHtml()",
    "{",
    "    // PHOBOI_CPP_FINAL_SPLIT_HTML_V2",
    "    // Final split runs after every CSS/JS phone patch so later additions",
    "    // cannot grow an earlier chunk back past MSVC's string-literal limit.",
    "    static const std::string html =",
    f'        std::string(R"PHOBOI({safe_chunks[0]})PHOBOI")',
]
for chunk in safe_chunks[1:]:
    lines.append(f'        + R"PHOBOI({chunk})PHOBOI"')
lines[-1] += ";"
lines.extend([
    "    return html.c_str();",
    "}",
])

replacement = "\n".join(lines)
text = text[:start] + replacement + text[end:]
path.write_text(text, encoding="utf-8")
print(f"Final-split PhoBoi controller HTML into {len(safe_chunks)} MSVC-safe literals")
