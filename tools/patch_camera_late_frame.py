from pathlib import Path

runtime = Path('src/local_coop_runtime.h')
main = Path('src/main.cc')

r = runtime.read_text(encoding='utf-8')
old = '''    // COOP_NATIVE_BILLBOARD_FPS_RUNTIME_V1\n    localCoopUpdateSharedCamera();\n    // COOP_FPS_SINGLE_LATE_TICK_V1\n'''
new = '''    // COOP_CAMERA_LATE_FRAME_OWNER_V1\n    // Camera is updated from mainLoop after movement/scripts/map requests.\n    // Updating it here lets later stock processing overwrite our center every frame.\n    // COOP_FPS_SINGLE_LATE_TICK_V1\n'''
if old in r:
    r = r.replace(old, new, 1)
elif 'COOP_CAMERA_LATE_FRAME_OWNER_V1' not in r:
    raise SystemExit('runtime camera call pattern not found')
runtime.write_text(r, encoding='utf-8')

m = main.read_text(encoding='utf-8')
old2 = '''        if ((gDude->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0) {\n            endgameSetupDeathEnding(ENDGAME_DEATH_ENDING_REASON_DEATH);\n            _main_show_death_scene = 1;\n            _game_user_wants_to_quit = 2;\n        }\n\n        // COOP_FPS_LATE_RENDER_HOOK_V1\n'''
new2 = '''        if ((gDude->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0) {\n            endgameSetupDeathEnding(ENDGAME_DEATH_ENDING_REASON_DEATH);\n            _main_show_death_scene = 1;\n            _game_user_wants_to_quit = 2;\n        }\n\n        // COOP_CAMERA_LATE_FRAME_OWNER_V1\n        // The shared camera must be the LAST isometric camera writer in the frame.\n        // gameHandleKey, scriptsHandleRequests and mapHandleTransition can all move\n        // actors or recenter the stock camera, so following before them is overwritten.\n        // One player follows P1; 2-4 players use the same Ascent-style group framing.\n        localCoopUpdateSharedCamera();\n\n        // COOP_FPS_LATE_RENDER_HOOK_V1\n'''
if old2 in m:
    m = m.replace(old2, new2, 1)
elif 'COOP_CAMERA_LATE_FRAME_OWNER_V1' not in m:
    raise SystemExit('mainLoop insertion pattern not found')
main.write_text(m, encoding='utf-8')
print('Applied late-frame shared camera ownership fix')
