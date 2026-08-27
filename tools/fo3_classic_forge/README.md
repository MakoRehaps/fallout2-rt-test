# FO3 Classic Forge — Fallout RT

This tool turns the **Fallout 3 xEdit world scan** into native Fallout 1/2-style `.MAP` prototypes for `fallout2-rt-test`.

It uses Fallout 3 for **geography and location identity**, and Fallout 1/2 maps as the **classic construction material**.

## Current generator

- Reads `PhoBoi_FO3_WorldScan.zip` directly.
- Deduplicates FO3 map markers and LAND terrain records.
- Converts FO3 coordinates to exterior cells.
- Reads VHGT and derives terrain roughness.
- Classifies locations as wasteland, urban, industrial, cave, or vault.
- Parses native Fallout MAP v19/v20 headers.
- Reads present 100x100 square tile layers using MAP elevation flags.
- Automatically harvests 10x10 chunks from the FO1/FO2 map archives.
- Mosaics those chunks into new 100x100 classic layouts.
- Patches a valid classic MAP container and disables the copied top-level map script.
- Generates a `MAPS.TXT` fragment, world graph, CSV index, and JSON manifest.

`fallout2-rt-test` accepts map versions 19 and 20; the generator prefers v20 by default.

## GUI

```bat
python fo3_classic_forge.py --gui
```

## CLI

```bat
python fo3_classic_forge.py ^
  --scan "PhoBoi_FO3_WorldScan.zip" ^
  --fo1 "MAPS.zip" ^
  --fo2 "maps.zip" ^
  --output "FO3_GENERATED"
```

Quick test:

```bat
python fo3_classic_forge.py --scan PhoBoi_FO3_WorldScan.zip --fo1 MAPS.zip --fo2 maps.zip --output FO3_TEST --limit 10
```

## Output

```text
FO3_GENERATED/
  MAPS/
    F3M0000.MAP
    F3M0001.MAP
    ...
  fo3_world_manifest.json
  map_index.csv
  maps_txt_fragment.txt
  fallout_rt_world_graph.json
```

## v1 boundary

This is the physical-map generator. It does not pretend Fallout 3 `QUST/DIAL/INFO` can simply be copied into Fallout 2 scripts. The relationship scan is reserved for the next quest/dialogue/NPC compiler stage.

The generated MAP still retains the selected classic template's serialized object/script tail, although its top-level map script is disabled. A later object-section writer should replace that tail with deliberately generated objects.
