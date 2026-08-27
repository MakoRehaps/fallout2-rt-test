# F3O runtime placement format

`F3O` is the generated-object intermediate format used by the Fallout 3 classic generator and consumed directly by `src/fo3_runtime_layout.h`.

The generator decides **what** should exist and **where**. The engine resolves symbolic roles against the installed Fallout prototype data, so the generator does not need to hardcode one particular Fallout 2 PID set.

Example:

```text
F3O 2
MAP F3M0007
CATEGORY urban
STRUCTURE F3M0007_S00 ruined_house 20 18 12 9 ruin=0.650
WALL_RUN F3M0007_S00 north 20 18 12
WALL_RUN F3M0007_S00 south 20 26 12
WALL_RUN F3M0007_S00 west 20 18 9
WALL_RUN F3M0007_S00 east 31 18 9
DOOR F3M0007_S00 26 26 south
ANCHOR F3M0007_S00 center 26 22
ANCHOR F3M0007_S00 loot 25 21
ANCHOR F3M0007_S00 npc 27 23
EXIT east 98 50 214 20100 0 4 target=F3M0014
```

Commands:

- `CATEGORY`: style/material family used by the runtime prototype resolver.
- `STRUCTURE`: generated building/compound footprint metadata.
- `WALL_RUN`: connected run of classic wall objects.
- `DOOR`: doorway position; the runtime selects an installed door scenery prototype.
- `ANCHOR center`: general-purpose structure center.
- `ANCHOR loot`: becomes a compatible item-container object.
- `ANCHOR npc`: reserved for the FO3 actor-semantic resolver.
- `EXIT`: creates an engine exit-grid object. Fields are `side x y destination_map destination_hex destination_elevation destination_rotation`; a trailing `target=<map name>` is human-readable/debug metadata.

## Coordinate spaces

`WALL_RUN`, `DOOR`, `ANCHOR`, and the local `EXIT x y` coordinates use the generator's 100x100 square-grid space. The runtime converts these positions into the classic 200x200 hex grid.

`EXIT destination_hex` is already a classic hex tile number. Generated maps currently use `20100` as the default arrival hex, elevation `0`.

## Runtime persistence

Generated F3O objects are marked `OBJECT_NO_SAVE`. They are reconstructed once when an `F3Mxxxx.MAP` becomes active, which prevents generated walls, doors, containers, and exits from duplicating inside savegames.

## Resolver behavior

The current engine resolver examines loaded prototype metadata directly:

- wall material for generated wall runs;
- scenery subtype/material for doors;
- item subtype/material for loot containers;
- the built-in exit-grid PID range for map transitions.

Category preferences currently distinguish `vault`, `industrial`, `urban`, `cave`, and `wasteland`. Later passes can add finer FO1/FO2/Tactics style families without changing F3O geometry.
