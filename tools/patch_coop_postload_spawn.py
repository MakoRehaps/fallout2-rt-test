from pathlib import Path

path = Path('src/main.cc')
text = path.read_text(encoding='utf-8')
marker = '// COOP_POSTLOAD_PREJOIN_SPAWN_V1'
if marker in text:
    print('post-load co-op spawn patch already applied')
    raise SystemExit(0)

needle = '                        _main_load_new(mapNameCopy);\n'
if needle not in text:
    raise SystemExit('could not find unified new-game _main_load_new call')

replacement = needle + '''\n                        // COOP_POSTLOAD_PREJOIN_SPAWN_V1\n                        // The ready-room vote is authoritative. Spawn P2-P4 immediately\n                        // after the first real map finishes loading instead of waiting for\n                        // the gameplay ticker. This removes the handoff gap seen in logs\n                        // where p2=1 transfers successfully but no live actor is ever made.\n                        localCoopMobileTick();\n                        localCoopRefreshControllers();\n                        debugPrint("[COOP PREJOIN] post-load handoff p1=%d p2=%d p3=%d p4=%d\\n",\n                            gLocalCoopPrejoinedSlots[0] ? 1 : 0,\n                            gLocalCoopPrejoinedSlots[1] ? 1 : 0,\n                            gLocalCoopPrejoinedSlots[2] ? 1 : 0,\n                            gLocalCoopPrejoinedSlots[3] ? 1 : 0);\n                        localCoopSpawnPrejoinedPlayers();\n'''
text = text.replace(needle, replacement, 1)
path.write_text(text, encoding='utf-8')
print('applied direct post-load co-op spawn handoff')
