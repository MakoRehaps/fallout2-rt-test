from pathlib import Path

path = Path('src/local_coop_group_room.h')
text = path.read_text(encoding='utf-8')

old = '''                const char* state = ready[slot]\n                    ? "READY"\n                    : joined[slot]\n                        ? "JOINED - PRESS START TO READY"\n                        : player.connected\n                            ? "PRESS START TO JOIN"\n                            : "WAITING FOR CONTROLLER / PHONE";\n                if (slot == 0 && player.controller == nullptr && joined[slot] && !ready[slot]) {\n                    state = "JOINED - ENTER OR START TO READY";\n                }\n'''
new = '''                const char* state = ready[slot]\n                    ? "READY"\n                    : joined[slot]\n                        ? "CHOOSE CLASS - THEN READY"\n                        : player.connected\n                            ? "PRESS START TO JOIN"\n                            : (slot == 0 ? "PRESS ENTER OR START TO JOIN" : "WAITING FOR CONTROLLER / PHONE");\n'''
if old not in text:
    raise SystemExit('uniform state block not found')
text = text.replace(old, new, 1)

old = '''            windowDrawText(win, "JOINED + NOT READY: D-PAD LEFT/RIGHT = CHOOSE CLASS", width - 48, 24, height - 78, _colorTable[32747]);\n            windowDrawText(win, "ALL JOINED PLAYERS MUST VOTE READY", width - 48, 24, height - 54, _colorTable[992]);\n'''
new = '''            windowDrawText(win, "EVERY PLAYER: LEFT/RIGHT = CLASS   START = READY", width - 48, 24, height - 78, _colorTable[32747]);\n            windowDrawText(win, "P1 KEYBOARD FALLBACK: ARROWS = CLASS   ENTER = READY", width - 48, 24, height - 54, _colorTable[992]);\n'''
if old not in text:
    raise SystemExit('uniform footer block not found')
text = text.replace(old, new, 1)

old = '''                if (joined[slot] && !ready[slot] && leftEdge) {\n                    player.archetype = (player.archetype + kLocalCoopArchetypeCount - 1) % kLocalCoopArchetypeCount;\n                    dirty = true;\n                    debugPrint("[COOP GROUP] slot=%d class=%s\\n", slot, kLocalCoopArchetypeNames[player.archetype]);\n                }\n                if (joined[slot] && !ready[slot] && rightEdge) {\n                    player.archetype = (player.archetype + 1) % kLocalCoopArchetypeCount;\n                    dirty = true;\n                    debugPrint("[COOP GROUP] slot=%d class=%s\\n", slot, kLocalCoopArchetypeNames[player.archetype]);\n                }\n\n                if (startEdge || (slot == 0 && enterDown && !enterWasDown)) {\n'''
new = '''                // COOP_UNIFORM_READY_ROOM_V1\n                // All four slots use the same JOIN -> CLASS -> READY flow. P1's\n                // keyboard arrows/Enter are only a fallback for a keyboard-only\n                // host; controller/phone behavior is identical for every slot.\n                bool classLeftEdge = leftEdge || (slot == 0 && keyLeft && !keyLeftWasDown);\n                bool classRightEdge = rightEdge || (slot == 0 && keyRight && !keyRightWasDown);\n                if (joined[slot] && !ready[slot] && classLeftEdge) {\n                    player.archetype = (player.archetype + kLocalCoopArchetypeCount - 1) % kLocalCoopArchetypeCount;\n                    dirty = true;\n                    debugPrint("[COOP GROUP] slot=%d class=%s\\n", slot, kLocalCoopArchetypeNames[player.archetype]);\n                }\n                if (joined[slot] && !ready[slot] && classRightEdge) {\n                    player.archetype = (player.archetype + 1) % kLocalCoopArchetypeCount;\n                    dirty = true;\n                    debugPrint("[COOP GROUP] slot=%d class=%s\\n", slot, kLocalCoopArchetypeNames[player.archetype]);\n                }\n\n                if (startEdge || (slot == 0 && enterDown && !enterWasDown)) {\n'''
if old not in text:
    raise SystemExit('class selection block not found')
text = text.replace(old, new, 1)

# P1 is the mandatory story slot, so it remains joined from room entry, but its
# class and ready controls are now the same as every other joined slot.
marker = '// COOP_TILELESS_GROUP_ROOM_V2\n'
if '// COOP_UNIFORM_READY_ROOM_FLOW_V1\n' not in text:
    text = text.replace(marker, marker + '// COOP_UNIFORM_READY_ROOM_FLOW_V1\n', 1)

path.write_text(text, encoding='utf-8')
print('Patched uniform ready room')
