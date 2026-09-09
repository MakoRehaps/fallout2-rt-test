from pathlib import Path


def read(path):
    return Path(path).read_text(encoding="utf-8")


def write(path, text):
    Path(path).write_text(text, encoding="utf-8")


# -----------------------------------------------------------------------------
# 1) Preserve the ready-room join decision until the live map exists, then
#    create P2-P4 beside P1.  The ready room intentionally has no map/critters,
#    so trying to create the actor there cannot work.
# -----------------------------------------------------------------------------
coop_path = "src/local_coop.h"
coop = read(coop_path)

if "COOP_PREJOINED_LIVE_SPAWN_V1" not in coop:
    anchor = "inline std::array<LocalCoopPlayer, kLocalCoopMaxPlayers> gLocalCoopPlayers;\n"
    if anchor not in coop:
        raise SystemExit("local_coop.h player array anchor missing")
    coop = coop.replace(
        anchor,
        anchor
        + "// COOP_PREJOINED_LIVE_SPAWN_V1\n"
        + "// The tileless ready room cannot create critters because no map exists yet.\n"
        + "// Remember which slots joined and materialize them on the first live map tick.\n"
        + "inline std::array<bool, kLocalCoopMaxPlayers> gLocalCoopPrejoinedSlots {};\n",
        1,
    )

    create_end = "    return true;\n}\n\ninline void localCoopRestoreCharactersFromSave()\n"
    if create_end not in coop:
        raise SystemExit("localCoopCreatePlayerActor tail anchor missing")
    spawn_fn = """    return true;\n}\n\ninline void localCoopSpawnPrejoinedPlayers()\n{\n    if (gDude == nullptr || !tileIsValid(gDude->tile)) {\n        return;\n    }\n\n    for (int slot = 1; slot < kLocalCoopMaxPlayers; ++slot) {\n        LocalCoopPlayer& player = gLocalCoopPlayers[slot];\n        if (!gLocalCoopPrejoinedSlots[slot]\n            || !player.connected\n            || player.controller == nullptr\n            || player.actor != nullptr) {\n            continue;\n        }\n\n        debugPrint(\"[COOP PREJOIN] slot=%d spawning on live map\\n\", slot);\n        if (localCoopCreatePlayerActor(slot)) {\n            gLocalCoopPrejoinedSlots[slot] = false;\n            debugPrint(\"[COOP PREJOIN] slot=%d live actor ready id=%d tile=%d\\n\",\n                slot, player.actor != nullptr ? player.actor->id : -1,\n                player.actor != nullptr ? player.actor->tile : -1);\n        } else {\n            debugPrint(\"[COOP PREJOIN] slot=%d spawn attempt failed; retrying next tick\\n\", slot);\n        }\n    }\n}\n\ninline void localCoopRestoreCharactersFromSave()\n"""
    coop = coop.replace(create_end, spawn_fn, 1)
    write(coop_path, coop)

room_path = "src/local_coop_group_room.h"
room = read(room_path)
if "COOP_READY_ROOM_PREJOIN_TRANSFER_V1" not in room:
    anchor = "    joined[0] = true;\n"
    if anchor not in room:
        raise SystemExit("group room joined[0] anchor missing")
    room = room.replace(
        anchor,
        anchor
        + "    // COOP_READY_ROOM_PREJOIN_TRANSFER_V1\n"
        + "    gLocalCoopPrejoinedSlots.fill(false);\n"
        + "    gLocalCoopPrejoinedSlots[0] = true;\n",
        1,
    )

    # Both tutorial and normal ready-room join paths contain this exact pair.
    pair = "                    joined[slot] = true;\n                    ready[slot] = false;\n"
    count = room.count(pair)
    if count < 2:
        raise SystemExit(f"expected two ready-room join paths, found {count}")
    room = room.replace(
        pair,
        pair + "                    gLocalCoopPrejoinedSlots[slot] = true;\n",
    )

    accept = "            if (joinedCount > 0 && readyCount == joinedCount) {\n                accepted = true;\n"
    if accept not in room:
        raise SystemExit("group room accept anchor missing")
    room = room.replace(
        accept,
        "            if (joinedCount > 0 && readyCount == joinedCount) {\n"
        "                for (int slot = 0; slot < kLocalCoopMaxPlayers; ++slot) {\n"
        "                    gLocalCoopPrejoinedSlots[slot] = joined[slot];\n"
        "                }\n"
        "                debugPrint(\"[COOP PREJOIN] ready-room transfer p1=%d p2=%d p3=%d p4=%d\\n\",\n"
        "                    joined[0] ? 1 : 0, joined[1] ? 1 : 0, joined[2] ? 1 : 0, joined[3] ? 1 : 0);\n"
        "                accepted = true;\n",
        1,
    )
    write(room_path, room)

runtime_path = "src/local_coop_runtime.h"
runtime = read(runtime_path)
if "COOP_PREJOIN_LIVE_SPAWN_RUNTIME_V1" not in runtime:
    anchor = "    localCoopRestoreCharactersFromSave();\n    localCoopKeepReservedActorsWithParty();\n"
    if anchor not in runtime:
        raise SystemExit("runtime restore/keep anchor missing")
    runtime = runtime.replace(
        anchor,
        "    localCoopRestoreCharactersFromSave();\n"
        "    // COOP_PREJOIN_LIVE_SPAWN_RUNTIME_V1\n"
        "    localCoopSpawnPrejoinedPlayers();\n"
        "    localCoopKeepReservedActorsWithParty();\n",
        1,
    )

# Render FPS exactly once, at the late main-loop point.  Runtime still drives
# all other co-op systems; the late hook owns FPS input + presentation.
if "COOP_FPS_SINGLE_LATE_TICK_V1" not in runtime:
    anchor = "    localCoopUpdateSharedCamera();\n    localCoopFpsTick();\n"
    if anchor not in runtime:
        raise SystemExit("runtime FPS tick anchor missing")
    runtime = runtime.replace(
        anchor,
        "    localCoopUpdateSharedCamera();\n"
        "    // COOP_FPS_SINGLE_LATE_TICK_V1\n"
        "    // FPS input/render runs once from main.cc immediately before present.\n",
        1,
    )
write(runtime_path, runtime)

main_path = "src/main.cc"
main = read(main_path)
if "COOP_FPS_SINGLE_LATE_TICK_V1" not in main:
    old = "        // COOP_FPS_LATE_RENDER_HOOK_V1\n        // Draw FPS last so later map/interface refreshes cannot cover it.\n        if (localCoopFpsActive()) {\n            localCoopFpsTick();\n        }\n"
    if old not in main:
        raise SystemExit("main late FPS hook anchor missing")
    new = "        // COOP_FPS_LATE_RENDER_HOOK_V1\n        // COOP_FPS_SINGLE_LATE_TICK_V1\n        // Run once, unconditionally: this processes L3/FPS input even while\n        // isometric, then draws FPS last so the stock world cannot cover it.\n        localCoopFpsTick();\n"
    main = main.replace(old, new, 1)
    write(main_path, main)

# -----------------------------------------------------------------------------
# 2) Dynamic FPS layouts.  Do not draw WAITING quadrants over a fullscreen P1.
#    1 player = fullscreen; 2 = left/right; 3 = two top + one full-width bottom;
#    4 = quadrants.  Slots are ranked only among actually spawned human actors.
# -----------------------------------------------------------------------------
fps_path = "src/local_coop_fps.h"
fps = read(fps_path)

if "COOP_FPS_DYNAMIC_ACTIVE_LAYOUT_V1" not in fps:
    start = fps.find("inline LocalCoopFpsViewport localCoopFpsViewportForSlot(")
    end = fps.find("\ninline bool localCoopFpsProject", start)
    if start < 0 or end < 0:
        raise SystemExit("FPS viewport function boundaries missing")
    dynamic_fn = r'''inline LocalCoopFpsViewport localCoopFpsViewportForSlot(int slot, int width, int height)
{
    // COOP_FPS_DYNAMIC_ACTIVE_LAYOUT_V1
    int activeSlots[kLocalCoopMaxPlayers] = { -1, -1, -1, -1 };
    int activeCount = 0;
    int rank = -1;
    for (int i = 0; i < kLocalCoopMaxPlayers; ++i) {
        const LocalCoopPlayer& p = gLocalCoopPlayers[i];
        if (p.connected && p.humanOwned && p.actor != nullptr) {
            activeSlots[activeCount] = i;
            if (i == slot) rank = activeCount;
            ++activeCount;
        }
    }

    LocalCoopFpsViewport view;
    if (rank < 0 || activeCount <= 0) return view;

    if (activeCount == 1) {
        view = { 0, 0, width, height };
        return view;
    }

    int halfW = width / 2;
    int halfH = height / 2;
    if (activeCount == 2) {
        view.x = rank == 0 ? 0 : halfW;
        view.y = 0;
        view.width = rank == 0 ? halfW : width - halfW;
        view.height = height;
        return view;
    }

    if (activeCount == 3) {
        if (rank < 2) {
            view.x = rank == 0 ? 0 : halfW;
            view.y = 0;
            view.width = rank == 0 ? halfW : width - halfW;
            view.height = halfH;
        } else {
            view.x = 0;
            view.y = halfH;
            view.width = width;
            view.height = height - halfH;
        }
        return view;
    }

    view.x = (rank & 1) != 0 ? halfW : 0;
    view.y = rank >= 2 ? halfH : 0;
    view.width = (rank & 1) != 0 ? width - halfW : halfW;
    view.height = rank >= 2 ? height - halfH : halfH;
    return view;
}
'''
    fps = fps[:start] + dynamic_fn + fps[end:]

# Move diagnostics after billboard drawing so drawn/occluded/clipped are real.
if "COOP_FPS_POST_DRAW_DIAGNOSTICS_V1" not in fps:
    old = r'''    localCoopFpsCollect(player.actor, billboards);
    Uint32 diagNow = SDL_GetTicks();
    if (slot == 0 && static_cast<Sint32>(diagNow - gLocalCoopFpsNextDiagnosticTick) >= 0) {
        debugPrint("[COOP FPS STAGE] render P1 tile=%d elev=%d rot=%d viewport=%dx%d rays=%d rayHits=%d scanned=%d projected=%d billboards=%d drawn=%d occluded=%d clipped=%d artFail=%d freedoom=%d\n",
            player.actor->tile, player.actor->elevation, player.actor->rotation,
            view.width, view.height, static_cast<int>(wallDepth.size()), gLocalCoopFpsRayHitColumns,
            gLocalCoopFpsObjectsScanned, gLocalCoopFpsObjectsProjected, static_cast<int>(billboards.size()),
            gLocalCoopFpsBillboardsDrawn, gLocalCoopFpsBillboardsOccluded, gLocalCoopFpsBillboardsClipped,
            gLocalCoopFpsArtLockFailures, gLocalCoopFreedoomTextureReady ? 1 : 0);
        gLocalCoopFpsNextDiagnosticTick = diagNow + 1000;
    }
    for (const auto& billboard : billboards) {
        localCoopFpsDrawBillboard(billboard, buffer, pitch, view, wallDepth);
    }
'''
    new = r'''    localCoopFpsCollect(player.actor, billboards);
    for (const auto& billboard : billboards) {
        localCoopFpsDrawBillboard(billboard, buffer, pitch, view, wallDepth);
    }

    // COOP_FPS_POST_DRAW_DIAGNOSTICS_V1
    Uint32 diagNow = SDL_GetTicks();
    if (slot == 0 && static_cast<Sint32>(diagNow - gLocalCoopFpsNextDiagnosticTick) >= 0) {
        debugPrint("[COOP FPS STAGE] render P1 tile=%d elev=%d rot=%d viewport=%dx%d rays=%d rayHits=%d scanned=%d projected=%d billboards=%d drawn=%d occluded=%d clipped=%d artFail=%d freedoom=%d\n",
            player.actor->tile, player.actor->elevation, player.actor->rotation,
            view.width, view.height, static_cast<int>(wallDepth.size()), gLocalCoopFpsRayHitColumns,
            gLocalCoopFpsObjectsScanned, gLocalCoopFpsObjectsProjected, static_cast<int>(billboards.size()),
            gLocalCoopFpsBillboardsDrawn, gLocalCoopFpsBillboardsOccluded, gLocalCoopFpsBillboardsClipped,
            gLocalCoopFpsArtLockFailures, gLocalCoopFreedoomTextureReady ? 1 : 0);
        gLocalCoopFpsNextDiagnosticTick = diagNow + 1000;
    }
'''
    if old not in fps:
        raise SystemExit("FPS pre-draw diagnostic block missing")
    fps = fps.replace(old, new, 1)

if "COOP_FPS_DRAW_ACTIVE_ONLY_V1" not in fps:
    old = r'''    for (int slot = 0; slot < kLocalCoopMaxPlayers; slot++) {
        localCoopFpsProcessLook(slot);
        localCoopFpsDrawViewport(slot, buffer, width, width, height);
    }

    int halfW = width / 2;
    int halfH = height / 2;
    windowDrawLine(gLocalCoopFpsWindow, halfW, 0, halfW, height - 1, _colorTable[992]);
    windowDrawLine(gLocalCoopFpsWindow, 0, halfH, width - 1, halfH, _colorTable[992]);
'''
    new = r'''    // COOP_FPS_DRAW_ACTIVE_ONLY_V1
    int activeCount = 0;
    for (int slot = 0; slot < kLocalCoopMaxPlayers; ++slot) {
        const LocalCoopPlayer& player = gLocalCoopPlayers[slot];
        if (player.connected && player.humanOwned && player.actor != nullptr) ++activeCount;
    }

    for (int slot = 0; slot < kLocalCoopMaxPlayers; slot++) {
        LocalCoopPlayer& player = gLocalCoopPlayers[slot];
        if (!player.connected || !player.humanOwned || player.actor == nullptr) continue;
        localCoopFpsProcessLook(slot);
        localCoopFpsDrawViewport(slot, buffer, width, width, height);
    }

    // Separators match the dynamic layout and never carve empty WAITING boxes
    // over an active player's view.
    int halfW = width / 2;
    int halfH = height / 2;
    if (activeCount == 2) {
        windowDrawLine(gLocalCoopFpsWindow, halfW, 0, halfW, height - 1, _colorTable[992]);
    } else if (activeCount == 3) {
        windowDrawLine(gLocalCoopFpsWindow, halfW, 0, halfW, halfH - 1, _colorTable[992]);
        windowDrawLine(gLocalCoopFpsWindow, 0, halfH, width - 1, halfH, _colorTable[992]);
    } else if (activeCount >= 4) {
        windowDrawLine(gLocalCoopFpsWindow, halfW, 0, halfW, height - 1, _colorTable[992]);
        windowDrawLine(gLocalCoopFpsWindow, 0, halfH, width - 1, halfH, _colorTable[992]);
    }
'''
    if old not in fps:
        raise SystemExit("FPS four-slot draw block missing")
    fps = fps.replace(old, new, 1)

write(fps_path, fps)
print("patched ready-room live spawn + dynamic FPS layout + single late FPS tick")
