#!/usr/bin/env python3
from pathlib import Path

p = Path('src/local_coop_mobile.cc')
s = p.read_text(encoding='utf-8')

MARKER = 'PHOBOI_FULL_CONTROL_LABELS_V1'
if MARKER in s:
    print('PhoBoi labels already current')
    raise SystemExit(0)

css_anchor = '.top{position:absolute;left:0;right:0;top:7px;text-align:center;font-size:13px}\n'
css_extra = '''.control-label{display:block;font-size:9px;line-height:1.05;color:#bfffc8;font-weight:normal;margin-top:2px;white-space:normal}\n.stick-label{position:absolute;left:50%;top:8%;transform:translateX(-50%);z-index:2;font-size:10px;line-height:1.05;text-align:center;color:#d7ffd7;background:#07100bcc;padding:3px 6px;border:1px solid #3a8d4d;border-radius:6px;pointer-events:none}\n'''
if css_anchor not in s:
    raise SystemExit('PhoBoi CSS anchor not found')
s = s.replace(css_anchor, css_anchor + css_extra, 1)

old = '''<button class="small" id="lb">LB</button><button class="small" id="lt">LT</button>\n<button class="small" id="rb">RB</button><button class="small" id="rt">RT</button>\n<button class="small" id="back">PHOBOI</button><button class="small" id="start">START</button><button class="small" id="skill">SKILLDEX</button>\n<div class="stick" id="ls"><div class="nub"></div></div><div class="stick" id="rs"><div class="nub"></div></div>\n<button class="btn" id="ba">A</button><button class="btn" id="bb">B</button><button class="btn" id="bx">X</button><button class="btn" id="by">Y</button>\n<div class="dpad"><button class="du" id="du">▲</button><button class="dl" id="dl">◀</button><button class="dr" id="dr">▶</button><button class="dd" id="dd">▼</button></div>'''
new = '''<!-- PHOBOI_FULL_CONTROL_LABELS_V1 -->\n<button class="small" id="lb">LB<span class="control-label">RUN</span></button><button class="small" id="lt">LT<span class="control-label">HEX AIM</span></button>\n<button class="small" id="rb">RB<span class="control-label">ALT ATTACK</span></button><button class="small" id="rt">RT<span class="control-label">ATTACK</span></button>\n<button class="small" id="back">PHOBOI<span class="control-label">PHONE / PIPBOY</span></button><button class="small" id="start">START<span class="control-label">MENU</span></button><button class="small" id="skill">RS CLICK<span class="control-label">SKILLDEX</span></button>\n<div class="stick" id="ls"><div class="stick-label">LEFT STICK<br>MOVE</div><div class="nub"></div></div><div class="stick" id="rs"><div class="stick-label">RIGHT STICK<br>AIM / CURSOR</div><div class="nub"></div></div>\n<button class="btn" id="ba">A<span class="control-label">USE / TALK / PICKUP</span></button><button class="btn" id="bb">B<span class="control-label">CANCEL / BACK</span></button><button class="btn" id="bx">X<span class="control-label">RELOAD</span></button><button class="btn" id="by">Y<span class="control-label">SWAP HAND</span></button>\n<div class="dpad"><button class="du" id="du">▲<span class="control-label">MENU UP</span></button><button class="dl" id="dl">◀<span class="control-label">FIRST AID</span></button><button class="dr" id="dr">▶<span class="control-label">DOCTOR</span></button><button class="dd" id="dd">▼<span class="control-label">MENU DOWN</span></button></div>'''
if old not in s:
    raise SystemExit('PhoBoi control markup anchor not found')
s = s.replace(old, new, 1)

p.write_text(s, encoding='utf-8')
print('PhoBoi full control labels applied')
