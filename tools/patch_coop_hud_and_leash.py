from pathlib import Path

# Widen co-op travel leash in local_coop.h.
p = Path('src/local_coop.h')
s = p.read_text(encoding='utf-8')
if '// COOP_WIDE_LEASH_V1' not in s:
    old = '''inline constexpr int kLocalCoopCameraTetherTiles = 18;'''
    new = '''// COOP_WIDE_LEASH_V1
// Players can range much farther apart before movement is constrained. Hard
// teleporting is reserved for true recovery, not ordinary exploration.
inline constexpr int kLocalCoopCameraTetherTiles = 60;
inline constexpr int kLocalCoopEmergencyWarpTiles = 120;'''
    if old not in s:
        raise SystemExit('camera tether constant missing')
    s = s.replace(old, new, 1)

    old_warp = '''            || actor->elevation != gDude->elevation
            || tileDistanceBetween(actor->tile, gDude->tile) > kLocalCoopCameraTetherTiles;'''
    new_warp = '''            || actor->elevation != gDude->elevation
            || tileDistanceBetween(actor->tile, gDude->tile) > kLocalCoopEmergencyWarpTiles;'''
    if old_warp not in s:
        raise SystemExit('hard warp threshold missing')
    s = s.replace(old_warp, new_warp, 1)

    p.write_text(s, encoding='utf-8')

# Replace the stock single-player bottom bar visually with a compact 4-player
# status/equipment HUD. Keep it in runtime so AP values are available.
p = Path('src/local_coop_runtime.h')
s = p.read_text(encoding='utf-8')
if '// COOP_FOUR_PLAYER_HUD_V1' not in s:
    anchor = '''inline int gLocalCoopCameraTargetTile = -1;
'''
    block = '''inline int gLocalCoopCameraTargetTile = -1;

// COOP_FOUR_PLAYER_HUD_V1
inline int gLocalCoopHudWindow = -1;
inline Uint32 gLocalCoopNextHudRefreshTick = 0;

inline void localCoopDestroyHud()
{
    if (gLocalCoopHudWindow != -1) {
        windowDestroy(gLocalCoopHudWindow);
        gLocalCoopHudWindow = -1;
    }
}

inline void localCoopEnsureHud()
{
    int width = screenGetWidth();
    int y = screenGetHeight() - INTERFACE_BAR_HEIGHT;
    if (width <= 0 || y < 0) {
        return;
    }

    if (gLocalCoopHudWindow == -1) {
        gLocalCoopHudWindow = windowCreate(0, y, width, INTERFACE_BAR_HEIGHT, _colorTable[0], WINDOW_MOVE_ON_TOP);
        if (gLocalCoopHudWindow == -1) {
            return;
        }
        if (gInterfaceBarWindow != -1) {
            interfaceBarHide();
        }
    }
}

inline void localCoopDrawHud(Uint32 now)
{
    if (!localCoopTickReached(now, gLocalCoopNextHudRefreshTick)) {
        return;
    }
    gLocalCoopNextHudRefreshTick = now + 100;

    localCoopEnsureHud();
    if (gLocalCoopHudWindow == -1) {
        return;
    }

    int width = screenGetWidth();
    int panelWidth = std::max(1, width / kLocalCoopMaxPlayers);
    windowFill(gLocalCoopHudWindow, 0, 0, width, INTERFACE_BAR_HEIGHT, _colorTable[0]);

    for (int slot = 0; slot < kLocalCoopMaxPlayers; slot++) {
        LocalCoopPlayer& player = gLocalCoopPlayers[slot];
        int x = slot * panelWidth;
        int textX = x + 10;
        int textWidth = std::max(40, panelWidth - 20);

        if (slot > 0) {
            windowDrawLine(gLocalCoopHudWindow, x, 5, x, INTERFACE_BAR_HEIGHT - 6, _colorTable[992]);
        }

        char header[64];
        snprintf(header, sizeof(header), "P%d  %s", slot + 1,
            player.connected ? "CONNECTED" : (player.slotLocked ? "RESERVED" : "EMPTY"));
        windowDrawText(gLocalCoopHudWindow, header, textWidth, textX, 8, _colorTable[992]);

        Object* actor = player.actor;
        if (actor == nullptr || !player.slotLocked) {
            windowDrawText(gLocalCoopHudWindow, "NO CHARACTER", textWidth, textX, 32, _colorTable[992]);
            continue;
        }

        int hp = actor->data.critter.hp;
        int maxHp = std::max(1, critterGetStat(actor, STAT_MAXIMUM_HIT_POINTS));
        int apHundredths = gLocalCoopRuntimeSlots[slot].actionPointsHundredths;
        if (apHundredths < 0) {
            apHundredths = critterGetStat(actor, STAT_MAXIMUM_ACTION_POINTS) * 100;
        }
        char stats[96];
        snprintf(stats, sizeof(stats), "HP %d/%d   AP %.1f", hp, maxHp, apHundredths / 100.0f);
        windowDrawText(gLocalCoopHudWindow, stats, textWidth, textX, 31, _colorTable[992]);

        Object* item = localCoopGetActiveItem(player);
        const char* itemName = item != nullptr ? protoGetName(item->pid) : nullptr;
        if (itemName == nullptr || *itemName == '\\0') {
            itemName = "UNARMED";
        }
        const char* hand = localCoopGetActiveHand(player) == HAND_LEFT ? "L" : "R";
        char equip[160];
        snprintf(equip, sizeof(equip), "[%s HAND] %s", hand, itemName);
        windowDrawText(gLocalCoopHudWindow, equip, textWidth, textX, 54, _colorTable[992]);

        if (player.archetype >= 0 && player.archetype < kLocalCoopArchetypeCount) {
            windowDrawText(gLocalCoopHudWindow, kLocalCoopArchetypeNames[player.archetype], textWidth, textX, 76, _colorTable[992]);
        }
    }

    windowRefresh(gLocalCoopHudWindow);
}
'''
    if anchor not in s:
        raise SystemExit('runtime HUD anchor missing')
    s = s.replace(anchor, block, 1)

    tick_anchor = '''    Uint32 now = SDL_GetTicks();
'''
    tick_new = '''    Uint32 now = SDL_GetTicks();
    localCoopDrawHud(now);
'''
    # Patch the runtime tick occurrence nearest the bottom by replacing the last match.
    pos = s.rfind(tick_anchor)
    if pos == -1:
        raise SystemExit('runtime tick time anchor missing')
    s = s[:pos] + s[pos:].replace(tick_anchor, tick_new, 1)

    p.write_text(s, encoding='utf-8')

print('Applied wider co-op leash and four-player bottom HUD')
