from pathlib import Path

path = Path('src/local_coop_runtime.h')
text = path.read_text(encoding='utf-8')

old = r'''    // COOP_SINGLE_PLAYER_CAMERA_FOLLOW_V1
    // With only one visible human, follow that actor's exact tile just like the
    // stock game. Converting tile -> screen -> tile for a one-player bounding
    // box can resolve to a neighbouring/unchanged hex and leaves the camera
    // apparently stuck while the player walks away. Multi-player still uses
    // the shared bounding-box midpoint below.
    int targetTile = soleActorTile;
    if (count > 1) {
        int targetX = minimumX + (maximumX - minimumX) / 2;
        int targetY = minimumY + (maximumY - minimumY) / 2;
        targetTile = tileFromScreenXY(targetX, targetY, elevation, true);
    }
    if (!tileIsValid(targetTile)) {
        return;
    }
    gLocalCoopCameraTargetTile = targetTile;

    Uint32 now = SDL_GetTicks();
    // Personal HUDs are rendered independently by localCoopPersonalUiTick.
    if (!localCoopTickReached(now, gLocalCoopNextCameraStepTick)) {
        return;
    }

    // A lone player should feel like normal Fallout, not a delayed co-op
    // midpoint camera. Center directly on the actor every camera tick.
    if (count == 1) {
        if (targetTile != gCenterTile) {
            tileSetCenter(targetTile,
                TILE_SET_CENTER_REFRESH_WINDOW | TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS);
        }
        gLocalCoopNextCameraStepTick = now + 16;
        return;
    }

    int distance = tileDistanceBetween(gCenterTile, targetTile);
    if (distance <= 0) {
        gLocalCoopNextCameraStepTick = now + 33;
        return;
    }

    // Ease instead of snapping. Far-away targets catch up faster, while the last
    // few hexes advance one at a time to avoid visible camera judder.
    int stepDistance = 1;
    if (distance > 12) {
        stepDistance = 4;
    } else if (distance > 7) {
        stepDistance = 3;
    } else if (distance > 3) {
        stepDistance = 2;
    }

    int nextCenter = targetTile;
    if (stepDistance < distance) {
        int rotation = tileGetRotationTo(gCenterTile, targetTile);
        nextCenter = tileGetTileInDirection(gCenterTile, rotation, stepDistance);
    }

    if (tileIsValid(nextCenter) && nextCenter != gCenterTile) {
        tileSetCenter(nextCenter,
            TILE_SET_CENTER_REFRESH_WINDOW | TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS);
    }
    gLocalCoopNextCameraStepTick = now + 33;
'''

new = r'''    // COOP_ASCENT_SHARED_CAMERA_V1
    // One shared camera for 1-4 local players. The focus point is the center of
    // the visible human-player bounds, matching the feel of a shared-screen
    // action RPG: no split screen and no camera ownership handoff. With one
    // player the bounds collapse to that actor, so solo play uses the exact same
    // camera controller and cannot fall into a separate broken follow path.
    int targetTile = soleActorTile;
    if (count > 1) {
        int targetX = minimumX + (maximumX - minimumX) / 2;
        int targetY = minimumY + (maximumY - minimumY) / 2;
        int framedTile = tileFromScreenXY(targetX, targetY, elevation, true);
        if (tileIsValid(framedTile)) {
            targetTile = framedTile;
        }
    }
    if (!tileIsValid(targetTile)) {
        gLocalCoopCameraTargetTile = -1;
        return;
    }
    gLocalCoopCameraTargetTile = targetTile;

    Uint32 now = SDL_GetTicks();
    if (!localCoopTickReached(now, gLocalCoopNextCameraStepTick)) {
        return;
    }

    int distance = tileDistanceBetween(gCenterTile, targetTile);

    // Soft one-hex dead zone prevents camera shimmer while characters idle or
    // shuffle around the center. Outside it, catch-up strength rises with group
    // displacement so a spread-out party never leaves the camera behind.
    if (distance <= 1) {
        gLocalCoopNextCameraStepTick = now + 16;
        return;
    }

    int stepDistance = 1;
    if (distance > 14) {
        stepDistance = 6;
    } else if (distance > 9) {
        stepDistance = 4;
    } else if (distance > 5) {
        stepDistance = 3;
    } else if (distance > 2) {
        stepDistance = 2;
    }

    int nextCenter = targetTile;
    if (stepDistance < distance) {
        int rotation = tileGetRotationTo(gCenterTile, targetTile);
        nextCenter = tileGetTileInDirection(gCenterTile, rotation, stepDistance);
    }

    if (tileIsValid(nextCenter) && nextCenter != gCenterTile) {
        tileSetCenter(nextCenter,
            TILE_SET_CENTER_REFRESH_WINDOW | TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS);
    }
    gLocalCoopNextCameraStepTick = now + 16;
'''

if 'COOP_ASCENT_SHARED_CAMERA_V1' in text:
    print('Ascent shared camera already applied')
elif old not in text:
    raise SystemExit('Expected camera block not found; source changed')
else:
    path.write_text(text.replace(old, new, 1), encoding='utf-8')
    print('Applied Ascent-style shared camera')
