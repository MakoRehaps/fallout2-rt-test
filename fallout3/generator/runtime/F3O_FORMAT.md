# F3O runtime placement format

`F3O` is the generated-object intermediate format used by the Fallout 3 classic generator.

It is intentionally separate from native Fallout `.MAP` serialization. The generator first decides **what** should exist and **where**; an asset resolver/runtime loader later decides which concrete Fallout PID/FID implements each symbolic role.

Example:

```text
F3O 1
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
```

Commands:

- `STRUCTURE`: declares a generated building/compound footprint.
- `WALL_RUN`: requests a connected run of compatible classic wall pieces.
- `DOOR`: punches a doorway into the owning structure and requests a compatible portal object.
- `ANCHOR center`: general-purpose structure center.
- `ANCHOR loot`: container/loot placement candidate.
- `ANCHOR npc`: actor/quest placement candidate.

The next resolver stage will map symbolic requests to source-tagged FO1/FO2/Tactics assets and ultimately to engine object creation/native MAP object records.
