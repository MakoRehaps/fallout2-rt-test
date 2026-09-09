from pathlib import Path

p = Path('src/local_coop.h')
s = p.read_text(encoding='utf-8')
marker = 'COOP_P1_EXIT_GRID_AUTHORITY_V1'
if marker in s:
    print('already patched')
    raise SystemExit(0)

old = '''        if (isExitGridAt(destination, actor->elevation)) {
            localCoopMarkMapExitTile(destination);
            debugPrint(
                "[COOP MAP EXIT] slot=%d actorId=%d tile=%d elevation=%d\\n",
                player.slot,
                actor->id,
                destination,
                actor->elevation);
            scriptsRequestWorldMap();
        }
'''
if old not in s:
    raise SystemExit('exit-grid block anchor missing')

new = '''        if (isExitGridAt(destination, actor->elevation)) {
            // COOP_P1_EXIT_GRID_AUTHORITY_V1
            // Every human may roam to an exit grid, but only the story/map
            // leader (P1) can transition the shared team to another map/world.
            // P2-P4 touching the grid never pulls the party out from under P1.
            if (player.slot == 0 && actor == gDude) {
                localCoopMarkMapExitTile(destination);
                debugPrint(
                    "[COOP MAP EXIT] P1 leader actorId=%d tile=%d elevation=%d\\n",
                    actor->id,
                    destination,
                    actor->elevation);
                scriptsRequestWorldMap();
            } else {
                debugPrint(
                    "[COOP MAP EXIT] slot=%d ignored; P1 owns team transitions tile=%d elevation=%d\\n",
                    player.slot,
                    destination,
                    actor->elevation);
            }
        }
'''

s = s.replace(old, new, 1)
p.write_text(s, encoding='utf-8')
print('P1 is now sole exit-grid/world-map authority')
