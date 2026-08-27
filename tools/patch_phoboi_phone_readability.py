from pathlib import Path

p = Path('src/local_coop_mobile.cc')
s = p.read_text(encoding='utf-8')
marker = '/* PHOBOI_PHONE_READABILITY_V1 */'
if marker in s:
    print('PhoBoi phone readability already applied')
    raise SystemExit(0)

s = s.replace(
    '#video{position:absolute;inset:0;width:100%;height:100%;object-fit:contain;background:#000;image-rendering:auto;z-index:1}',
    '#video{position:absolute;inset:0;width:100%;height:100%;object-fit:contain;background:#000;image-rendering:pixelated;z-index:1;transform-origin:50% 50%;transition:transform .12s ease}\\n/* PHOBOI_PHONE_READABILITY_V1 */\\n#pad.view-zoom1 #video{transform:scale(1.28)}#pad.view-zoom2 #video{transform:scale(1.62)}'
)
s = s.replace(
    '.top{position:absolute;left:22%;right:22%;top:max(3px,env(safe-area-inset-top));height:22px;text-align:center;font-size:clamp(8px,1.6vw,12px);line-height:22px;',
    '.top{position:absolute;left:20%;right:20%;top:max(3px,env(safe-area-inset-top));height:28px;text-align:center;font-size:clamp(11px,2vw,16px);line-height:28px;'
)
s = s.replace(
    '.control-label{display:block;font-size:clamp(5px,1vw,8px);line-height:.95;',
    '.control-label{display:block;font-size:clamp(8px,1.45vw,11px);line-height:1.02;'
)
s = s.replace(
    '.stick-label{position:absolute;left:50%;top:7%;transform:translateX(-50%);z-index:2;font-size:clamp(6px,1vw,8px);',
    '.stick-label{position:absolute;left:50%;top:7%;transform:translateX(-50%);z-index:2;font-size:clamp(9px,1.4vw,12px);'
)
s = s.replace(
    '.btn{position:absolute;border-radius:50%;width:clamp(44px,10vh,64px);height:clamp(44px,10vh,64px);',
    '.btn{position:absolute;border-radius:50%;width:clamp(52px,11vh,72px);height:clamp(52px,11vh,72px);'
)
s = s.replace(
    '.small{position:absolute;width:clamp(46px,8vw,68px);height:clamp(30px,6vh,42px);',
    '.small{position:absolute;width:clamp(56px,9vw,82px);height:clamp(36px,7vh,48px);font-size:clamp(11px,1.7vw,15px);'
)

button_anchor = '<button class="small" id="back">PHOBOI<span class="control-label">PHONE / PIPBOY</span></button><button class="small" id="start">START<span class="control-label">MENU</span></button><button class="small" id="skill">RS CLICK<span class="control-label">SKILLDEX</span></button>'
button_new = button_anchor + '\n<button class="small" id="view" style="left:calc(50% - 41px);top:max(112px,calc(env(safe-area-inset-top) + 108px));width:82px">VIEW<span class="control-label">FIT / 1.3X / 1.6X</span></button>'
if button_anchor not in s:
    raise SystemExit('phone VIEW button anchor not found')
s = s.replace(button_anchor, button_new, 1)

js_anchor = "$('pin').value=new URLSearchParams(location.search).get('pin')||'';"
js_new = js_anchor + "\nlet phoneViewZoom=0;$('view').addEventListener('click',ev=>{ev.preventDefault();phoneViewZoom=(phoneViewZoom+1)%3;$('pad').classList.remove('view-zoom1','view-zoom2');if(phoneViewZoom===1)$('pad').classList.add('view-zoom1');if(phoneViewZoom===2)$('pad').classList.add('view-zoom2');updateStatus()});"
if js_anchor not in s:
    raise SystemExit('phone JS anchor not found')
s = s.replace(js_anchor, js_new, 1)

status_old = "$('top').textContent=`PLAYER ${slot+1} | CTRL ${controlMode} | VIDEO ${videoMode}${link}`;"
status_new = "$('top').textContent=`P${slot+1} | CTRL ${controlMode} | VIDEO ${videoMode} | VIEW ${phoneViewZoom===0?'FIT':phoneViewZoom===1?'1.3X':'1.6X'}${link}`;"
if status_old not in s:
    raise SystemExit('phone status anchor not found')
s = s.replace(status_old, status_new, 1)

p.write_text(s, encoding='utf-8')
print('Applied PhoBoi phone readability and zoom controls')
