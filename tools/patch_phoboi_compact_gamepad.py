#!/usr/bin/env python3
from pathlib import Path

path = Path('src/local_coop_mobile.cc')
text = path.read_text(encoding='utf-8')
marker = 'PHOBOI_COMPACT_GAMEPAD_V1'

if marker in text:
    print('Compact PhoBoi gamepad layout already applied')
    raise SystemExit(0)

start_token = 'const char* mobileControllerHtml()\n{'
end_token = '\n\nstd::string mobileReadRequest'
start = text.find(start_token)
end = text.find(end_token, start)
if start < 0 or end < 0:
    raise SystemExit('PhoBoi compact layout: controller HTML function boundary missing')

fn = text[start:end]

css_anchor = '</style>'
if css_anchor not in fn:
    raise SystemExit('PhoBoi compact layout: CSS anchor missing')

css = r'''
/* PHOBOI_COMPACT_GAMEPAD_V1
   Keep combat controls visible; move secondary/system controls into one drawer. */
.control-label{display:none!important}
#more{
 position:absolute!important;z-index:40!important;right:14px!important;top:14px!important;
 left:auto!important;width:52px!important;height:44px!important;min-width:52px!important;
 border-radius:22px!important;font-size:28px!important;line-height:28px!important;
 background:rgba(4,14,8,.76)!important;border:1px solid rgba(121,255,145,.85)!important;
 color:#effff2!important;backdrop-filter:blur(4px)!important
}
/* Secondary controls are completely out of the way during normal play. */
#lb,#rb,#back,#fps,#start,#skill,#view{
 opacity:0!important;visibility:hidden!important;pointer-events:none!important;
 transform:translateX(18px) scale(.92)!important;transition:opacity .12s ease,transform .12s ease!important
}
/* A narrow utility tray instead of buttons scattered across the video. */
#pad.phoboi-more #lb,#pad.phoboi-more #rb,#pad.phoboi-more #back,
#pad.phoboi-more #fps,#pad.phoboi-more #start,#pad.phoboi-more #skill,#pad.phoboi-more #view{
 opacity:.96!important;visibility:visible!important;pointer-events:auto!important;
 position:absolute!important;left:auto!important;right:12px!important;width:118px!important;
 min-width:118px!important;height:42px!important;min-height:42px!important;
 padding:4px 7px!important;margin:0!important;transform:none!important;
 border-radius:7px!important;background:rgba(4,14,8,.88)!important;
 border:1px solid rgba(121,255,145,.78)!important;font-size:13px!important;z-index:39!important
}
#pad.phoboi-more #start{top:68px!important}
#pad.phoboi-more #back{top:114px!important}
#pad.phoboi-more #skill{top:160px!important}
#pad.phoboi-more #fps{top:206px!important}
#pad.phoboi-more #view{top:252px!important}
#pad.phoboi-more #lb{top:298px!important}
#pad.phoboi-more #rb{top:344px!important}
#pad.phoboi-more #more{background:rgba(25,78,39,.94)!important}
/* Keep the always-needed controls readable without covering half the game. */
#lt,#rt{opacity:.82!important}
#ba,#bb,#bx,#by,.dpad,#ls,#rs{opacity:.78!important;transition:opacity .12s ease!important}
#ba:active,#bb:active,#bx:active,#by:active,#lt:active,#rt:active,.dpad button:active{opacity:1!important}
#ls:active,#rs:active{opacity:1!important}
/* Class/ready selection is contextual and can remain fully legible when shown. */
#classpick .control-label,#classpick .classrow .control-label{display:inline!important}
'''
fn = fn.replace(css_anchor, css + '\n' + css_anchor, 1)

# Put the drawer toggle immediately before the old secondary-control row.
controls_anchor = '<button class="small" id="lb">'
if controls_anchor not in fn:
    raise SystemExit('PhoBoi compact layout: secondary-control row anchor missing')
fn = fn.replace(
    controls_anchor,
    '<button class="small" id="more" aria-label="More controls">&#8942;</button>\n' + controls_anchor,
    1,
)

js_anchor = "const $=id=>document.getElementById(id);"
if js_anchor not in fn:
    raise SystemExit('PhoBoi compact layout: JS helper anchor missing')
js = r'''
// PHOBOI_COMPACT_GAMEPAD_DRAWER_V1
let phoboiMoreOpen=false;
function phoboiSetMore(open){
 phoboiMoreOpen=!!open;$('pad').classList.toggle('phoboi-more',phoboiMoreOpen);
 $('more').setAttribute('aria-expanded',phoboiMoreOpen?'true':'false');
}
$('more').addEventListener('pointerdown',ev=>{ev.preventDefault();ev.stopPropagation();phoboiSetMore(!phoboiMoreOpen)});
'''
fn = fn.replace(js_anchor, js_anchor + '\n' + js, 1)

# Close the tray after selecting a secondary action; the actual button's existing
# pointer handler still fires, so this does not change the controller mapping.
script_close_anchor = "[['ba',0],['bb',1],['bx',2],['by',3],['back',4],['start',6],['fps',7],['skill',8],['lb',9],['rb',10],['du',11],['dd',12],['dl',13],['dr',14]].forEach(x=>bindButton(x[0],x[1]));"
if script_close_anchor in fn:
    fn = fn.replace(
        script_close_anchor,
        script_close_anchor + "\n['back','start','fps','skill','lb','rb','view'].forEach(id=>$(id)?.addEventListener('pointerup',()=>phoboiSetMore(false)));",
        1,
    )

text = text[:start] + fn + text[end:]
path.write_text(text, encoding='utf-8')
print('Applied compact PhoBoi gamepad layout with utility drawer')
