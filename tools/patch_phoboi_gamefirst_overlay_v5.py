from pathlib import Path

p = Path('src/local_coop_mobile.cc')
s = p.read_text(encoding='utf-8')
marker = '/* PHOBOI_GAMEFIRST_OVERLAY_V5 */'
if marker in s:
    print('PhoBoi game-first overlay v5 already applied')
    raise SystemExit(0)

start = s.find('/* PHOBOI_VIDEO_LAYOUT_V4 */')
end_marker = '@media (orientation:portrait){#pad:before{content:\'ROTATE PHONE TO LANDSCAPE\''
end = s.find(end_marker, start)
if start == -1 or end == -1:
    raise SystemExit('PhoBoi v4 CSS block not found')
end = s.find("}\n", end)
if end == -1:
    raise SystemExit('PhoBoi v4 CSS end not found')
end += 2

css = '''/* PHOBOI_GAMEFIRST_OVERLAY_V5 */
*{box-sizing:border-box;touch-action:none;-webkit-user-select:none;user-select:none}
html,body{width:100%;height:100%;margin:0;overflow:hidden;background:#000;color:#9cff9c;font-family:monospace}
body{min-height:100dvh}
#join{position:fixed;inset:0;display:flex;align-items:center;justify-content:center;background:#07100b;z-index:20;padding:max(12px,env(safe-area-inset-top)) max(12px,env(safe-area-inset-right)) max(12px,env(safe-area-inset-bottom)) max(12px,env(safe-area-inset-left))}
.panel{border:2px solid #4dbd68;padding:20px;width:min(92vw,420px);box-shadow:0 0 28px #174d29}
h1{margin:0 0 14px;font-size:25px}.row{display:flex;gap:8px;margin-top:10px}input,select,button{font:inherit;color:#d7ffd7;background:#102218;border:1px solid #4dbd68;padding:8px}
input{width:100%}select{flex:1}button{font-weight:bold}.status{min-height:22px;margin-top:10px;color:#ffd56a}
#pad{display:none;position:fixed;inset:0;background:#000;overflow:hidden}
#video{position:absolute;inset:0;width:100%;height:100%;object-fit:contain;background:#000;image-rendering:auto;z-index:1}
.top{position:absolute;left:22%;right:22%;top:max(3px,env(safe-area-inset-top));height:22px;text-align:center;font-size:clamp(8px,1.6vw,12px);line-height:22px;z-index:9;color:#eaffea;background:#07100b99;border-radius:8px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;pointer-events:none}
.control-label{display:block;font-size:clamp(5px,1vw,8px);line-height:.95;color:#d6ffdc;font-weight:normal;margin-top:0;white-space:normal;pointer-events:none}
.stick-label{position:absolute;left:50%;top:7%;transform:translateX(-50%);z-index:2;font-size:clamp(6px,1vw,8px);line-height:1;text-align:center;color:#d7ffd7;background:#07100b88;padding:2px 4px;border-radius:5px;pointer-events:none}
.stick{position:absolute;width:clamp(104px,25vh,160px);height:clamp(104px,25vh,160px);border:2px solid #79ff91aa;border-radius:50%;background:#07100b44;bottom:max(8px,env(safe-area-inset-bottom));z-index:7;opacity:.78}
#ls{left:max(8px,env(safe-area-inset-left))}#rs{right:calc(max(8px,env(safe-area-inset-right)) + clamp(130px,28vh,188px))}.nub{position:absolute;width:36%;height:36%;left:32%;top:32%;border-radius:50%;background:#68dd82bb}
.btn{position:absolute;border-radius:50%;width:clamp(44px,10vh,64px);height:clamp(44px,10vh,64px);padding:1px;background:#102d19bb;border-color:#79ff91aa;z-index:8;opacity:.86}
#ba{right:max(8px,env(safe-area-inset-right));bottom:clamp(52px,12vh,78px)}
#bb{right:clamp(56px,13vh,84px);bottom:max(6px,env(safe-area-inset-bottom))}
#bx{right:clamp(56px,13vh,84px);bottom:clamp(104px,24vh,156px)}
#by{right:clamp(108px,25vh,164px);bottom:clamp(52px,12vh,78px)}
.small{position:absolute;width:clamp(46px,8vw,68px);height:clamp(30px,6vh,42px);border-radius:8px;padding:1px;background:#102d19bb;border-color:#79ff91aa;z-index:8;opacity:.86}
#lb{left:max(6px,env(safe-area-inset-left));top:max(4px,env(safe-area-inset-top))}#lt{left:clamp(58px,9vw,86px);top:max(4px,env(safe-area-inset-top))}#rb{right:clamp(58px,9vw,86px);top:max(4px,env(safe-area-inset-top))}#rt{right:max(6px,env(safe-area-inset-right));top:max(4px,env(safe-area-inset-top))}
#back{left:calc(50% - 90px);top:max(30px,calc(env(safe-area-inset-top) + 28px))}#start{right:calc(50% - 90px);top:max(30px,calc(env(safe-area-inset-top) + 28px))}#skill{left:calc(50% - 32px);top:max(66px,calc(env(safe-area-inset-top) + 66px));width:64px}
.dpad{position:absolute;left:calc(max(8px,env(safe-area-inset-left)) + clamp(112px,27vh,172px));bottom:max(8px,env(safe-area-inset-bottom));display:grid;grid-template-columns:repeat(3,34px);grid-template-rows:repeat(3,34px);gap:1px;z-index:8;opacity:.82}
.dpad button{padding:0;min-width:0;min-height:0;background:#102d19bb;border-color:#79ff91aa}.du{grid-column:2}.dl{grid-column:1;grid-row:2}.dr{grid-column:3;grid-row:2}.dd{grid-column:2;grid-row:3}
@media (max-height:390px){.stick{width:96px;height:96px}#rs{right:124px}.btn{width:46px;height:46px}#ba{bottom:48px}#bb{right:50px}#bx{right:50px;bottom:96px}#by{right:98px;bottom:48px}.dpad{left:112px;grid-template-columns:repeat(3,30px);grid-template-rows:repeat(3,30px)}.small{height:28px}.control-label{display:none}#skill{top:58px}}
@media (orientation:portrait){#pad:before{content:'ROTATE PHONE TO LANDSCAPE';position:absolute;inset:0;display:flex;align-items:center;justify-content:center;background:#07100b;color:#ffd56a;font-size:18px;z-index:15}#video,.stick,.btn,.small,.dpad,.top{visibility:hidden}}
'''

s = s[:start] + css + s[end:]
p.write_text(s, encoding='utf-8')
print('PhoBoi game-first full-screen video overlay v5 applied')
