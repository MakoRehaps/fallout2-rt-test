from pathlib import Path

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
