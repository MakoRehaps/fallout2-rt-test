# F3O generator placement format

`F3O` is an intermediate generated-object format used only by the isolated Fallout 3 classic generation pipeline.

It is **not a core-engine runtime dependency** and it is not loaded by the normal `fallout2-rt-test` engine. The generator uses F3O to describe what should exist and where while later compilation stages resolve those symbolic requests into deliberately generated native classic Fallout map/object data.

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

- `CATEGORY`: style/material family requested by the generator-side asset resolver.
- `STRUCTURE`: generated building/compound footprint metadata.
- `WALL_RUN`: connected run of classic wall objects to compile into the final map.
- `DOOR`: doorway position and orientation.
- `ANCHOR center`: general-purpose structure center.
- `ANCHOR loot`: container/loot placement candidate.
- `ANCHOR npc`: actor/quest placement candidate.
- `EXIT`: requested classic exit-grid transition. Fields are `side x y destination_map destination_hex destination_elevation destination_rotation`; a trailing `target=<map name>` is debug metadata.

## Coordinate spaces

`WALL_RUN`, `DOOR`, `ANCHOR`, and local `EXIT x y` coordinates use the generator's 100x100 square-grid space. The native object writer converts these positions into the classic 200x200 hex grid when building final map object records.

`EXIT destination_hex` is already a classic hex tile number. Generated maps currently use `20100` as the default arrival hex, elevation `0`.

## Isolation rule

The Fallout 3 conversion layer must remain removable from the main engine. After generation/compilation is complete, the game should need only normal classic Fallout-compatible data files. No frame-loop hook, global engine subsystem, mandatory FO3 source-data reader, or F3O loader belongs in `src/`.

This keeps the Fallout 3 conversion as an asset-production/content compiler rather than turning the base engine into a Fallout 3-specific runtime.

## Resolver direction

Prototype/style resolution is performed on the generator/compiler side. Category preferences distinguish `vault`, `industrial`, `urban`, `cave`, and `wasteland`; later passes can add finer FO1/FO2/Tactics families without changing the core engine.
