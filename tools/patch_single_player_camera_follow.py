from pathlib import Path

p = Path('src/local_coop_runtime.h')
s = p.read_text(encoding='utf-8')

old = '''    int maximumX = -0x7FFFFFFF;\n    int maximumY = -0x7FFFFFFF;\n    int count = 0;\n'''
new = '''    int maximumX = -0x7FFFFFFF;\n    int maximumY = -0x7FFFFFFF;\n    int count = 0;\n    int soleActorTile = -1;\n'''
if old not in s:
    raise SystemExit('camera locals marker not found')
s = s.replace(old, new, 1)

old = '''            maximumX = std::max(maximumX, x);\n            maximumY = std::max(maximumY, y);\n            count++;\n'''
new = '''            maximumX = std::max(maximumX, x);\n            maximumY = std::max(maximumY, y);\n            soleActorTile = actor->tile;\n            count++;\n'''
if old not in s:
    raise SystemExit('camera actor accumulation marker not found')
s = s.replace(old, new, 1)

old = '''    if (count == 0) {\n        gLocalCoopCameraTargetTile = -1;\n        return;\n    }\n\n    int targetX = minimumX + (maximumX - minimumX) / 2;\n    int targetY = minimumY + (maximumY - minimumY) / 2;\n    int targetTile = tileFromScreenXY(targetX, targetY, elevation, true);\n'''
new = '''    if (count == 0) {\n        gLocalCoopCameraTargetTile = -1;\n        return;\n    }\n\n    // COOP_SINGLE_PLAYER_CAMERA_FOLLOW_V1\n    // With only one visible human, follow that actor's exact tile just like the\n    // stock game. Converting tile -> screen -> tile for a one-player bounding\n    // box can resolve to a neighbouring/unchanged hex and leaves the camera\n    // apparently stuck while the player walks away. Multi-player still uses\n    // the shared bounding-box midpoint below.\n    int targetTile = soleActorTile;\n    if (count > 1) {\n        int targetX = minimumX + (maximumX - minimumX) / 2;\n        int targetY = minimumY + (maximumY - minimumY) / 2;\n        targetTile = tileFromScreenXY(targetX, targetY, elevation, true);\n    }\n'''
if old not in s:
    raise SystemExit('camera target marker not found')
s = s.replace(old, new, 1)

old = '''    int distance = tileDistanceBetween(gCenterTile, targetTile);\n    if (distance <= 0) {\n'''
new = '''    // A lone player should feel like normal Fallout, not a delayed co-op\n    // midpoint camera. Center directly on the actor every camera tick.\n    if (count == 1) {\n        if (targetTile != gCenterTile) {\n            tileSetCenter(targetTile,\n                TILE_SET_CENTER_REFRESH_WINDOW | TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS);\n        }\n        gLocalCoopNextCameraStepTick = now + 16;\n        return;\n    }\n\n    int distance = tileDistanceBetween(gCenterTile, targetTile);\n    if (distance <= 0) {\n'''
if old not in s:
    raise SystemExit('camera distance marker not found')
s = s.replace(old, new, 1)

p.write_text(s, encoding='utf-8')
print('patched single-player camera follow')
