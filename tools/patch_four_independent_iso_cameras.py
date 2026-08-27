from pathlib import Path

RUNTIME = Path('src/local_coop_runtime.h')
MARKER = 'COOP_FOUR_INDEPENDENT_ISOMETRIC_CAMERAS_RUNTIME_V1'

text = RUNTIME.read_text(encoding='utf-8')
if MARKER in text:
    print('independent ISO cameras already wired')
    raise SystemExit(0)

include_anchor = '#include "local_coop_fps.h"\n'
include_insert = include_anchor + '#include "local_coop_iso_cameras.h"\n'
if include_anchor not in text:
    raise SystemExit('missing local_coop_fps include anchor')
text = text.replace(include_anchor, include_insert, 1)

old = '''    // COOP_NATIVE_BILLBOARD_FPS_RUNTIME_V1\n    localCoopUpdateSharedCamera();\n    localCoopFpsTick();\n    // Personal HUDs draw after FPS so all four remain readable.\n'''
new = '''    // COOP_NATIVE_BILLBOARD_FPS_RUNTIME_V1\n    // COOP_FOUR_INDEPENDENT_ISOMETRIC_CAMERAS_RUNTIME_V1\n    // Shared-camera easing is retained only for the single-player stock ISO\n    // presentation. Multi-player ISO uses one independently centered capture\n    // per active player; FPS already owns separate player viewports.\n    if (!localCoopFpsActive() && localCoopIsoActivePlayerCount() <= 1) {\n        localCoopUpdateSharedCamera();\n    }\n    localCoopFpsTick();\n    localCoopIsoCamerasTick();\n    // Personal HUDs draw after camera presentation so all four remain readable.\n'''
if old not in text:
    raise SystemExit('missing camera runtime anchor')
text = text.replace(old, new, 1)

RUNTIME.write_text(text, encoding='utf-8')
print('wired independent ISO cameras')
