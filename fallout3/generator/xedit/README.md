# xEdit Scan Input

This folder documents the Fallout 3 xEdit scanner/export contract consumed by the generator.

The scanner should keep physical-world data separate from global game semantics:

- world pass: WRLD / CELL / LAND / REFR / ACHR / ACRE / doors / markers
- game-data pass: QUST / DIAL / INFO / NPC_ / CREA / FACT / PACK / SCPT / REGN / ECZN / leveled lists
- relationship pass: explicit cross-references connecting quests/dialogue/scripts back to actors, refs, cells and worldspaces

Expected generator inputs include JSONL files such as:
- world.jsonl
- cells.jsonl
- terrain.jsonl
- references.jsonl
- markers.jsonl
- quests.jsonl
- dialogue.jsonl
- semantic_records.jsonl
- relationships.jsonl
