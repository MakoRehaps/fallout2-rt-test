from pathlib import Path

READY_MARKER = '// COOP_READY_ROOM_SEX_AND_HOST_KICK_V1'
MOBILE_MARKER = '// COOP_READY_ROOM_HOST_KICK_MOBILE_V1'


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise SystemExit(f'{label} anchor not found')
    return text.replace(old, new, 1)


# Export a safe phone-session kick. This invalidates the browser token and
# detaches the SDL virtual controller through the mobile backend that owns it.
p = Path('src/local_coop_mobile.h')
s = p.read_text(encoding='utf-8')
if 'bool localCoopMobileKickSlot(int slot);' not in s:
    s = replace_once(
        s,
        'void localCoopMobileTick();\n',
        'void localCoopMobileTick();\nbool localCoopMobileKickSlot(int slot);\n',
        'local_coop_mobile.h declaration')
    p.write_text(s, encoding='utf-8')

p = Path('src/local_coop_mobile.cc')
s = p.read_text(encoding='utf-8')
if MOBILE_MARKER not in s:
    anchor = '} // namespace\n\nvoid localCoopMobileTick()\n'
    helper = r''' } // namespace

// COOP_READY_ROOM_HOST_KICK_MOBILE_V1
bool localCoopMobileKickSlot(int slot)
{
    if (slot <= 0 || slot >= kLocalCoopMaxPlayers) {
        return false;
    }

    MobileSlotState& state = gMobileSlots[slot];
    MobileVirtualDevice& device = gMobileDevices[slot];
    bool hadMobileSession = state.claimed.load() || device.deviceIndex >= 0;
    if (!hadMobileSession) {
        return false;
    }

    mobileResetInput(state);
    state.claimed.store(false);
    state.token.store(0);
    state.lastSeen.store(0);

    if (device.deviceIndex >= 0) {
        mobileDetachController(slot);
    }

    mobileDrawHostWindow();
    debugPrint("[COOP GROUP] P1 kicked PhoBoi mobile slot=%d\n", slot);
    return true;
}

void localCoopMobileTick()
'''
    # Keep the namespace close exactly once; helper is exported in fallout namespace.
    helper = helper.replace(' } // namespace\n', '} // namespace\n', 1)
    s = replace_once(s, anchor, helper, 'local_coop_mobile.cc export point')
    p.write_text(s, encoding='utf-8')

# Ready room: every joined player can choose class + sex before readying.
# P1 can select P2-P4 with LB/RB and toggle a session kick with X. Kicked slots
# cannot rejoin until P1 presses X on that slot again. Phone kicks also invalidate
# the PhoBoi session token immediately.
p = Path('src/local_coop_group_room.h')
s = p.read_text(encoding='utf-8')
if READY_MARKER not in s:
    s = replace_once(
        s,
        '// COOP_UNIFORM_READY_ROOM_FLOW_V1\n',
        '// COOP_UNIFORM_READY_ROOM_FLOW_V1\n// COOP_READY_ROOM_SEX_AND_HOST_KICK_V1\n',
        'ready-room marker')

    s = replace_once(
        s,
        '''    std::array<bool, kLocalCoopMaxPlayers> leftWasDown {};\n    std::array<bool, kLocalCoopMaxPlayers> rightWasDown {};\n\n    joined[0] = true;\n''',
        '''    std::array<bool, kLocalCoopMaxPlayers> leftWasDown {};\n    std::array<bool, kLocalCoopMaxPlayers> rightWasDown {};\n    std::array<bool, kLocalCoopMaxPlayers> yWasDown {};\n    std::array<bool, kLocalCoopMaxPlayers> lbWasDown {};\n    std::array<bool, kLocalCoopMaxPlayers> rbWasDown {};\n    std::array<bool, kLocalCoopMaxPlayers> xWasDown {};\n    std::array<bool, kLocalCoopMaxPlayers> hostKicked {};\n    int kickTarget = 1;\n\n    joined[0] = true;\n''',
        'ready-room input state')

    old_draw = '''                char line[180];\n                const char* state = ready[slot]\n                    ? "READY"\n                    : joined[slot]\n                        ? "CHOOSE CLASS - THEN READY"\n                        : player.connected\n                            ? "PRESS START TO JOIN"\n                            : (slot == 0 ? "PRESS ENTER OR START TO JOIN" : "WAITING FOR CONTROLLER / PHONE");\n                const char* archetype = (player.archetype >= 0 && player.archetype < kLocalCoopArchetypeCount)\n                    ? kLocalCoopArchetypeNames[player.archetype]\n                    : "UNKNOWN";\n                std::snprintf(line, sizeof(line), "PLAYER %d   %s   CLASS: %s", slot + 1, state, archetype);\n                windowDrawText(win, line, width - 72, 38, 126 + slot * 54,\n                    ready[slot] ? _colorTable[32747] : _colorTable[992]);\n            }\n\n            windowDrawText(win, "EVERY PLAYER: LEFT/RIGHT = CLASS   START = READY", width - 48, 24, height - 78, _colorTable[32747]);\n            windowDrawText(win, "P1 KEYBOARD FALLBACK: ARROWS = CLASS   ENTER = READY", width - 48, 24, height - 54, _colorTable[992]);\n            windowDrawText(win, "ESC = CANCEL", width - 48, 24, height - 30, _colorTable[992]);\n'''
    new_draw = '''                char line[256];\n                const char* state = hostKicked[slot]\n                    ? "KICKED BY P1"\n                    : ready[slot]\n                        ? "READY"\n                        : joined[slot]\n                            ? "CHOOSE CLASS / SEX - THEN READY"\n                            : player.connected\n                                ? "PRESS START TO JOIN"\n                                : (slot == 0 ? "PRESS ENTER OR START TO JOIN" : "WAITING FOR CONTROLLER / PHONE");\n                const char* archetype = (player.archetype >= 0 && player.archetype < kLocalCoopArchetypeCount)\n                    ? kLocalCoopArchetypeNames[player.archetype]\n                    : "UNKNOWN";\n                const char* sex = player.gender == GENDER_FEMALE ? "FEMALE" : "MALE";\n                const char* kickCursor = slot == kickTarget && slot > 0 ? "  <P1 TARGET>" : "";\n                std::snprintf(line, sizeof(line), "PLAYER %d   %s   CLASS: %s   SEX: %s%s",\n                    slot + 1, state, archetype, sex, kickCursor);\n                windowDrawText(win, line, width - 72, 38, 126 + slot * 54,\n                    ready[slot] ? _colorTable[32747] : _colorTable[992]);\n            }\n\n            windowDrawText(win, "ALL PLAYERS: LEFT/RIGHT = CLASS   Y = SEX   START = READY", width - 48, 24, height - 78, _colorTable[32747]);\n            windowDrawText(win, "P1 HOST: LB/RB = TARGET P2-P4   X = KICK / ALLOW", width - 48, 24, height - 54, _colorTable[992]);\n            windowDrawText(win, "P1 KEYBOARD: ARROWS = CLASS   ENTER = READY   ESC = CANCEL", width - 48, 24, height - 30, _colorTable[992]);\n'''
    s = replace_once(s, old_draw, new_draw, 'ready-room draw block')

    old_buttons = '''            bool rightDown = hasController\n                && SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) != 0;\n\n            bool startEdge = startDown && !startWasDown[slot];\n            bool aEdge = aDown && !aWasDown[slot];\n            bool leftEdge = leftDown && !leftWasDown[slot];\n            bool rightEdge = rightDown && !rightWasDown[slot];\n'''
    new_buttons = '''            bool rightDown = hasController\n                && SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) != 0;\n            bool yDown = hasController\n                && SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_Y) != 0;\n            bool lbDown = hasController\n                && SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER) != 0;\n            bool rbDown = hasController\n                && SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) != 0;\n            bool xDown = hasController\n                && SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_X) != 0;\n\n            bool startEdge = startDown && !startWasDown[slot];\n            bool aEdge = aDown && !aWasDown[slot];\n            bool leftEdge = leftDown && !leftWasDown[slot];\n            bool rightEdge = rightDown && !rightWasDown[slot];\n            bool yEdge = yDown && !yWasDown[slot];\n            bool lbEdge = lbDown && !lbWasDown[slot];\n            bool rbEdge = rbDown && !rbWasDown[slot];\n            bool xEdge = xDown && !xWasDown[slot];\n'''
    s = replace_once(s, old_buttons, new_buttons, 'ready-room buttons')

    # Prevent a kicked slot from joining or changing its setup until P1 allows it.
    old_else = '''            } else {\n                // COOP_READY_ROOM_ARCHETYPE_SELECT_V1\n'''
    new_else = '''            } else {\n                if (hostKicked[slot] && slot > 0) {\n                    // P1 owns the pre-game party. Keep this controller/phone\n                    // visible but barred from joining until P1 toggles ALLOW.\n                } else {\n                // COOP_READY_ROOM_ARCHETYPE_SELECT_V1\n'''
    s = replace_once(s, old_else, new_else, 'ready-room post-tutorial gate')

    old_after_class = '''                if (joined[slot] && !ready[slot] && classRightEdge) {\n                    player.archetype = (player.archetype + 1) % kLocalCoopArchetypeCount;\n                    dirty = true;\n                    debugPrint("[COOP GROUP] slot=%d class=%s\\n", slot, kLocalCoopArchetypeNames[player.archetype]);\n                }\n\n                if (startEdge || (slot == 0 && enterDown && !enterWasDown)) {\n'''
    new_after_class = '''                if (joined[slot] && !ready[slot] && classRightEdge) {\n                    player.archetype = (player.archetype + 1) % kLocalCoopArchetypeCount;\n                    dirty = true;\n                    debugPrint("[COOP GROUP] slot=%d class=%s\\n", slot, kLocalCoopArchetypeNames[player.archetype]);\n                }\n                if (joined[slot] && !ready[slot] && yEdge) {\n                    player.gender = player.gender == GENDER_MALE ? GENDER_FEMALE : GENDER_MALE;\n                    dirty = true;\n                    debugPrint("[COOP GROUP] slot=%d sex=%s\\n", slot,\n                        player.gender == GENDER_FEMALE ? "FEMALE" : "MALE");\n                }\n\n                if (startEdge || (slot == 0 && enterDown && !enterWasDown)) {\n'''
    s = replace_once(s, old_after_class, new_after_class, 'ready-room sex toggle')

    old_close_else = '''                    }\n                }\n            }\n\n            startWasDown[slot] = startDown;\n            aWasDown[slot] = aDown;\n            leftWasDown[slot] = leftDown;\n            rightWasDown[slot] = rightDown;\n'''
    new_close_else = '''                    }\n                }\n                }\n\n                if (slot == 0 && joined[0]) {\n                    if (lbEdge) {\n                        kickTarget = kickTarget <= 1 ? 3 : kickTarget - 1;\n                        dirty = true;\n                    }\n                    if (rbEdge) {\n                        kickTarget = kickTarget >= 3 ? 1 : kickTarget + 1;\n                        dirty = true;\n                    }\n                    if (xEdge && kickTarget > 0 && kickTarget < kLocalCoopMaxPlayers) {\n                        if (hostKicked[kickTarget]) {\n                            hostKicked[kickTarget] = false;\n                            dirty = true;\n                            debugPrint("[COOP GROUP] P1 allowed slot=%d\\n", kickTarget);\n                        } else if (joined[kickTarget] || gLocalCoopPlayers[kickTarget].connected) {\n                            localCoopMobileKickSlot(kickTarget);\n                            joined[kickTarget] = false;\n                            ready[kickTarget] = false;\n                            gLocalCoopPrejoinedSlots[kickTarget] = false;\n                            hostKicked[kickTarget] = true;\n                            dirty = true;\n                            debugPrint("[COOP GROUP] P1 kicked slot=%d\\n", kickTarget);\n                        }\n                    }\n                }\n            }\n\n            startWasDown[slot] = startDown;\n            aWasDown[slot] = aDown;\n            leftWasDown[slot] = leftDown;\n            rightWasDown[slot] = rightDown;\n            yWasDown[slot] = yDown;\n            lbWasDown[slot] = lbDown;\n            rbWasDown[slot] = rbDown;\n            xWasDown[slot] = xDown;\n'''
    s = replace_once(s, old_close_else, new_close_else, 'ready-room host kick block')

    p.write_text(s, encoding='utf-8')

print('Ready room now supports per-player sex selection and P1 host kick controls')
