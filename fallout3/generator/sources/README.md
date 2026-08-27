# Generator Source Libraries

This directory defines the source libraries the Fallout 3 classic generator may mine.

## Sources

- `fallout1` — classic Fallout 1 MAP construction vocabulary.
- `fallout2` — classic Fallout 2 MAP construction vocabulary and preferred native output container.
- `tactics` — optional Fallout Tactics DLC source. Tactics data is never loaded directly by fallout2-rt-test; it is converted into an abstract prefab/layout vocabulary first.

Every mined part should eventually carry provenance and tags:

```json
{
  "source_game": "FO2",
  "source_map": "example.map",
  "kind": "room_corner",
  "theme": ["urban", "ruined"],
  "connectors": {"north":"wall","east":"doorway","south":"open","west":"wall"}
}
```

The final generated game remains Fallout 1/2-engine compatible regardless of source library.
