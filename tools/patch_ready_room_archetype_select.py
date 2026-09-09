from pathlib import Path

path = Path(__file__).resolve().parents[1] / "src" / "local_coop_group_room.h"
text = path.read_text(encoding="utf-8")

old_line = '''                std::snprintf(line, sizeof(line), "PLAYER %d   %s", slot + 1, state);\n                windowDrawText(win, line, width - 72, 38, 126 + slot * 54,\n                    ready[slot] ? _colorTable[32747] : _colorTable[992]);\n'''
new_line = '''                const char* archetype = (player.archetype >= 0 && player.archetype < kLocalCoopArchetypeCount)\n                    ? kLocalCoopArchetypeNames[player.archetype]\n                    : "UNKNOWN";\n                std::snprintf(line, sizeof(line), "PLAYER %d   %s   CLASS: %s", slot + 1, state, archetype);\n                windowDrawText(win, line, width - 72, 38, 126 + slot * 54,\n                    ready[slot] ? _colorTable[32747] : _colorTable[992]);\n'''
if old_line in text:
    text = text.replace(old_line, new_line, 1)
elif "CLASS: %s" not in text:
    raise SystemExit("draw line anchor not found")

old_help = '''            windowDrawText(win, "ALL JOINED PLAYERS MUST VOTE READY", width - 48, 24, height - 54, _colorTable[992]);\n            windowDrawText(win, "ESC = CANCEL", width - 48, 24, height - 30, _colorTable[992]);\n'''
new_help = '''            windowDrawText(win, "JOINED + NOT READY: D-PAD LEFT/RIGHT = CHOOSE CLASS", width - 48, 24, height - 78, _colorTable[32747]);\n            windowDrawText(win, "ALL JOINED PLAYERS MUST VOTE READY", width - 48, 24, height - 54, _colorTable[992]);\n            windowDrawText(win, "ESC = CANCEL", width - 48, 24, height - 30, _colorTable[992]);\n'''
if old_help in text:
    text = text.replace(old_help, new_help, 1)
elif "CHOOSE CLASS" not in text:
    raise SystemExit("help anchor not found")

old_logic = '''            } else if (startEdge || (slot == 0 && enterDown && !enterWasDown)) {\n                if (!joined[slot]) {\n                    joined[slot] = true;\n                    ready[slot] = false;\n                    gLocalCoopPrejoinedSlots[slot] = true;\n                    dirty = true;\n                    debugPrint("[COOP GROUP] slot=%d joined\\n", slot);\n                } else {\n                    ready[slot] = !ready[slot];\n                    dirty = true;\n                    debugPrint("[COOP GROUP] slot=%d ready=%d\\n", slot, ready[slot] ? 1 : 0);\n                }\n            }\n'''
new_logic = '''            } else {\n                // COOP_READY_ROOM_ARCHETYPE_SELECT_V1\n                // A joined player chooses their own class before locking READY.\n                // This is deliberately per-slot, so P2-P4 do not inherit the\n                // default BRUISER archetype just because they joined in the room.\n                if (joined[slot] && !ready[slot] && leftEdge) {\n                    player.archetype = (player.archetype + kLocalCoopArchetypeCount - 1) % kLocalCoopArchetypeCount;\n                    dirty = true;\n                    debugPrint("[COOP GROUP] slot=%d class=%s\\n", slot, kLocalCoopArchetypeNames[player.archetype]);\n                }\n                if (joined[slot] && !ready[slot] && rightEdge) {\n                    player.archetype = (player.archetype + 1) % kLocalCoopArchetypeCount;\n                    dirty = true;\n                    debugPrint("[COOP GROUP] slot=%d class=%s\\n", slot, kLocalCoopArchetypeNames[player.archetype]);\n                }\n\n                if (startEdge || (slot == 0 && enterDown && !enterWasDown)) {\n                    if (!joined[slot]) {\n                        joined[slot] = true;\n                        ready[slot] = false;\n                        gLocalCoopPrejoinedSlots[slot] = true;\n                        dirty = true;\n                        debugPrint("[COOP GROUP] slot=%d joined class=%s\\n", slot, kLocalCoopArchetypeNames[player.archetype]);\n                    } else {\n                        ready[slot] = !ready[slot];\n                        dirty = true;\n                        debugPrint("[COOP GROUP] slot=%d ready=%d class=%s\\n", slot, ready[slot] ? 1 : 0, kLocalCoopArchetypeNames[player.archetype]);\n                    }\n                }\n            }\n'''
if old_logic in text:
    text = text.replace(old_logic, new_logic, 1)
elif "COOP_READY_ROOM_ARCHETYPE_SELECT_V1" not in text:
    raise SystemExit("ready room logic anchor not found")

path.write_text(text, encoding="utf-8")
print("patched ready-room per-player archetype selection")
