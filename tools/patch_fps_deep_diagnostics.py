from pathlib import Path

fps = Path('src/local_coop_fps.h')
fs = fps.read_text(encoding='utf-8')
changed = False

if 'COOP_FPS_DEEP_DIAGNOSTICS_V2' not in fs:
    anchor = 'inline bool gLocalCoopFpsLoggedFirstFrame = false;\n'
    if anchor not in fs:
        raise SystemExit('deep diagnostics globals anchor missing')
    fs = fs.replace(anchor, anchor +
        '// COOP_FPS_DEEP_DIAGNOSTICS_V2\n'
        'inline int gLocalCoopFpsObjectsScanned = 0;\n'
        'inline int gLocalCoopFpsObjectsProjected = 0;\n'
        'inline int gLocalCoopFpsArtLockFailures = 0;\n'
        'inline int gLocalCoopFpsBillboardsOccluded = 0;\n'
        'inline int gLocalCoopFpsBillboardsClipped = 0;\n'
        'inline int gLocalCoopFpsBillboardsDrawn = 0;\n'
        'inline bool gLocalCoopFpsLoggedBufferFailure = false;\n'
        'inline bool gLocalCoopFpsLoggedActiveEntry = false;\n', 1)

    collect_start = '    out.clear();\n    if (camera == nullptr) return;\n\n    for (Object* object = objectFindFirst(); object != nullptr; object = objectFindNext()) {\n'
    if collect_start not in fs:
        raise SystemExit('deep diagnostics collect start anchor missing')
    fs = fs.replace(collect_start,
        '    out.clear();\n'
        '    gLocalCoopFpsObjectsScanned = 0;\n'
        '    gLocalCoopFpsObjectsProjected = 0;\n'
        '    if (camera == nullptr) return;\n\n'
        '    for (Object* object = objectFindFirst(); object != nullptr; object = objectFindNext()) {\n'
        '        gLocalCoopFpsObjectsScanned++;\n', 1)

    push_anchor = '        out.push_back({ object, depth, lateral, distance });\n'
    if push_anchor not in fs:
        raise SystemExit('deep diagnostics projected anchor missing')
    fs = fs.replace(push_anchor,
        '        gLocalCoopFpsObjectsProjected++;\n'
        + push_anchor, 1)

    art_anchor = '    Art* art = artLock(object->fid, &handle);\n    if (art == nullptr) return;\n'
    if art_anchor not in fs:
        raise SystemExit('deep diagnostics artLock anchor missing')
    fs = fs.replace(art_anchor,
        '    Art* art = artLock(object->fid, &handle);\n'
        '    if (art == nullptr) {\n'
        '        gLocalCoopFpsArtLockFailures++;\n'
        '        return;\n'
        '    }\n', 1)

    occ_anchor = '            artUnlock(handle);\n            return;\n'
    if occ_anchor not in fs:
        raise SystemExit('deep diagnostics occlusion anchor missing')
    fs = fs.replace(occ_anchor,
        '            gLocalCoopFpsBillboardsOccluded++;\n'
        + occ_anchor, 1)

    draw_anchor = '            blitBufferToBufferStretchTrans(src, srcWidth, srcHeight, srcWidth,\n                dest + y * pitch + x, drawWidth, drawHeight, pitch);\n        }\n'
    if draw_anchor not in fs:
        raise SystemExit('deep diagnostics draw anchor missing')
    fs = fs.replace(draw_anchor,
        '            blitBufferToBufferStretchTrans(src, srcWidth, srcHeight, srcWidth,\n'
        '                dest + y * pitch + x, drawWidth, drawHeight, pitch);\n'
        '            gLocalCoopFpsBillboardsDrawn++;\n'
        '        } else {\n'
        '            gLocalCoopFpsBillboardsClipped++;\n'
        '        }\n', 1)

    collect_call = '    localCoopFpsCollect(player.actor, billboards);\n'
    if collect_call not in fs:
        raise SystemExit('deep diagnostics collect call anchor missing')
    fs = fs.replace(collect_call,
        '    gLocalCoopFpsArtLockFailures = 0;\n'
        '    gLocalCoopFpsBillboardsOccluded = 0;\n'
        '    gLocalCoopFpsBillboardsClipped = 0;\n'
        '    gLocalCoopFpsBillboardsDrawn = 0;\n'
        + collect_call, 1)

    old_diag = '        debugPrint("[COOP FPS] P1 actor tile=%d elev=%d rot=%d viewport=%dx%d walls=%d billboards=%d\\n",\n            player.actor->tile, player.actor->elevation, player.actor->rotation,\n            view.width, view.height, static_cast<int>(wallDepth.size()), static_cast<int>(billboards.size()));\n'
    if old_diag not in fs:
        raise SystemExit('deep diagnostics summary anchor missing')
    fs = fs.replace(old_diag,
        '        debugPrint("[COOP FPS STAGE] render P1 tile=%d elev=%d rot=%d viewport=%dx%d rays=%d rayHits=%d scanned=%d projected=%d billboards=%d drawn=%d occluded=%d clipped=%d artFail=%d freedoom=%d\\n",\n'
        '            player.actor->tile, player.actor->elevation, player.actor->rotation,\n'
        '            view.width, view.height, static_cast<int>(wallDepth.size()), gLocalCoopFpsRayHitColumns,\n'
        '            gLocalCoopFpsObjectsScanned, gLocalCoopFpsObjectsProjected, static_cast<int>(billboards.size()),\n'
        '            gLocalCoopFpsBillboardsDrawn, gLocalCoopFpsBillboardsOccluded, gLocalCoopFpsBillboardsClipped,\n'
        '            gLocalCoopFpsArtLockFailures, gLocalCoopFreedoomTextureReady ? 1 : 0);\n', 1)

    active_anchor = '    if (!localCoopFpsActive()) {\n        localCoopFpsDestroyWindow();\n        return;\n    }\n\n    int width = screenGetWidth();\n'
    if active_anchor not in fs:
        raise SystemExit('deep diagnostics active anchor missing')
    fs = fs.replace(active_anchor,
        '    if (!localCoopFpsActive()) {\n'
        '        gLocalCoopFpsLoggedActiveEntry = false;\n'
        '        localCoopFpsDestroyWindow();\n'
        '        return;\n'
        '    }\n'
        '    if (!gLocalCoopFpsLoggedActiveEntry) {\n'
        '        debugPrint("[COOP FPS STAGE] 1 active mode entered\\n");\n'
        '        gLocalCoopFpsLoggedActiveEntry = true;\n'
        '    }\n\n'
        '    int width = screenGetWidth();\n', 1)

    size_anchor = '    int height = screenGetVisibleHeight();\n    if (width <= 0 || height <= 0) return;\n'
    if size_anchor not in fs:
        raise SystemExit('deep diagnostics size anchor missing')
    fs = fs.replace(size_anchor,
        '    int height = screenGetVisibleHeight();\n'
        '    if (width <= 0 || height <= 0) {\n'
        '        debugPrint("[COOP FPS STAGE] ERROR invalid screen size=%dx%d\\n", width, height);\n'
        '        return;\n'
        '    }\n', 1)

    buffer_anchor = '    unsigned char* buffer = windowGetBuffer(gLocalCoopFpsWindow);\n    if (buffer == nullptr) return;\n'
    if buffer_anchor not in fs:
        raise SystemExit('deep diagnostics buffer anchor missing')
    fs = fs.replace(buffer_anchor,
        '    unsigned char* buffer = windowGetBuffer(gLocalCoopFpsWindow);\n'
        '    if (buffer == nullptr) {\n'
        '        if (!gLocalCoopFpsLoggedBufferFailure) {\n'
        '            debugPrint("[COOP FPS STAGE] ERROR window buffer null id=%d\\n", gLocalCoopFpsWindow);\n'
        '            gLocalCoopFpsLoggedBufferFailure = true;\n'
        '        }\n'
        '        return;\n'
        '    }\n'
        '    gLocalCoopFpsLoggedBufferFailure = false;\n', 1)

    first_frame_anchor = '        debugPrint("[COOP FPS] first rendered frame refreshed successfully\\n");\n'
    if first_frame_anchor not in fs:
        raise SystemExit('deep diagnostics first frame anchor missing')
    fs = fs.replace(first_frame_anchor,
        '        debugPrint("[COOP FPS STAGE] 6 first rendered frame refreshed successfully window=%d\\n", gLocalCoopFpsWindow);\n', 1)

    fs = fs.replace('debugPrint("[COOP FPS] window created id=%d size=%dx%d\\n", gLocalCoopFpsWindow, width, height);',
        'debugPrint("[COOP FPS STAGE] 2 window created id=%d size=%dx%d\\n", gLocalCoopFpsWindow, width, height);', 1)

    fps.write_text(fs, encoding='utf-8')
    changed = True

ray = Path('src/local_coop_fps_raycast.h')
rs = ray.read_text(encoding='utf-8')
if 'COOP_FPS_RAYCAST_DIAGNOSTICS_V2' not in rs:
    globals_anchor = 'inline std::array<unsigned char, 64 * 64> gLocalCoopFreedoomFlat {};\n'
    if globals_anchor not in rs:
        raise SystemExit('ray diagnostics globals anchor missing')
    rs = rs.replace(globals_anchor, globals_anchor +
        '// COOP_FPS_RAYCAST_DIAGNOSTICS_V2\n'
        'inline int gLocalCoopFpsRayHitColumns = 0;\n'
        'inline int gLocalCoopFpsRayLastBlockerFid = -1;\n', 1)

    start_anchor = '    std::vector<float> depth(static_cast<size_t>(std::max(0, viewWidth)), 1000000.0f);\n'
    if start_anchor not in rs:
        raise SystemExit('ray diagnostics start anchor missing')
    rs = rs.replace(start_anchor, start_anchor +
        '    gLocalCoopFpsRayHitColumns = 0;\n'
        '    gLocalCoopFpsRayLastBlockerFid = -1;\n', 1)

    hit_anchor = '        depth[static_cast<size_t>(column)] = hitDistance;\n'
    if hit_anchor not in rs:
        raise SystemExit('ray diagnostics hit anchor missing')
    rs = rs.replace(hit_anchor,
        '        gLocalCoopFpsRayHitColumns++;\n'
        '        gLocalCoopFpsRayLastBlockerFid = hitObject->fid;\n'
        + hit_anchor, 1)

    ray.write_text(rs, encoding='utf-8')
    changed = True

print('deep FPS diagnostics applied' if changed else 'deep FPS diagnostics already applied')
