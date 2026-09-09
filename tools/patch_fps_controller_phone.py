from pathlib import Path

FPS = Path('src/local_coop_fps.h')
MOBILE = Path('src/local_coop_mobile.cc')

fps = FPS.read_text(encoding='utf-8')
mobile = MOBILE.read_text(encoding='utf-8')

old = 'inline bool gLocalCoopFpsToggleWasDown = false;\ninline std::array<Uint32, kLocalCoopMaxPlayers> gLocalCoopFpsNextTurnTick {};'
new = 'inline bool gLocalCoopFpsToggleWasDown = false;\ninline bool gLocalCoopFpsControllerToggleWasDown = false;\ninline std::array<Uint32, kLocalCoopMaxPlayers> gLocalCoopFpsNextTurnTick {};'
if old in fps:
    fps = fps.replace(old, new, 1)
elif 'gLocalCoopFpsControllerToggleWasDown' not in fps:
    raise SystemExit('FPS global toggle anchor not found')

old = '''    gLocalCoopFpsToggleWasDown = toggleDown;\n\n    if (!localCoopFpsActive()) {'''
new = '''    gLocalCoopFpsToggleWasDown = toggleDown;\n\n    // COOP_FPS_CONTROLLER_TOGGLE_V1\n    // L3 is intentionally reserved for camera mode. It was unused by the\n    // co-op control map, and PhoBoi phones expose it as a dedicated FPS / ISO\n    // touchscreen button instead of asking phone users to emulate stick-click.\n    bool controllerToggleDown = false;\n    int controllerToggleSlot = -1;\n    for (int slot = 0; slot < kLocalCoopMaxPlayers; slot++) {\n        LocalCoopPlayer& player = gLocalCoopPlayers[slot];\n        if (!player.connected || player.controller == nullptr) continue;\n        if (SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_LEFTSTICK) != 0) {\n            controllerToggleDown = true;\n            controllerToggleSlot = slot;\n            break;\n        }\n    }\n    if (controllerToggleDown && !gLocalCoopFpsControllerToggleWasDown) {\n        debugPrint("[COOP CAMERA] controller P%d L3/FPS pressed\\n", controllerToggleSlot + 1);\n        localCoopFpsToggle();\n    }\n    gLocalCoopFpsControllerToggleWasDown = controllerToggleDown;\n\n    if (!localCoopFpsActive()) {'''
if old in fps:
    fps = fps.replace(old, new, 1)
elif 'COOP_FPS_CONTROLLER_TOGGLE_V1' not in fps:
    raise SystemExit('FPS tick anchor not found')

# Phone layout: put camera toggle in the natural center utility cluster between PHOBOI and START.
old = '#back{left:calc(50% - 90px);top:max(30px,calc(env(safe-area-inset-top) + 28px))}#start{right:calc(50% - 90px);top:max(30px,calc(env(safe-area-inset-top) + 28px))}#skill{left:calc(50% - 32px);top:max(66px,calc(env(safe-area-inset-top) + 66px));width:64px}'
new = '#back{left:calc(50% - 104px);top:max(30px,calc(env(safe-area-inset-top) + 28px));width:68px}#fps{left:calc(50% - 34px);top:max(30px,calc(env(safe-area-inset-top) + 28px));width:68px}#start{right:calc(50% - 104px);top:max(30px,calc(env(safe-area-inset-top) + 28px));width:68px}#skill{left:calc(50% - 36px);top:max(72px,calc(env(safe-area-inset-top) + 70px));width:72px}'
if old in mobile:
    mobile = mobile.replace(old, new, 1)
elif '#fps{' not in mobile:
    raise SystemExit('Phone utility CSS anchor not found')

old = '<button class="small" id="back">PHOBOI<span class="control-label">PHONE / PIPBOY</span></button><button class="small" id="start">START<span class="control-label">MENU</span></button><button class="small" id="skill">RS CLICK<span class="control-label">SKILLDEX</span></button>'
new = '<button class="small" id="back">PHOBOI<span class="control-label">PHONE</span></button><button class="small" id="fps">FPS / ISO<span class="control-label">CAMERA</span></button><button class="small" id="start">START<span class="control-label">MENU</span></button><button class="small" id="skill">SKILLS<span class="control-label">SKILLDEX</span></button>'
if old in mobile:
    mobile = mobile.replace(old, new, 1)
elif 'id="fps"' not in mobile:
    raise SystemExit('Phone utility HTML anchor not found')

old = "[['ba',0],['bb',1],['bx',2],['by',3],['back',4],['start',6],['skill',8],['lb',9],['rb',10],['du',11],['dd',12],['dl',13],['dr',14]].forEach(x=>bindButton(x[0],x[1]));"
new = "[['ba',0],['bb',1],['bx',2],['by',3],['back',4],['start',6],['fps',7],['skill',8],['lb',9],['rb',10],['du',11],['dd',12],['dl',13],['dr',14]].forEach(x=>bindButton(x[0],x[1]));"
if old in mobile:
    mobile = mobile.replace(old, new, 1)
elif "['fps',7]" not in mobile:
    raise SystemExit('Phone button binding anchor not found')

FPS.write_text(fps, encoding='utf-8')
MOBILE.write_text(mobile, encoding='utf-8')
print('Patched FPS camera toggle: F9 + controller L3 + dedicated PhoBoi FPS/ISO button')
