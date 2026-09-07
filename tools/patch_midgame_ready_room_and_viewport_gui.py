from pathlib import Path


def replace_once(path: Path, old: str, new: str, label: str) -> None:
    text = path.read_text(encoding="utf-8")
    if new in text:
        print(f"{label}: already applied")
        return
    if old not in text:
        raise SystemExit(f"{label}: expected source block not found in {path}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")
    print(f"{label}: applied")


# -----------------------------------------------------------------------------
# 1) Shared request state + physical-controller START -> Baldur's Gate style room
# -----------------------------------------------------------------------------
coop = Path("src/local_coop.h")
text = coop.read_text(encoding="utf-8")
if "COOP_MIDGAME_READY_ROOM_V1" not in text:
    anchor = "inline bool gLocalCoopSystemMenuActive = false;\n"
    insert = anchor + "\n// COOP_MIDGAME_READY_ROOM_V1\n// New P2-P4 participants do not spawn directly into a running map. Their join\n// request pauses the live campaign and opens the shared party/ready room first.\ninline bool gLocalCoopMidgameReadyRoomRequested = false;\ninline bool gLocalCoopMidgameReadyRoomActive = false;\n"
    if anchor not in text:
        raise SystemExit("midgame state: system-menu anchor not found")
    text = text.replace(anchor, insert, 1)

old_join = '''        if (!player.slotLocked
            && startDown
            && !player.joinStartWasDown
            && !player.joinMenuActive) {
            localCoopOpenJoinMenu(player);
        }
'''
new_join = '''        if (!player.slotLocked
            && startDown
            && !player.joinStartWasDown
            && !player.joinMenuActive) {
            // COOP_MIDGAME_READY_ROOM_V1
            // During a live campaign START means "join party", which returns the
            // session to the shared ready room instead of spawning this player
            // directly into combat. The old compact join panel remains only as a
            // defensive fallback when no live map exists.
            if (gDude != nullptr && tileIsValid(gDude->tile)) {
                gLocalCoopMidgameReadyRoomRequested = true;
                debugPrint("[COOP GROUP] midgame ready room requested by slot=%d\\n", slot);
            } else {
                localCoopOpenJoinMenu(player);
            }
        }
'''
if "midgame ready room requested by slot=" not in text:
    if old_join not in text:
        raise SystemExit("midgame physical join: join-menu anchor not found")
    text = text.replace(old_join, new_join, 1)
coop.write_text(text, encoding="utf-8")


# -----------------------------------------------------------------------------
# 2) Phone claim during live play requests the same shared party room
# -----------------------------------------------------------------------------
mobile = Path("src/local_coop_mobile.cc")
text = mobile.read_text(encoding="utf-8")
if "COOP_MIDGAME_PHONE_READY_ROOM_V1" not in text:
    anchor = '''    player.connected = true;
    snprintf(player.controllerGuid, sizeof(player.controllerGuid), "PHOBOI-MOBILE-%d", slot + 1);
'''
    replacement = anchor + '''
    // COOP_MIDGAME_PHONE_READY_ROOM_V1
    // Selecting an unused phone slot is itself a join request. In a running
    // campaign route it through the same party-management room as a physical
    // controller instead of silently creating a live-map actor.
    if (!player.slotLocked
        && !gLocalCoopMidgameReadyRoomActive
        && gDude != nullptr
        && tileIsValid(gDude->tile)) {
        gLocalCoopMidgameReadyRoomRequested = true;
        debugPrint("[COOP GROUP] midgame ready room requested by phone slot=%d\\n", slot);
    }
'''
    if anchor not in text:
        raise SystemExit("midgame phone join: mobile controller anchor not found")
    text = text.replace(anchor, replacement, 1)
    mobile.write_text(text, encoding="utf-8")


# -----------------------------------------------------------------------------
# 3) Midgame shared ready room: keeps the live map loaded and resumes it in place
# -----------------------------------------------------------------------------
group = Path("src/local_coop_group_room.h")
text = group.read_text(encoding="utf-8")
if "#include <algorithm>" not in text:
    text = text.replace("#include <SDL.h>\n\n", "#include <SDL.h>\n\n#include <algorithm>\n", 1)

if "COOP_MIDGAME_READY_ROOM_UI_V1" not in text:
    end_anchor = "\n} // namespace fallout\n\n#endif"
    if end_anchor not in text:
        raise SystemExit("midgame ready room UI: namespace end anchor not found")

    function = r'''

// COOP_MIDGAME_READY_ROOM_UI_V1
// Baldur's Gate-style party management for a campaign that is already running.
// The current map remains loaded underneath this modal room; no scripts, world
// position, quest state, clocks, inventories or protagonist state are reset.
inline bool localCoopRunMidgameGroupRoom()
{
    gLocalCoopMidgameReadyRoomRequested = false;
    gLocalCoopMidgameReadyRoomActive = true;

    std::array<bool, kLocalCoopMaxPlayers> joined {};
    std::array<bool, kLocalCoopMaxPlayers> ready {};
    std::array<bool, kLocalCoopMaxPlayers> startWasDown {};
    std::array<bool, kLocalCoopMaxPlayers> leftWasDown {};
    std::array<bool, kLocalCoopMaxPlayers> rightWasDown {};
    std::array<bool, kLocalCoopMaxPlayers> yWasDown {};
    std::array<bool, kLocalCoopMaxPlayers> lbWasDown {};
    std::array<bool, kLocalCoopMaxPlayers> rbWasDown {};
    std::array<bool, kLocalCoopMaxPlayers> xWasDown {};

    joined[0] = true;
    for (int slot = 1; slot < kLocalCoopMaxPlayers; ++slot) {
        const LocalCoopPlayer& player = gLocalCoopPlayers[slot];
        // Existing connected party members and newly attached empty slots are
        // participants. A disconnected reserved character remains in the save
        // but does not block the ready vote.
        joined[slot] = player.connected && (player.slotLocked || player.actor == nullptr);
    }

    int kickTarget = 1;
    int sw = std::max(640, screenGetWidth());
    int sh = std::max(480, screenGetVisibleHeight());
    int width = std::max(600, std::min(920, sw - 24));
    int height = std::max(390, std::min(540, sh - 24));
    int win = windowCreate(
        (screenGetWidth() - width) / 2,
        (screenGetVisibleHeight() - height) / 2,
        width,
        height,
        _colorTable[0],
        WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    if (win == -1) {
        gLocalCoopMidgameReadyRoomActive = false;
        return false;
    }

    bool oldCursorHidden = cursorIsHidden();
    mouseShowCursor();
    bool accepted = false;
    bool dirty = true;
    bool enterWasDown = false;
    bool keyLeftWasDown = false;
    bool keyRightWasDown = false;

    auto draw = [&]() {
        windowFill(win, 0, 0, width, height, _colorTable[0]);
        windowDrawBorder(win);
        windowDrawText(win, "PHOBOI PARTY MANAGEMENT - GAME PAUSED", width - 48, 24, 20, _colorTable[992]);
        windowDrawText(win, "CURRENT MAP AND CAMPAIGN ARE PRESERVED - READY TO RETURN", width - 48, 24, 48, _colorTable[32747]);

        int rowTop = 92;
        int rowGap = std::max(48, (height - 190) / kLocalCoopMaxPlayers);
        for (int slot = 0; slot < kLocalCoopMaxPlayers; ++slot) {
            const LocalCoopPlayer& player = gLocalCoopPlayers[slot];
            const char* archetype = player.archetype >= 0 && player.archetype < kLocalCoopArchetypeCount
                ? kLocalCoopArchetypeNames[player.archetype]
                : "UNKNOWN";
            const char* sex = player.gender == GENDER_FEMALE ? "FEMALE" : "MALE";
            const char* state = ready[slot]
                ? "READY"
                : joined[slot]
                    ? (player.slotLocked ? "CURRENT CHARACTER" : "NEW - CHOOSE CLASS / SEX")
                    : player.connected
                        ? "NOT IN PARTY"
                        : (player.slotLocked ? "RESERVED / OFFLINE" : "OPEN");
            const char* target = slot == kickTarget && slot > 0 ? "  <P1 TARGET>" : "";
            char line[320];
            std::snprintf(line, sizeof(line), "P%d  %s  CLASS: %s  SEX: %s%s",
                slot + 1, state, archetype, sex, target);
            windowDrawText(win, line, width - 72, 36, rowTop + slot * rowGap,
                ready[slot] ? _colorTable[32747] : _colorTable[992]);
        }

        windowDrawText(win, "NEW PLAYER: LEFT/RIGHT = CLASS   Y = SEX   START = READY", width - 48, 24, height - 78, _colorTable[32747]);
        windowDrawText(win, "P1 HOST: LB/RB = TARGET P2-P4   X = KICK CONTROLLER / PHONE", width - 48, 24, height - 54, _colorTable[992]);
        windowDrawText(win, "EVERY ACTIVE PLAYER READIES, THEN THE SAME MAP RESUMES", width - 48, 24, height - 30, _colorTable[992]);
        windowRefresh(win);
    };

    while (_game_user_wants_to_quit == 0) {
        sharedFpsLimiter.mark();
        inputGetInput();
        localCoopMobileTick();
        localCoopRefreshControllers();

        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        bool enterDown = keys != nullptr && keys[SDL_SCANCODE_RETURN] != 0;
        bool escapeDown = keys != nullptr && keys[SDL_SCANCODE_ESCAPE] != 0;
        bool keyLeft = keys != nullptr && keys[SDL_SCANCODE_LEFT] != 0;
        bool keyRight = keys != nullptr && keys[SDL_SCANCODE_RIGHT] != 0;

        if (escapeDown) {
            break;
        }

        // A second phone/controller may arrive while this room is already open.
        // Add it to the same party-management pass instead of opening another UI.
        for (int slot = 1; slot < kLocalCoopMaxPlayers; ++slot) {
            LocalCoopPlayer& player = gLocalCoopPlayers[slot];
            if (player.connected && !player.slotLocked && !joined[slot]) {
                joined[slot] = true;
                ready[slot] = false;
                dirty = true;
            }
        }

        for (int slot = 0; slot < kLocalCoopMaxPlayers; ++slot) {
            LocalCoopPlayer& player = gLocalCoopPlayers[slot];
            bool hasController = player.controller != nullptr;
            bool startDown = hasController && SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_START) != 0;
            bool leftDown = hasController && SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT) != 0;
            bool rightDown = hasController && SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) != 0;
            bool yDown = hasController && SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_Y) != 0;
            bool lbDown = hasController && SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER) != 0;
            bool rbDown = hasController && SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) != 0;
            bool xDown = hasController && SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_X) != 0;

            bool startEdge = startDown && !startWasDown[slot];
            bool leftEdge = leftDown && !leftWasDown[slot];
            bool rightEdge = rightDown && !rightWasDown[slot];
            bool yEdge = yDown && !yWasDown[slot];
            bool lbEdge = lbDown && !lbWasDown[slot];
            bool rbEdge = rbDown && !rbWasDown[slot];
            bool xEdge = xDown && !xWasDown[slot];

            // Only a genuinely new character can change class/sex here. Existing
            // characters keep the stats they have earned in this campaign.
            if (joined[slot] && !player.slotLocked && !ready[slot]) {
                if (leftEdge) {
                    player.archetype = (player.archetype + kLocalCoopArchetypeCount - 1) % kLocalCoopArchetypeCount;
                    dirty = true;
                }
                if (rightEdge) {
                    player.archetype = (player.archetype + 1) % kLocalCoopArchetypeCount;
                    dirty = true;
                }
                if (yEdge) {
                    player.gender = player.gender == GENDER_MALE ? GENDER_FEMALE : GENDER_MALE;
                    dirty = true;
                }
            }

            bool keyboardReadyEdge = slot == 0 && enterDown && !enterWasDown;
            if (joined[slot] && (startEdge || keyboardReadyEdge)) {
                ready[slot] = !ready[slot];
                dirty = true;
            }

            if (slot == 0 && joined[0]) {
                if (lbEdge) {
                    kickTarget = kickTarget <= 1 ? 3 : kickTarget - 1;
                    dirty = true;
                }
                if (rbEdge) {
                    kickTarget = kickTarget >= 3 ? 1 : kickTarget + 1;
                    dirty = true;
                }
                if (xEdge && kickTarget > 0 && kickTarget < kLocalCoopMaxPlayers) {
                    LocalCoopPlayer& target = gLocalCoopPlayers[kickTarget];
                    bool kickedMobile = localCoopMobileKickSlot(kickTarget);
                    if (!kickedMobile && target.connected) {
                        localCoopClearController(target);
                    }
                    joined[kickTarget] = false;
                    ready[kickTarget] = false;
                    dirty = true;
                    debugPrint("[COOP GROUP] P1 midgame kicked controller slot=%d\\n", kickTarget);
                }
            }

            startWasDown[slot] = startDown;
            leftWasDown[slot] = leftDown;
            rightWasDown[slot] = rightDown;
            yWasDown[slot] = yDown;
            lbWasDown[slot] = lbDown;
            rbWasDown[slot] = rbDown;
            xWasDown[slot] = xDown;
        }

        enterWasDown = enterDown;
        keyLeftWasDown = keyLeft;
        keyRightWasDown = keyRight;

        int joinedCount = 0;
        int readyCount = 0;
        for (int slot = 0; slot < kLocalCoopMaxPlayers; ++slot) {
            if (joined[slot]) {
                ++joinedCount;
                if (ready[slot]) ++readyCount;
            }
        }

        if (joinedCount > 0 && readyCount == joinedCount) {
            bool createdAll = true;
            for (int slot = 1; slot < kLocalCoopMaxPlayers; ++slot) {
                LocalCoopPlayer& player = gLocalCoopPlayers[slot];
                if (joined[slot] && !player.slotLocked) {
                    if (!localCoopCreatePlayerActor(slot)) {
                        ready[slot] = false;
                        createdAll = false;
                        dirty = true;
                        debugPrint("[COOP GROUP] midgame create failed slot=%d; staying in room\\n", slot);
                    }
                }
            }
            if (createdAll) {
                accepted = true;
                break;
            }
        }

        if (dirty) {
            draw();
            dirty = false;
        }
        renderPresent();
        sharedFpsLimiter.throttle();
    }

    windowDestroy(win);
    if (oldCursorHidden) mouseHideCursor();
    gLocalCoopMidgameReadyRoomRequested = false;
    gLocalCoopMidgameReadyRoomActive = false;
    debugPrint("[COOP GROUP] midgame room closed accepted=%d\\n", accepted ? 1 : 0);
    return accepted;
}
'''
    text = text.replace(end_anchor, function + end_anchor, 1)
    group.write_text(text, encoding="utf-8")


# -----------------------------------------------------------------------------
# 4) Runtime opens party management as a modal pass, then resumes same map
# -----------------------------------------------------------------------------
runtime = Path("src/local_coop_runtime.h")
text = runtime.read_text(encoding="utf-8")
if '#include "local_coop_group_room.h"' not in text:
    text = text.replace('#include "local_coop.h"\n', '#include "local_coop.h"\n#include "local_coop_group_room.h"\n', 1)

if "COOP_MIDGAME_READY_ROOM_RUNTIME_V1" not in text:
    anchor = '''    localCoopUpdateP1InputSource();
    localCoopProcessJoinMenus();
    localCoopSystemMenuTick();
'''
    replacement = '''    localCoopUpdateP1InputSource();
    localCoopProcessJoinMenus();

    // COOP_MIDGAME_READY_ROOM_RUNTIME_V1
    // Party management is modal and intentionally leaves the current map loaded.
    // Tear down personal presentation windows first so the ready room is the
    // only foreground UI, then let the next runtime tick rebuild each viewport.
    if (gLocalCoopMidgameReadyRoomRequested && !gLocalCoopMidgameReadyRoomActive) {
        localCoopPersonalUiShutdown();
        localCoopDestroyHud();
        localCoopRunMidgameGroupRoom();
        gLocalCoopRuntimeInsideTick = false;
        return;
    }

    localCoopSystemMenuTick();
'''
    if anchor not in text:
        raise SystemExit("midgame runtime hook: controller-processing anchor not found")
    text = text.replace(anchor, replacement, 1)
    runtime.write_text(text, encoding="utf-8")


# -----------------------------------------------------------------------------
# 5) Personal HUD/inventory geometry follows the actual per-player camera pane
# -----------------------------------------------------------------------------
pui = Path("src/local_coop_personal_ui.h")
text = pui.read_text(encoding="utf-8")
if "COOP_VIEWPORT_SCALED_PERSONAL_UI_V1" not in text:
    state_old = '''    Uint32 nextRefreshTick = 0;
};
'''
    state_new = '''    Uint32 nextRefreshTick = 0;
    int hudX = -1;
    int hudY = -1;
    int hudWidth = -1;
    int hudHeight = -1;
    int inventoryX = -1;
    int inventoryY = -1;
    int inventoryWidth = -1;
    int inventoryHeight = -1;
};
'''
    if state_old not in text:
        raise SystemExit("personal UI: state anchor not found")
    text = text.replace(state_old, state_new, 1)

    rect_old = '''inline void localCoopPersonalUiHudRect(int slot, int& x, int& y, int& width, int& height)
{
    int sw = std::max(640, screenGetWidth());
    int sh = std::max(480, screenGetHeight());
    width = std::min(360, std::max(280, sw / 3));
    height = 112;
    x = (slot & 1) ? sw - width : 0;
    y = slot >= 2 ? sh - INTERFACE_BAR_HEIGHT - height : 0;
}

inline void localCoopPersonalUiInventoryRect(int slot, int& x, int& y, int& width, int& height)
{
    int sw = std::max(640, screenGetWidth());
    int sh = std::max(480, screenGetHeight());
    width = std::max(310, sw / 2 - 8);
    height = std::max(230, sh / 2 - 8);
    x = (slot & 1) ? sw - width : 0;
    y = slot >= 2 ? sh - height : 0;
}
'''
    rect_new = '''// COOP_VIEWPORT_SCALED_PERSONAL_UI_V1
inline bool localCoopPersonalUiViewportForSlot(int slot, LocalCoopIsoViewport& view)
{
    int sw = screenGetWidth();
    int sh = screenGetVisibleHeight();
    if (sw <= 0 || sh <= 0) return false;

    std::array<int, kLocalCoopMaxPlayers> activeSlots {};
    int count = 0;
    int ordinal = -1;
    for (int candidate = 0; candidate < kLocalCoopMaxPlayers; ++candidate) {
        const LocalCoopPlayer& player = gLocalCoopPlayers[candidate];
        if (!player.connected || !player.humanOwned || player.actor == nullptr
            || (player.actor->flags & OBJECT_HIDDEN) != 0) {
            continue;
        }
        if (candidate == slot) ordinal = count;
        activeSlots[count++] = candidate;
    }
    if (ordinal < 0 || count <= 0) return false;
    view = localCoopIsoViewportForOrdinal(ordinal, count, sw, sh);
    return view.width > 0 && view.height > 0;
}

inline void localCoopPersonalUiHudRect(int slot, int& x, int& y, int& width, int& height)
{
    LocalCoopIsoViewport view;
    if (!localCoopPersonalUiViewportForSlot(slot, view)) {
        x = y = width = height = 0;
        return;
    }
    int margin = std::clamp(std::min(view.width, view.height) / 80, 4, 12);
    width = std::min(420, std::max(220, view.width - margin * 2));
    height = std::clamp(view.height / 5, 78, 112);
    x = view.x + margin;
    y = view.y + view.height - height - margin;
}

inline void localCoopPersonalUiInventoryRect(int slot, int& x, int& y, int& width, int& height)
{
    LocalCoopIsoViewport view;
    if (!localCoopPersonalUiViewportForSlot(slot, view)) {
        x = y = width = height = 0;
        return;
    }
    int margin = std::clamp(std::min(view.width, view.height) / 30, 8, 20);
    width = std::max(240, view.width - margin * 2);
    height = std::max(180, view.height - margin * 2);
    width = std::min(width, view.width);
    height = std::min(height, view.height);
    x = view.x + (view.width - width) / 2;
    y = view.y + (view.height - height) / 2;
}
'''
    if rect_old not in text:
        raise SystemExit("personal UI: geometry block not found")
    text = text.replace(rect_old, rect_new, 1)

    close_old = '''    ui.inventoryWindow = -1;
    if (gLocalCoopPlayers[slot].uiMode == LocalCoopUiMode::Inventory) {
'''
    close_new = '''    ui.inventoryWindow = -1;
    ui.inventoryX = ui.inventoryY = ui.inventoryWidth = ui.inventoryHeight = -1;
    if (gLocalCoopPlayers[slot].uiMode == LocalCoopUiMode::Inventory) {
'''
    text = text.replace(close_old, close_new, 1)

    open_old = '''    localCoopPersonalUiInventoryRect(slot, x, y, w, h);
    ui.inventoryWindow = windowCreate(x, y, w, h, _colorTable[0], WINDOW_MOVE_ON_TOP);
    if (ui.inventoryWindow == -1) return;
    ui.equipHand = localCoopGetActiveHand(player);
'''
    open_new = '''    localCoopPersonalUiInventoryRect(slot, x, y, w, h);
    if (w <= 0 || h <= 0) return;
    ui.inventoryWindow = windowCreate(x, y, w, h, _colorTable[0], WINDOW_MOVE_ON_TOP);
    if (ui.inventoryWindow == -1) return;
    ui.inventoryX = x;
    ui.inventoryY = y;
    ui.inventoryWidth = w;
    ui.inventoryHeight = h;
    ui.equipHand = localCoopGetActiveHand(player);
'''
    if open_old not in text:
        raise SystemExit("personal UI: inventory-open anchor not found")
    text = text.replace(open_old, open_new, 1)

    hud_create_old = '''    localCoopPersonalUiHudRect(slot, x, y, w, h);
    if (ui.hudWindow == -1) ui.hudWindow = windowCreate(x, y, w, h, _colorTable[0], WINDOW_MOVE_ON_TOP);
    if (ui.hudWindow == -1) return;

    windowFill(ui.hudWindow, 0, 0, w, h, _colorTable[0]);
'''
    hud_create_new = '''    localCoopPersonalUiHudRect(slot, x, y, w, h);
    if (w <= 0 || h <= 0) {
        if (ui.hudWindow != -1) windowDestroy(ui.hudWindow);
        ui.hudWindow = -1;
        ui.hudX = ui.hudY = ui.hudWidth = ui.hudHeight = -1;
        return;
    }
    if (ui.hudWindow != -1
        && (ui.hudX != x || ui.hudY != y || ui.hudWidth != w || ui.hudHeight != h)) {
        windowDestroy(ui.hudWindow);
        ui.hudWindow = -1;
    }
    if (ui.hudWindow == -1) {
        ui.hudWindow = windowCreate(x, y, w, h, _colorTable[0], WINDOW_MOVE_ON_TOP);
        ui.hudX = x;
        ui.hudY = y;
        ui.hudWidth = w;
        ui.hudHeight = h;
    }
    if (ui.hudWindow == -1) return;

    windowFill(ui.hudWindow, 0, 0, w, h, _colorTable[0]);
'''
    if hud_create_old not in text:
        raise SystemExit("personal UI: HUD create anchor not found")
    text = text.replace(hud_create_old, hud_create_new, 1)

    # Responsive HUD line spacing.
    text = text.replace('''    windowDrawText(ui.hudWindow, line, w - 16, 8, 7, _colorTable[992]);

    Object* actor = player.actor;
''', '''    int hudStep = std::max(16, (h - 12) / 4);
    int hudTop = 5;
    windowDrawText(ui.hudWindow, line, w - 16, 8, hudTop, _colorTable[992]);

    Object* actor = player.actor;
''', 1)
    text = text.replace('windowDrawText(ui.hudWindow, "NO CHARACTER", w - 16, 8, 31, _colorTable[992]);', 'windowDrawText(ui.hudWindow, "NO CHARACTER", w - 16, 8, hudTop + hudStep, _colorTable[992]);', 1)
    text = text.replace('windowDrawText(ui.hudWindow, line, w - 16, 8, 29, _colorTable[992]);', 'windowDrawText(ui.hudWindow, line, w - 16, 8, hudTop + hudStep, _colorTable[992]);', 1)
    text = text.replace('windowDrawText(ui.hudWindow, line, w - 16, 8, 51, _colorTable[992]);', 'windowDrawText(ui.hudWindow, line, w - 16, 8, hudTop + hudStep * 2, _colorTable[992]);', 1)
    text = text.replace('windowDrawText(ui.hudWindow, line, w - 16, 8, 75, _colorTable[992]);', 'windowDrawText(ui.hudWindow, line, w - 16, 8, hudTop + hudStep * 3, _colorTable[992]);', 1)

    draw_inv_old = '''inline void localCoopPersonalUiDrawInventory(int slot)
{
    auto& ui = gLocalCoopPersonalUi[slot];
    if (ui.inventoryWindow == -1) return;
    Object* shared = localCoopGetSharedInventoryOwner();
'''
    draw_inv_new = '''inline void localCoopPersonalUiDrawInventory(int slot)
{
    auto& ui = gLocalCoopPersonalUi[slot];
    if (ui.inventoryWindow == -1) return;

    int wantedX, wantedY, wantedW, wantedH;
    localCoopPersonalUiInventoryRect(slot, wantedX, wantedY, wantedW, wantedH);
    if (wantedW <= 0 || wantedH <= 0) {
        localCoopPersonalUiCloseInventory(slot);
        return;
    }
    if (ui.inventoryX != wantedX || ui.inventoryY != wantedY
        || ui.inventoryWidth != wantedW || ui.inventoryHeight != wantedH) {
        windowDestroy(ui.inventoryWindow);
        ui.inventoryWindow = windowCreate(wantedX, wantedY, wantedW, wantedH, _colorTable[0], WINDOW_MOVE_ON_TOP);
        if (ui.inventoryWindow == -1) {
            ui.inventoryX = ui.inventoryY = ui.inventoryWidth = ui.inventoryHeight = -1;
            return;
        }
        ui.inventoryX = wantedX;
        ui.inventoryY = wantedY;
        ui.inventoryWidth = wantedW;
        ui.inventoryHeight = wantedH;
    }

    Object* shared = localCoopGetSharedInventoryOwner();
'''
    if draw_inv_old not in text:
        raise SystemExit("personal UI: inventory draw anchor not found")
    text = text.replace(draw_inv_old, draw_inv_new, 1)

    text = text.replace('''    windowDrawText(ui.inventoryWindow, "UP/DOWN SELECT  LEFT/RIGHT HAND  A EQUIP  X UNEQUIP  B/BACK CLOSE", w - 20, 10, 31, _colorTable[992]);

    constexpr int visible = 10;
''', '''    const char* controls = w >= 500
        ? "UP/DOWN SELECT  LEFT/RIGHT HAND  A EQUIP  X UNEQUIP  B/BACK CLOSE"
        : "UP/DOWN ITEM  L/R HAND  A EQUIP  X OFF  B CLOSE";
    windowDrawText(ui.inventoryWindow, controls, w - 20, 10, 31, _colorTable[992]);

    int visible = std::max(4, (h - 74) / 18);
''', 1)

    pui.write_text(text, encoding="utf-8")


# -----------------------------------------------------------------------------
# 6) Opening a personal menu must not collapse/reorder split-screen viewports
# -----------------------------------------------------------------------------
iso = Path("src/local_coop_iso_cameras.h")
text = iso.read_text(encoding="utf-8")
if "COOP_STABLE_SPLIT_WITH_PERSONAL_UI_V1" not in text:
    old = '''        if (player.connected && player.humanOwned && player.actor != nullptr
            && player.uiMode == LocalCoopUiMode::World
            && (player.actor->flags & OBJECT_HIDDEN) == 0) {
            activeSlots[playerCount++] = slot;
        }
'''
    new = '''        // COOP_STABLE_SPLIT_WITH_PERSONAL_UI_V1
        // Inventory/character overlays belong to this player's viewport; opening
        // one must not remove the player from the camera layout and reshuffle the
        // other players' panes.
        if (player.connected && player.humanOwned && player.actor != nullptr
            && (player.actor->flags & OBJECT_HIDDEN) == 0) {
            activeSlots[playerCount++] = slot;
        }
'''
    if old not in text:
        raise SystemExit("iso camera: active-slot UI-mode anchor not found")
    text = text.replace(old, new, 1)
    iso.write_text(text, encoding="utf-8")

print("Midgame ready room + per-viewport GUI patch complete")
