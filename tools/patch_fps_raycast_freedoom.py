from pathlib import Path

path = Path('src/local_coop_fps.h')
text = path.read_text(encoding='utf-8')
marker = 'COOP_REAL_RAYCAST_FREEDOOM_RUNTIME_V1'
if marker in text:
    print('raycast/Freedoom FPS patch already applied')
    raise SystemExit(0)

needle = '#include "local_coop.h"\n'
if needle not in text:
    raise SystemExit('local_coop include anchor missing')
text = text.replace(needle, needle + '#include "local_coop_fps_raycast.h"\n', 1)

text = text.replace('// COOP_FOUR_INDEPENDENT_FPS_CAMERAS_V2\n', '// COOP_FOUR_INDEPENDENT_FPS_CAMERAS_V2\n// COOP_REAL_RAYCAST_FREEDOOM_RUNTIME_V1\n', 1)

old_sig = '''inline void localCoopFpsDrawBillboard(const LocalCoopFpsBillboard& billboard,\n    unsigned char* dest,\n    int pitch,\n    const LocalCoopFpsViewport& view)\n'''
new_sig = '''inline void localCoopFpsDrawBillboard(const LocalCoopFpsBillboard& billboard,\n    unsigned char* dest,\n    int pitch,\n    const LocalCoopFpsViewport& view,\n    const std::vector<float>& wallDepth)\n'''
if old_sig not in text:
    raise SystemExit('billboard signature anchor missing')
text = text.replace(old_sig, new_sig, 1)

old_center = '''        int centerX = view.x + view.width / 2\n            + static_cast<int>((billboard.lateral / billboard.depth) * view.width * 0.44f);\n        int x = centerX - drawWidth / 2;\n'''
new_center = '''        int centerX = view.x + view.width / 2\n            + static_cast<int>((billboard.lateral / billboard.depth) * view.width * 0.44f);\n        int localColumn = centerX - view.x;\n        if (localColumn >= 0 && localColumn < static_cast<int>(wallDepth.size())\n            && billboard.depth > wallDepth[static_cast<size_t>(localColumn)] + 6.0f) {\n            artUnlock(handle);\n            return;\n        }\n        int x = centerX - drawWidth / 2;\n'''
if old_center not in text:
    raise SystemExit('billboard center anchor missing')
text = text.replace(old_center, new_center, 1)

old_floor = '''    windowFill(gLocalCoopFpsWindow, view.x, view.y + view.height / 2, view.width,\n        view.height - view.height / 2, _colorTable[4228]);\n\n    std::vector<LocalCoopFpsBillboard> billboards;\n'''
new_floor = '''    windowFill(gLocalCoopFpsWindow, view.x, view.y + view.height / 2, view.width,\n        view.height - view.height / 2, _colorTable[4228]);\n\n    // Cast one collision ray per viewport column through the real Fallout map.\n    // Walls/scenery stop the ray; Freedoom supplies a BSD-licensed texture.\n    std::vector<float> wallDepth = localCoopFpsRaycastWalls(\n        player.actor, buffer, pitch, view.x, view.y, view.width, view.height);\n\n    std::vector<LocalCoopFpsBillboard> billboards;\n'''
if old_floor not in text:
    raise SystemExit('FPS floor anchor missing')
text = text.replace(old_floor, new_floor, 1)

old_call = '        localCoopFpsDrawBillboard(billboard, buffer, pitch, view);\n'
new_call = '        localCoopFpsDrawBillboard(billboard, buffer, pitch, view, wallDepth);\n'
if old_call not in text:
    raise SystemExit('billboard draw call anchor missing')
text = text.replace(old_call, new_call, 1)

path.write_text(text, encoding='utf-8')
print('hooked real map raycast + Freedoom texture support into FPS renderer')
