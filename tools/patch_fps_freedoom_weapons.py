from pathlib import Path

path = Path('src/local_coop_fps.h')
text = path.read_text(encoding='utf-8')
marker = 'COOP_FREEDOOM_FIRST_PERSON_WEAPONS_RUNTIME_V1'
if marker in text:
    print('Freedoom FPS weapon overlay already wired')
    raise SystemExit(0)

inc = '#include "local_coop_fps_raycast.h"\n'
if inc not in text:
    raise SystemExit('raycast include anchor missing')
text = text.replace(inc, inc + '#include "local_coop_fps_weapon.h"\n', 1)

anchor = '''    for (const auto& billboard : billboards) {\n        localCoopFpsDrawBillboard(billboard, buffer, pitch, view, wallDepth);\n    }\n\n    int cx = view.x + view.width / 2;\n'''
replacement = '''    for (const auto& billboard : billboards) {\n        localCoopFpsDrawBillboard(billboard, buffer, pitch, view, wallDepth);\n    }\n\n    // COOP_FREEDOOM_FIRST_PERSON_WEAPONS_RUNTIME_V1\n    // Draw the equipped player's first-person weapon after world geometry and\n    // billboards so it behaves like a classic FPS view model.\n    localCoopFpsDrawWeapon(slot, buffer, pitch, view.x, view.y, view.width, view.height);\n\n    int cx = view.x + view.width / 2;\n'''
if anchor not in text:
    raise SystemExit('billboard/crosshair anchor missing')
text = text.replace(anchor, replacement, 1)
path.write_text(text, encoding='utf-8')
print('wired Freedoom first-person weapon overlay')
