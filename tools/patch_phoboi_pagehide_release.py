#!/usr/bin/env python3
from pathlib import Path

p = Path('src/local_coop_mobile.cc')
s = p.read_text(encoding='utf-8')

marker = '// PHOBOI_NO_PAGEHIDE_RELEASE_V1'
if marker in s:
    print('PhoBoi pagehide release fix already patched')
    raise SystemExit(0)

old = "setInterval(send,16);addEventListener('pagehide',()=>{slot<0||navigator.sendBeacon('/release',new URLSearchParams({slot,token}))});"
new = "setInterval(send,16);" + marker + "\n// Do not release the claimed player slot on pagehide: browsers may fire pagehide\n// during fullscreen/orientation transitions. The server timeout owns stale sessions."
if old not in s:
    raise SystemExit('pagehide release anchor not found')

s = s.replace(old, new, 1)
p.write_text(s, encoding='utf-8')
print('Patched PhoBoi to preserve claimed slot across pagehide/fullscreen/orientation changes')
