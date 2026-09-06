#!/usr/bin/env python3
from pathlib import Path
import re
import runpy


def read(path):
    return Path(path).read_text(encoding="utf-8")


def write(path, text):
    Path(path).write_text(text, encoding="utf-8")


# ---------------------------------------------------------------------------
# FINAL INPUT MODEL
# P1 is the first PHYSICAL SDL/XInput controller. Keyboard/mouse remain usable
# only by explicit system/setup paths elsewhere (Esc, PhoBoi host setup, etc.).
# SDL virtual controllers created for phones are attached directly by PhoBoi
# and must never be rediscovered by the physical-controller scanner.
# ---------------------------------------------------------------------------
coop_path = "src/local_coop.h"
coop = read(coop_path)
if "COOP_P1_FIRST_PHYSICAL_XINPUT_V1" not in coop:
    anchor = '''inline void localCoopOpenController(int deviceIndex)\n{\n    if (!SDL_IsGameController(deviceIndex)) {\n        return;\n    }\n'''
    replacement = '''inline void localCoopOpenController(int deviceIndex)\n{\n    if (!SDL_IsGameController(deviceIndex)) {\n        return;\n    }\n\n    // COOP_P1_FIRST_PHYSICAL_XINPUT_V1\n    // PhoBoi phones use SDL_JoystickAttachVirtual and are assigned directly to\n    // their claimed slot by mobileAttachController. Never let the ordinary\n    // scanner count one of those virtual devices as another local player.\n    if (SDL_JoystickIsVirtual(deviceIndex)) {\n        return;\n    }\n'''
    if anchor not in coop:
        raise SystemExit("localCoopOpenController physical-device anchor missing")
    coop = coop.replace(anchor, replacement, 1)
write(coop_path, coop)


runtime_path = "src/local_coop_runtime.h"
runtime = read(runtime_path)

# No keyboard Tab gameplay hand swap for P1.
runtime = runtime.replace(
    '''        if (player.slot == 0) {\n            swapDown = swapDown || gPressedPhysicalKeys[SDL_SCANCODE_TAB];\n        }\n\n''',
    '''        // COOP_P1_CONTROLLER_ONLY_GAMEPLAY_V2\n        // Keyboard shortcuts do not become player gameplay input.\n\n''',
    1,
)

# No controller-less P1 combat fallback.
runtime = runtime.replace(
    '''            || (!hasController && player.slot != 0)) {\n''',
    '''            || !hasController) {\n''',
    1,
)

# Replace the old hybrid mouse/keyboard/controller arbitration completely.
pattern = re.compile(
    r'''inline void localCoopUpdateP1InputSource\(\)\n\{.*?\n\}\n\ninline void localCoopUpdateSharedCamera\(\)''',
    re.S,
)
match = pattern.search(runtime)
if match is None:
    raise SystemExit("P1 hybrid-input function boundary missing")
replacement = r'''inline void localCoopUpdateP1InputSource()
{
    // COOP_P1_CONTROLLER_ONLY_GAMEPLAY_V2
    // P1 is the first physical XInput/SDL game controller. Keyboard and mouse
    // are never a second player and never steal gameplay ownership from P1.
    LocalCoopPlayer& p1 = gLocalCoopPlayers[0];
    gLocalCoopP1ControllerActive = p1.connected && p1.controller != nullptr;
    if (!cursorIsHidden()) {
        mouseHideCursor();
    }
}

inline void localCoopUpdateSharedCamera()'''
runtime = runtime[:match.start()] + replacement + runtime[match.end():]

# D-pad-left is now PHOBOI, not the stock Pip-Boy. Start and D-pad-left both
# enter the same P1-owned PhoBoi system shell. Shared Skilldex remains separate.
if "COOP_PHOBOI_ONLY_MENU_V1" not in runtime:
    start = runtime.find("inline void localCoopProcessModalMenuInput()")
    end = runtime.find("\n// COOP_DOWNED_MEDICAL_V1", start)
    if start == -1 or end == -1:
        raise SystemExit("modal menu function boundary missing")
    block = runtime[start:end]

    block = block.replace(
        '''    bool pipboyModalActive = (currentGameMode & (GameMode::kPipboy | GameMode::kAutomap)) != 0;\n''',
        '''    // COOP_PHOBOI_ONLY_MENU_V1\n    // Stock Pip-Boy remains only as an internal compatibility backend for old\n    // scripts. It is not a co-op player-facing menu or controller destination.\n''',
        1,
    )

    old = '''        // COOP_P1_GLOBAL_UI_OWNER_V1\n        // COOP_P1_GLOBAL_UI_TOGGLE_V1\n        // COOP_SYSTEM_MENU_RUNTIME_V1\n        // COOP_PIPBOY_EDGE_TOGGLE_V2\n        // P1 owns the stock global Pip-Boy. Use the physical D-pad-left edge\n        // directly; no delayed re-arm timer survives across modal transitions.\n        bool canOwnGlobalUi = canOpen && slot == 0;\n        bool p1PipboyEdge = slot == 0 && pipboyDown && !runtime.pipboyWasDown;\n\n        if (p1PipboyEdge && pipboyModalActive) {\n            gLocalCoopModalControllerSlot = 0;\n            enqueueInputEvent(KEY_ESCAPE);\n            debugPrint("[PHOBOI INPUT] slot=0 global-ui=pipboy action=close\\n");\n        } else if (canOwnGlobalUi && p1PipboyEdge) {\n            gLocalCoopModalControllerSlot = 0;\n            enqueueInputEvent(KEY_LOWERCASE_P);\n            modalActive = true;\n            debugPrint("[PHOBOI INPUT] slot=0 global-ui=pipboy action=open\\n");\n        } else if (canOpen && skilldexDown && !runtime.skilldexWasDown) {\n'''
    new = '''        // COOP_PHOBOI_ONLY_MENU_V1\n        bool p1PhoBoiEdge = slot == 0 && pipboyDown && !runtime.pipboyWasDown;\n\n        if (canOpen && p1PhoBoiEdge) {\n            gLocalCoopModalControllerSlot = 0;\n            localCoopSystemMenuOpen();\n            modalActive = true;\n            debugPrint("[PHOBOI INPUT] slot=0 button=dpad-left action=open-phoboi\\n");\n        } else if (canOpen && skilldexDown && !runtime.skilldexWasDown) {\n'''
    if old in block:
        block = block.replace(old, new, 1)
    else:
        # The restored-working-path patch may have changed the exact Pip-Boy
        # implementation. Replace the section structurally between ownership
        # comments and the Skilldex branch.
        structural = re.compile(
            r'''        // COOP_P1_GLOBAL_UI_OWNER_V1.*?        \} else if \(canOpen && skilldexDown && !runtime\.skilldexWasDown\) \{''',
            re.S,
        )
        sm = structural.search(block)
        if sm is None:
            raise SystemExit("final Pip-Boy modal branch not found")
        new_structural = '''        // COOP_PHOBOI_ONLY_MENU_V1\n        bool p1PhoBoiEdge = slot == 0 && pipboyDown && !runtime.pipboyWasDown;\n\n        if (canOpen && p1PhoBoiEdge) {\n            gLocalCoopModalControllerSlot = 0;\n            localCoopSystemMenuOpen();\n            modalActive = true;\n            debugPrint("[PHOBOI INPUT] slot=0 button=dpad-left action=open-phoboi\\n");\n        } else if (canOpen && skilldexDown && !runtime.skilldexWasDown) {'''
        block = block[:sm.start()] + new_structural + block[sm.end():]

    runtime = runtime[:start] + block + runtime[end:]

write(runtime_path, runtime)


# ---------------------------------------------------------------------------
# PHOBOI SYSTEM MENU: remove the visible Pip-Boy entry. The old pipboyOpen()
# symbol stays in the engine only for scripts/save compatibility; players see
# one device/menu identity: PHOBOI.
# ---------------------------------------------------------------------------
system_path = "src/local_coop_system_menu.h"
system = read(system_path)
if "COOP_PHOBOI_ONLY_SYSTEM_MENU_V1" not in system:
    system = system.replace(
        '''// point while preserving the game's proven inventory/Pip-Boy/character/save\n// backends underneath, so quests and script-driven menus stay compatible.\n''',
        '''// point while preserving proven inventory/character/save backends.\n// COOP_PHOBOI_ONLY_SYSTEM_MENU_V1\n// The stock Pip-Boy is not exposed as a player-facing menu. PHOBOI owns that\n// identity and routes the party's co-op functions from this shell.\n''',
        1,
    )
    system = system.replace("    PipBoy,\n", "", 1)
    system = system.replace('        "PIP-BOY / MAP",\n', "", 1)
    pip_case = '''    case LocalCoopSystemMenuAction::PipBoy:\n        enqueueInputEvent(KEY_LOWERCASE_P);\n        break;\n'''
    system = system.replace(pip_case, "", 1)
write(system_path, system)


# ---------------------------------------------------------------------------
# READY ROOM: P1 is not a keyboard player. The first physical controller joins
# P1; additional physical controllers and phones fill the remaining slots.
# Keyboard remains only for Escape/system setup. Also remove visible Pip-Boy and
# legacy FPS tutorial language from the PhoBoi-facing guide.
# ---------------------------------------------------------------------------
group_path = "src/local_coop_group_room.h"
group = read(group_path)
if "COOP_READY_ROOM_CONTROLLER_ONLY_P1_V1" not in group:
    group = group.replace(
        '''    joined[0] = true;\n    // COOP_READY_ROOM_PREJOIN_TRANSFER_V1\n    gLocalCoopPrejoinedSlots.fill(false);\n    gLocalCoopPrejoinedSlots[0] = true;\n''',
        '''    // COOP_READY_ROOM_CONTROLLER_ONLY_P1_V1\n    // P1 exists as the story slot, but it is not considered joined until the\n    // first physical controller is actually attached. Keyboard/mouse do not\n    // allocate or represent a player slot.\n    joined[0] = gLocalCoopPlayers[0].connected && gLocalCoopPlayers[0].controller != nullptr;\n    // COOP_READY_ROOM_PREJOIN_TRANSFER_V1\n    gLocalCoopPrejoinedSlots.fill(false);\n    gLocalCoopPrejoinedSlots[0] = joined[0];\n''',
        1,
    )

    group = group.replace(
        '            "D-pad Left opens the Pip-Boy / personal device flow.",',
        '            "D-pad Left opens the PHOBOI co-op device/menu.",',
    )
    group = group.replace(
        '            "ISOMETRIC <-> FIRST PERSON",\n            "The first-person mode uses the same live Fallout map and simulation.",\n            "Walls are raycast and critters/objects are rendered as billboards.",\n            "Keyboard: F9 toggles camera mode.",\n            "Controller: L3 / Left Stick Click. Phone: tap FPS / ISO.",',
        '            "PHONE / PHOBOI",\n            "Phone players use the same live isometric Fallout world.",\n            "The game feed is rendered as an 800 x 600 PhoBoi surface.",\n            "Phone controls overlay the game and do not consume extra PC slots.",\n            "Each phone selects the class for its own claimed player slot.",',
    )
    group = group.replace(
        '"Press A on a controller/phone or ENTER on keyboard to unlock READY voting.",',
        '"Press A on a controller/phone to unlock READY voting.",',
    )
    group = group.replace(
        '"LEFT/RIGHT OR D-PAD = PAGE    A/ENTER = NEXT    START = JOIN",',
        '"D-PAD LEFT/RIGHT = PAGE    A = NEXT    START = JOIN",',
    )
    group = group.replace(
        '"P1 KEYBOARD FALLBACK: ARROWS = CLASS   ENTER = READY",',
        '"P1 = FIRST XINPUT CONTROLLER   PHONES / EXTRA XINPUT = REMAINING SLOTS",',
    )

    # Keep Escape as an intentional system/cancel keyboard function, but make
    # Enter/arrows inert for player/class/ready ownership.
    group = group.replace(
        '''        bool enterDown = keys != nullptr && keys[SDL_SCANCODE_RETURN] != 0;\n        bool escapeDown = keys != nullptr && keys[SDL_SCANCODE_ESCAPE] != 0;\n        bool keyLeft = keys != nullptr && keys[SDL_SCANCODE_LEFT] != 0;\n        bool keyRight = keys != nullptr && keys[SDL_SCANCODE_RIGHT] != 0;\n''',
        '''        // COOP_READY_ROOM_CONTROLLER_ONLY_P1_V1\n        bool enterDown = false;\n        bool escapeDown = keys != nullptr && keys[SDL_SCANCODE_ESCAPE] != 0;\n        bool keyLeft = false;\n        bool keyRight = false;\n''',
        1,
    )
write(group_path, group)


# ---------------------------------------------------------------------------
# PHONE PAGE: 800x600 logical PhoBoi stage, auto reload/resume after CONNECT,
# full usable viewport, and explicit phone-side class controls that pulse the
# claimed virtual controller's D-pad/Start so the ready room updates THAT slot.
# ---------------------------------------------------------------------------
mobile_path = "src/local_coop_mobile.cc"
mobile = read(mobile_path)
function_start_token = "const char* mobileControllerHtml()\n{"
next_function_token = "\n\nstd::string mobileReadRequest"
start = mobile.find(function_start_token)
end = mobile.find(next_function_token, start)
if start == -1 or end == -1:
    raise SystemExit("PhoBoi controller HTML function boundary missing")
function_text = mobile[start:end]
chunks = re.findall(r'R"PHOBOI\((.*?)\)PHOBOI"', function_text, flags=re.S)
if not chunks:
    raise SystemExit("PhoBoi controller HTML chunks missing")
html = "".join(chunks)

if "PHOBOI_PHONE_800X600_V1" not in html:
    html = html.replace(
        '<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">',
        '<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no,viewport-fit=cover">',
        1,
    )

    css = r'''
/* PHOBOI_PHONE_800X600_V1 */
html,body{width:100%!important;height:100%!important;min-height:100dvh!important;margin:0!important;overflow:hidden!important;background:#000!important}
#pad{
  --phoboi-ui:.833333;
  position:fixed!important;
  inset:auto!important;
  left:50%!important;
  top:50%!important;
  width:800px!important;
  height:600px!important;
  max-width:none!important;
  max-height:none!important;
  transform-origin:50% 50%!important;
  transform:translate(-50%,-50%) scale(var(--phoboi-stage-scale,1))!important;
  background:#000!important;
}
html.phoboi-phone-portrait #pad{
  transform:translate(-50%,-50%) rotate(90deg) scale(var(--phoboi-stage-scale,1))!important;
}
#video{position:absolute!important;inset:0!important;width:800px!important;height:600px!important;object-fit:fill!important;background:#000!important}
#classpick{position:absolute;left:190px;right:190px;top:68px;z-index:19;padding:8px 10px;background:rgba(4,14,8,.90);border:2px solid #79ff91;border-radius:8px;text-align:center;color:#effff2;font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Arial,sans-serif}
#classpick .classrow{display:flex;gap:8px;justify-content:center;align-items:center;margin-top:6px}
#classpick button{min-width:96px;min-height:42px;padding:5px 8px;font-size:14px;border:1px solid #79ff91;background:#102d19;color:#f2fff4}
#classpick .ready{min-width:120px;font-weight:800}
#className{display:block;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;font-weight:800}
'''.strip()
    if "</style>" not in html:
        raise SystemExit("PhoBoi closing style tag missing")
    html = html.replace("</style>", css + "\n</style>", 1)

    html = html.replace(
        '<div id="pad"><canvas id="video"',
        '<div id="pad"><canvas id="video"',
        1,
    )
    # Force an 800x600 backing canvas regardless of older generated defaults.
    html = re.sub(r'<canvas id="video" width="\d+" height="\d+">',
                  '<canvas id="video" width="800" height="600">', html, count=1)

    top_anchor = '<div class="top" id="top">PHOBOI CONTROLLER</div>'
    class_ui = '''<div class="top" id="top">PHOBOI CONTROLLER</div>\n<div id="classpick"><span id="className">PLAYER CLASS</span><div class="classrow"><button id="cprev">◀ CLASS</button><button id="cready" class="ready">READY</button><button id="cnext">CLASS ▶</button></div></div>'''
    if top_anchor not in html:
        raise SystemExit("PhoBoi top HUD anchor missing")
    html = html.replace(top_anchor, class_ui, 1)

    js_anchor = "const $=id=>document.getElementById(id);\n"
    if js_anchor not in html:
        raise SystemExit("PhoBoi JS helper anchor missing")
    layout_js = r'''// PHOBOI_PHONE_800X600_V1
function phoboi800Layout(){
 const vv=window.visualViewport;const w=Math.max(1,vv?vv.width:window.innerWidth),h=Math.max(1,vv?vv.height:window.innerHeight);
 const portrait=h>w;document.documentElement.classList.toggle('phoboi-phone-portrait',portrait);
 const logicalW=portrait?h:w,logicalH=portrait?w:h;
 const scale=Math.min(logicalW/800,logicalH/600);
 document.documentElement.style.setProperty('--phoboi-stage-scale',Math.max(.1,scale).toFixed(5));
}
phoboi800Layout();window.addEventListener('resize',phoboi800Layout,{passive:true});window.addEventListener('orientationchange',()=>setTimeout(phoboi800Layout,60),{passive:true});window.visualViewport?.addEventListener('resize',phoboi800Layout,{passive:true});
const phoboiScratch=document.createElement('canvas');const phoboiScratchCtx=phoboiScratch.getContext('2d',{alpha:false});
function phoboiPulse(bit){buttons|=(1<<bit);send(true);setTimeout(()=>{buttons&=~(1<<bit);send(true);setTimeout(phoboiRefreshClass,80)},90)}
$('cprev').addEventListener('click',e=>{e.preventDefault();phoboiPulse(13)});
$('cnext').addEventListener('click',e=>{e.preventDefault();phoboiPulse(14)});
$('cready').addEventListener('click',e=>{e.preventDefault();phoboiPulse(6)});
async function phoboiRefreshClass(){
 if(slot<0||!token)return;
 try{const r=await fetch(`/slotmeta?slot=${slot}&token=${token}`,{cache:'no-store'});const j=await r.json();if(!j.ok)return;
  $('className').textContent=`P${slot+1} CLASS: ${j.className||'SELECT'}`;
  if(j.locked)$('classpick').style.display='none';
 }catch(_){ }
}
setInterval(phoboiRefreshClass,500);
'''
    html = html.replace(js_anchor, js_anchor + layout_js, 1)

    # CONNECT is a deliberate refresh boundary. The claim is saved first, then
    # the new page resumes it. This matches Safari's reliable lifecycle instead
    # of trying to keep half-open sockets alive across the join transition.
    connect_pattern = re.compile(
        r'''\$\('connect'\)\.onclick=async\(\)=>\{try\{const body=.*?\}\};''',
        re.S,
    )
    cm = connect_pattern.search(html)
    if cm is None:
        raise SystemExit("PhoBoi CONNECT handler not found")
    new_connect = r'''$('connect').onclick=async()=>{try{
 const pin=$('pin').value;
 try{await document.documentElement.requestFullscreen?.()}catch(_){}
 const body=new URLSearchParams({slot:$('slot').value,pin});
 const r=await fetch('/claim',{method:'POST',body,cache:'no-store'});const j=await r.json();if(!j.ok)throw new Error(j.error||'Unable to connect');
 slot=j.slotIndex;token=j.token;const session={slot,token};sessionStorage.setItem('phoboiSession',JSON.stringify(session));try{localStorage.setItem('phoboiSession',JSON.stringify(session))}catch(_){}
 // PHOBOI_CONNECT_AUTO_REFRESH_V1
 location.replace(`${location.pathname}?pin=${encodeURIComponent(pin)}&connected=1&_=${Date.now()}`);
}catch(e){$('msg').textContent=e.message;phoboiRefreshSlots?.()}};'''
    html = html[:cm.start()] + new_connect + html[cm.end():]

    # When resume succeeds, make the 800x600 stage live immediately.
    html = html.replace(
        "slot=saved.slot;token=saved.token;$('join').style.display='none';$('pad').style.display='block';controlMode='STARTING';videoMode='STARTING';updateStatus();openSocket();openStream();",
        "slot=saved.slot;token=saved.token;$('join').style.display='none';$('pad').style.display='block';phoboi800Layout();controlMode='STARTING';videoMode='STARTING';updateStatus();openSocket();openStream();phoboiRefreshClass();",
        1,
    )

    # Keep the public canvas at 800x600 and scale each incoming frame into it.
    inflate_pattern = re.compile(
        r'''const c=\$\('video'\);if\(c\.width!==w\|\|c\.height!==h\)\{c\.width=w;c\.height=h\}\n c\.getContext\('2d',\{alpha:false\}\)\.putImageData\(new ImageData\(rgba,w,h\),0,0\);'''
    )
    if inflate_pattern.search(html):
        html = inflate_pattern.sub(
            "const c=$('video');if(c.width!==800||c.height!==600){c.width=800;c.height=600}\\n phoboiScratch.width=w;phoboiScratch.height=h;phoboiScratchCtx.putImageData(new ImageData(rgba,w,h),0,0);const ctx=c.getContext('2d',{alpha:false});ctx.imageSmoothingEnabled=true;ctx.clearRect(0,0,800,600);ctx.drawImage(phoboiScratch,0,0,w,h,0,0,800,600);",
            html,
            count=1,
        )

    new_function = '''const char* mobileControllerHtml()\n{\n    static const std::string html = R"PHOBOI(''' + html + ''')PHOBOI";\n    return html.c_str();\n}'''
    mobile = mobile[:start] + new_function + mobile[end:]

# Add slot metadata endpoint after token validation so the phone can display its
# OWN ready-room class without mutating P1.
if "PHOBOI_SLOT_META_ENDPOINT_V1" not in mobile:
    token_anchor = '''    uint32_t token = mobileUnsignedValue(values, "token", 0);\n    if (!state.claimed.load() || token == 0 || token != state.token.load()) {\n        mobileSendResponse(client, "403 Forbidden", "application/json", "{\\\"ok\\\":false,\\\"error\\\":\\\"Session expired\\\"}");\n        return;\n    }\n\n'''
    if token_anchor not in mobile:
        raise SystemExit("PhoBoi post-claim token validation anchor missing")
    endpoint = r'''    // PHOBOI_SLOT_META_ENDPOINT_V1
    if (method == "GET" && route == "/slotmeta") {
        const LocalCoopPlayer& player = gLocalCoopPlayers[slot];
        int archetype = std::max(0, std::min(player.archetype, kLocalCoopArchetypeCount - 1));
        std::ostringstream body;
        body << "{\"ok\":true,\"player\":" << slot + 1
             << ",\"classIndex\":" << archetype
             << ",\"className\":\"" << kLocalCoopArchetypeNames[archetype] << "\""
             << ",\"locked\":" << (player.slotLocked ? "true" : "false") << "}";
        mobileSendResponse(client, "200 OK", "application/json", body.str());
        return;
    }

'''
    mobile = mobile.replace(token_anchor, token_anchor + endpoint, 1)

# Fixed 800x600 stream. Crop the host capture to 4:3 before scaling so wide PC
# output is not geometrically stretched on the phone.
if "PHOBOI_STREAM_800X600_V1" not in mobile:
    ladder_pattern = re.compile(
        r'''    // PHOBOI_READABLE_STREAM_LADDER_V2 PHOBOI_NATIVE_TEXT_STREAM_V1\n.*?    if \(captureSurface->w < width \|\| captureSurface->h < height\) \{\n        width = captureSurface->w;\n        height = captureSurface->h;\n    \}\n''',
        re.S,
    )
    lm = ladder_pattern.search(mobile)
    if lm is not None:
        fixed = '''    // PHOBOI_STREAM_800X600_V1\n    int width = 800;\n    int height = 600;\n    int fps = 3;\n    // Resolution is fixed for phones; only FPS yields to weak links.\n    if (worstRtt <= 55 && worstJitter <= 15 && worstInterval <= 26) fps = 8;\n    else if (worstRtt <= 140 && worstJitter <= 40 && worstInterval <= 55) fps = 6;\n    else if (worstRtt <= 300 && worstJitter <= 100 && worstInterval <= 120) fps = 4;\n'''
        mobile = mobile[:lm.start()] + fixed + mobile[lm.end():]
    else:
        # Base-source fallback before readability patches are materialized.
        base_pattern = re.compile(
            r'''    int width = 320;\n    int height = 180;\n    int fps = 8;.*?    \}\n\n    uint64_t previous''',
            re.S,
        )
        bm = base_pattern.search(mobile)
        if bm is None:
            raise SystemExit("PhoBoi stream quality ladder not found")
        fixed = '''    // PHOBOI_STREAM_800X600_V1\n    int width = 800;\n    int height = 600;\n    int fps = 3;\n    if (worstRtt <= 55 && worstJitter <= 15 && worstInterval <= 26) fps = 8;\n    else if (worstRtt <= 140 && worstJitter <= 40 && worstInterval <= 55) fps = 6;\n    else if (worstRtt <= 300 && worstJitter <= 100 && worstInterval <= 120) fps = 4;\n\n    uint64_t previous'''
        mobile = mobile[:bm.start()] + fixed + mobile[bm.end():]

    blit_old = '''    SDL_Rect dst { 0, 0, width, height };\n    if (SDL_BlitScaled(captureSurface, nullptr, scaled, &dst) != 0) {\n'''
    blit_new = '''    SDL_Rect dst { 0, 0, width, height };\n    // PHOBOI_STREAM_800X600_V1: center-crop to 4:3, then scale.\n    SDL_Rect srcRect { 0, 0, captureSurface->w, captureSurface->h };\n    if (captureSurface->w * 3 > captureSurface->h * 4) {\n        srcRect.w = captureSurface->h * 4 / 3;\n        srcRect.x = (captureSurface->w - srcRect.w) / 2;\n    } else if (captureSurface->w * 3 < captureSurface->h * 4) {\n        srcRect.h = captureSurface->w * 3 / 4;\n        srcRect.y = (captureSurface->h - srcRect.h) / 2;\n    }\n    if (SDL_BlitScaled(captureSurface, &srcRect, scaled, &dst) != 0) {\n'''
    if blit_old not in mobile:
        raise SystemExit("PhoBoi stream blit anchor missing")
    mobile = mobile.replace(blit_old, blit_new, 1)

write(mobile_path, mobile)
# Re-split HTML only after every phone edit.
runpy.run_path("tools/patch_phoboi_msvc_final_split.py", run_name="__main__")


# ---------------------------------------------------------------------------
# MAIN MENU: reuse Fallout's proven background/button art, but turn the right
# side into a PhoBoi co-op terminal instead of presenting the build as stock UI.
# ---------------------------------------------------------------------------
mainmenu_path = "src/mainmenu.cc"
mainmenu = read(mainmenu_path)
if "PHOBOI_MAIN_MENU_SHELL_V1" not in mainmenu:
    anchor = '''static void coopMainMenuDrawSelection()\n{\n'''
    if anchor not in mainmenu:
        raise SystemExit("main menu selection function anchor missing")
    shell = r'''// PHOBOI_MAIN_MENU_SHELL_V1
static void coopMainMenuDrawPhoBoiShell()
{
    if (gMainMenuWindow == -1) return;
    constexpr int left = 278;
    constexpr int top = 54;
    constexpr int width = 334;
    constexpr int height = 330;
    windowFill(gMainMenuWindow, left, top, width, height, _colorTable[0]);
    windowDrawLine(gMainMenuWindow, left, top, left + width, top, _colorTable[992]);
    windowDrawLine(gMainMenuWindow, left, top + height, left + width, top + height, _colorTable[992]);
    windowDrawLine(gMainMenuWindow, left, top, left, top + height, _colorTable[992]);
    windowDrawLine(gMainMenuWindow, left + width, top, left + width, top + height, _colorTable[992]);
    windowDrawText(gMainMenuWindow, "NOKIA BLACKBERRY PHOBOI", width - 28, left + 14, top + 20, _colorTable[32747]);
    windowDrawText(gMainMenuWindow, "FALLOUT UNIFIED CO-OP", width - 28, left + 14, top + 50, _colorTable[992]);
    windowDrawText(gMainMenuWindow, "P1  FIRST XINPUT CONTROLLER", width - 28, left + 14, top + 102, _colorTable[992]);
    windowDrawText(gMainMenuWindow, "P2-P4  EXTRA XINPUT / PHONES", width - 28, left + 14, top + 130, _colorTable[992]);
    windowDrawText(gMainMenuWindow, "PHONE VIEW  800 x 600", width - 28, left + 14, top + 178, _colorTable[992]);
    windowDrawText(gMainMenuWindow, "SHARED BAG + SHARED SKILLDEX", width - 28, left + 14, top + 206, _colorTable[992]);
    windowDrawText(gMainMenuWindow, "D-PAD LEFT / START = PHOBOI", width - 28, left + 14, top + 254, _colorTable[32747]);
    windowDrawText(gMainMenuWindow, "A SELECT   B BACK", width - 28, left + 14, top + 282, _colorTable[992]);
}

'''
    mainmenu = mainmenu.replace(anchor, shell + anchor, 1)

    # Draw after stock menu labels so the PhoBoi terminal wins its region.
    draw_anchor = '''    fontSetCurrent(oldFont);\n\n    gMainMenuWindowInitialized = true;\n'''
    draw_new = '''    fontSetCurrent(oldFont);\n\n    coopMainMenuDrawPhoBoiShell();\n\n    gMainMenuWindowInitialized = true;\n'''
    if draw_anchor not in mainmenu:
        raise SystemExit("main menu post-label draw anchor missing")
    mainmenu = mainmenu.replace(draw_anchor, draw_new, 1)

    # Refresh the shell after selection redraw as well.
    sel_anchor = '''    windowDrawText(gMainMenuWindow,\n        ">",\n        markerW,\n        markerX,\n        markerY + gCoopMainMenuSelection * 41,\n        _colorTable[32747]);\n    windowRefresh(gMainMenuWindow);\n'''
    sel_new = '''    windowDrawText(gMainMenuWindow,\n        ">",\n        markerW,\n        markerX,\n        markerY + gCoopMainMenuSelection * 41,\n        _colorTable[32747]);\n    coopMainMenuDrawPhoBoiShell();\n    windowRefresh(gMainMenuWindow);\n'''
    if sel_anchor not in mainmenu:
        raise SystemExit("main menu selection redraw anchor missing")
    mainmenu = mainmenu.replace(sel_anchor, sel_new, 1)
write(mainmenu_path, mainmenu)


# Visible tutorial/system-menu language must be PhoBoi-only.
for filename in (group_path, system_path, mainmenu_path):
    visible = read(filename)
    if "PIP-BOY / MAP" in visible or "opens the Pip-Boy" in visible:
        raise SystemExit(f"visible stock Pip-Boy wording survived in {filename}")

# Required behavior markers.
for filename, marker in (
    (coop_path, "COOP_P1_FIRST_PHYSICAL_XINPUT_V1"),
    (runtime_path, "COOP_P1_CONTROLLER_ONLY_GAMEPLAY_V2"),
    (runtime_path, "COOP_PHOBOI_ONLY_MENU_V1"),
    (system_path, "COOP_PHOBOI_ONLY_SYSTEM_MENU_V1"),
    (group_path, "COOP_READY_ROOM_CONTROLLER_ONLY_P1_V1"),
    (mobile_path, "PHOBOI_PHONE_800X600_V1"),
    (mobile_path, "PHOBOI_CONNECT_AUTO_REFRESH_V1"),
    (mobile_path, "PHOBOI_SLOT_META_ENDPOINT_V1"),
    (mobile_path, "PHOBOI_STREAM_800X600_V1"),
    (mainmenu_path, "PHOBOI_MAIN_MENU_SHELL_V1"),
):
    if marker not in read(filename):
        raise SystemExit(f"missing final PhoBoi marker {marker} in {filename}")

print("Installed controller-only P1, 800x600 phone flow, phone class controls, and PhoBoi-only menus")
