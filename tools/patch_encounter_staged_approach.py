from pathlib import Path

p = Path('src/worldmap.cc')
s = p.read_text(encoding='utf-8')
marker = 'COOP_STAGED_ENCOUNTER_APPROACH_V1'
if marker in s:
    print('already patched')
    raise SystemExit(0)

anchor = '''static const char* coopEncounterObjectiveName(CoopEncounterObjective objective)\n{'''
if anchor not in s:
    raise SystemExit('self-play encounter anchor missing')

state_code = r'''
// COOP_STAGED_ENCOUNTER_APPROACH_V1
// Random-encounter groups are staged apart before the stock combat AI takes over.
// Off-screen history remains abstract; once the map is visible, normal Fallout
// pathfinding/combat movement is responsible for closing the distance.
static int gCoopEncounterStagingGroup = 0;
static int gCoopEncounterActiveStagingGroup = 0;

static void coopEncounterResetStaging()
{
    gCoopEncounterStagingGroup = 0;
    gCoopEncounterActiveStagingGroup = 0;
}

static int coopEncounterStagingDirection(int group)
{
    // Oppose the first two groups, then distribute additional groups around the
    // remaining map edges so three/four-sided encounters do not pile together.
    static const int directions[6] = { 0, 3, 1, 4, 2, 5 };
    return directions[group % 6];
}

static int coopEncounterStagingDistance(int group)
{
    return 16 + (group % 3) * 3;
}

static int coopEncounterFindStagingCenter(int group, int fallback)
{
    int direction = coopEncounterStagingDirection(group);
    int wanted = coopEncounterStagingDistance(group);
    for (int distance = wanted; distance >= 9; distance--) {
        int candidate = tileGetTileInDirection(gDude->tile, direction, distance);
        if (candidate >= 0 && wmEvalTileNumForPlacement(candidate)) {
            return candidate;
        }
    }
    return fallback;
}

static void coopEncounterQueueVisibleApproach(Object* object, int group)
{
    if (object == nullptr || PID_TYPE(object->pid) != OBJ_TYPE_CRITTER) {
        return;
    }

    // Walk roughly halfway toward the map/player center. Hostile groups will
    // continue from there using the ordinary combat AI; peaceful generated
    // groups still visibly enter/move instead of materializing beside a target.
    int inward = (coopEncounterStagingDirection(group) + 3) % ROTATION_COUNT;
    int stride = 5 + (group % 3);
    int destination = tileGetTileInDirection(object->tile, inward, stride);
    if (destination < 0 || !wmEvalTileNumForPlacement(destination)) {
        return;
    }

    if (reg_anim_begin(ANIMATION_REQUEST_UNRESERVED) == 0) {
        animationRegisterMoveToTile(object, destination, object->elevation, -1, 0);
        reg_anim_end();
    }
}

'''
s = s.replace(anchor, state_code + anchor, 1)

start_anchor = '''int wmSetupRandomEncounter()\n{\n    MessageListItem messageListItem;'''
if start_anchor not in s:
    raise SystemExit('wmSetupRandomEncounter anchor missing')
s = s.replace(start_anchor,
'''int wmSetupRandomEncounter()\n{\n    MessageListItem messageListItem;\n\n    coopEncounterResetStaging();''', 1)

init_anchor = '''    coopDirectEncounterBySelfPlay(encounter);\n\n    if (wmSetupRndNextTileNumInit(encounter) == -1) {'''
if init_anchor not in s:
    raise SystemExit('wmSetupCritterObjs staging anchor missing')
s = s.replace(init_anchor,
'''    coopDirectEncounterBySelfPlay(encounter);\n\n    gCoopEncounterActiveStagingGroup = gCoopEncounterStagingGroup++;\n\n    if (wmSetupRndNextTileNumInit(encounter) == -1) {''', 1)

center_anchor = '''            wmRndTileDirs[0] = tileGetRotationTo(wmRndCenterTiles[0], gDude->tile);\n            wmRndTileDirs[1] = tileGetRotationTo(wmRndCenterTiles[1], gDude->tile);\n\n            wmRndOriginalCenterTile = wmRndCenterTiles[0];'''
if center_anchor not in s:
    raise SystemExit('formation center anchor missing')
s = s.replace(center_anchor,
'''            // Stage each encounter group at a different map-side approach.\n            // Keep authored/random start points as a fallback when an edge tile\n            // is unreachable on a particular wilderness layout.\n            int stagedCenter = coopEncounterFindStagingCenter(\n                gCoopEncounterActiveStagingGroup, wmRndCenterTiles[0]);\n            wmRndCenterTiles[0] = stagedCenter;\n            wmRndCenterTiles[1] = stagedCenter;\n\n            wmRndTileDirs[0] = tileGetRotationTo(wmRndCenterTiles[0], gDude->tile);\n            wmRndTileDirs[1] = tileGetRotationTo(wmRndCenterTiles[1], gDude->tile);\n\n            wmRndOriginalCenterTile = wmRndCenterTiles[0];''', 1)

surround_anchor = '''        wmRndCenterTiles[0] = gDude->tile;\n        wmRndTileDirs[0] = randomBetween(0, ROTATION_COUNT - 1);\n\n        wmRndOriginalCenterTile = wmRndCenterTiles[0];'''
if surround_anchor not in s:
    raise SystemExit('surrounding formation anchor missing')
s = s.replace(surround_anchor,
'''        wmRndCenterTiles[0] = coopEncounterFindStagingCenter(\n            gCoopEncounterActiveStagingGroup, gDude->tile);\n        wmRndTileDirs[0] = tileGetRotationTo(wmRndCenterTiles[0], gDude->tile);\n\n        wmRndOriginalCenterTile = wmRndCenterTiles[0];''', 1)

move_anchor = '''            int direction = tileGetRotationTo(tile, gDude->tile);\n            objectSetRotation(object, direction, nullptr);\n\n            for (int itemIndex = 0; itemIndex < encounterEntry->itemsLength; itemIndex++) {'''
if move_anchor not in s:
    raise SystemExit('spawn movement anchor missing')
s = s.replace(move_anchor,
'''            int direction = tileGetRotationTo(tile, gDude->tile);\n            objectSetRotation(object, direction, nullptr);\n\n            coopEncounterQueueVisibleApproach(object, gCoopEncounterActiveStagingGroup);\n\n            for (int itemIndex = 0; itemIndex < encounterEntry->itemsLength; itemIndex++) {''', 1)

p.write_text(s, encoding='utf-8')
print('added staged encounter spawn/approach movement')
