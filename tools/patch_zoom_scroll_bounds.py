from pathlib import Path

# Repair the co-op medical fee compile break introduced by the downed/medical
# patch. P2-P4 are synthetic critters and do not own Fallout PC_STAT_* values;
# co-op progression is campaign-wide, so treatment cost uses the campaign PC
# level whenever the patient actor exists.
runtime_path = Path('src/local_coop_runtime.h')
runtime = runtime_path.read_text(encoding='utf-8')
bad_level = 'int level = actor != nullptr ? std::max(1, critterGetStat(actor, STAT_LEVEL)) : 1;'
good_level = 'int level = actor != nullptr ? std::max(1, pcGetStat(PC_STAT_LEVEL)) : 1;'
if bad_level in runtime:
    runtime = runtime.replace(bad_level, good_level, 1)
    runtime_path.write_text(runtime, encoding='utf-8')
    print('Repaired co-op medical fee level lookup for MSVC build')
elif good_level in runtime:
    print('Co-op medical fee level lookup already repaired')

p = Path('src/tile.cc')
s = p.read_text(encoding='utf-8')
marker = '// COOP_VIEWPORT_SCROLL_BOUNDS_V1'
if marker in s:
    print('viewport scroll bounds already applied')
    raise SystemExit(0)

old_init = '''    // In order to calculate scroll borders correctly we need to pretend we're
    // at original resolution. Since border is calculated only once at start,
    // there is not need to change it all the time.
    gTileWindowWidth = ORIGINAL_ISO_WINDOW_WIDTH;
    gTileWindowHeight = ORIGINAL_ISO_WINDOW_HEIGHT;

    tileSetCenter(hexGridWidth * (hexGridHeight / 2) + hexGridWidth / 2, TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS);
    tileSetBorder(windowWidth, windowHeight, hexGridWidth, hexGridHeight);

    // Restore actual window size and set center one more time to calculate
    // correct screen offsets, which are required for subsequent object update
    // area calculations.
    gTileWindowWidth = windowWidth;
    gTileWindowHeight = windowHeight;

    tileSetCenter(hexGridWidth * (hexGridHeight / 2) + hexGridWidth / 2, TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS);
'''
new_init = '''    // COOP_VIEWPORT_SCROLL_BOUNDS_V1
    // Co-op renders a wider logical viewport than stock Fallout. Calculate the
    // legal camera border using that real viewport so zooming/panning can never
    // reveal space beyond the map's scroll box.
    gTileWindowWidth = windowWidth;
    gTileWindowHeight = windowHeight;

    tileSetCenter(hexGridWidth * (hexGridHeight / 2) + hexGridWidth / 2, TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS);
    tileSetBorder(windowWidth, windowHeight, hexGridWidth, hexGridHeight);

    // Recenter once after the limits are established so all render/object
    // offsets are based on the same real viewport dimensions.
    tileSetCenter(hexGridWidth * (hexGridHeight / 2) + hexGridWidth / 2, TILE_SET_CENTER_FLAG_IGNORE_SCROLL_RESTRICTIONS);
'''
if old_init not in s:
    raise SystemExit('tileInit original-resolution border block not found')
s = s.replace(old_init, new_init, 1)

old_border = '''    // TODO: Borders, scroll blockers and tile system overall were designed
    // with 640x480 in mind, so using windowWidth and windowHeight is
    // meaningless for calculating borders. For now keep borders for original
    // resolution.
    int v1 = tileFromScreenXY(-320, -240, 0);
    int v2 = tileFromScreenXY(-320, ORIGINAL_ISO_WINDOW_HEIGHT + 240, 0);
'''
new_border = '''    // The stock code hard-coded a 640x480 half-viewport here. With the co-op
    // 1280x720+ logical view that allows the visible corners to cross the old
    // scroll box even while the center tile remains legal. Expand the border
    // probe by the actual half-viewport dimensions instead.
    int halfWidth = std::max(ORIGINAL_ISO_WINDOW_WIDTH / 2, windowWidth / 2);
    int halfHeight = std::max(ORIGINAL_ISO_WINDOW_HEIGHT / 2, windowHeight / 2);
    int v1 = tileFromScreenXY(-halfWidth, -halfHeight, 0);
    int v2 = tileFromScreenXY(-halfWidth, windowHeight + halfHeight, 0);
'''
if old_border not in s:
    raise SystemExit('tileSetBorder stock 640x480 block not found')
s = s.replace(old_border, new_border, 1)

p.write_text(s, encoding='utf-8')
print('Updated tile scroll limits for the real co-op viewport')
