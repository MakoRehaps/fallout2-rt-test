# Fallout 3 Classic Generator

This folder is the dedicated Fallout 3 -> classic Fallout 1/2 generation pipeline for `fallout2-rt-test`.

The generator consumes Fallout 3 xEdit scan data plus user-supplied Fallout 1/2 map archives, then creates a connected classic-style Fallout 3 world. Generated runtime geometry is described by `.F3O` sidecars and instantiated by the engine.

## One-command build

Windows:

```bat
RUN_AUTO_BUILD.bat
```

Python:

```text
python build_fallout3.py --scan <PhoBoi_FO3_WorldScan.zip> --fo1 <FO1_MAPS.zip> --fo2 <FO2_maps.zip> --output <FO3_GENERATED>
```

Optional Fallout Tactics material can be indexed with `--tactics` when a DLC profile gives Tactics a non-zero source weight.

## Current automatic pipeline

1. **Fallout 3 scan input**
   - WRLD / CELL / LAND terrain
   - placed references
   - map markers
   - actors and creature bases
   - doors and destinations
   - factions / quests / dialogue / scripts
   - relationship graph

2. **Surface map generation**
   - classifies FO3 locations as wasteland, urban, industrial, cave, or vault
   - harvests compatible square-tile chunks from Fallout 1/2 maps
   - synthesizes 100x100 classic tile layers
   - writes generated `F3Mxxxx.MAP` files
   - allocates MAPS.TXT IDs
   - builds a connected surface world graph

3. **Control imagery**
   - terrain roughness
   - settlement/reference density
   - buildability
   - portable PGM control maps plus coordinate metadata

4. **Structure generation**
   - houses / shops / apartment shells
   - warehouses / factories / checkpoints
   - shacks / camps / ruins
   - vault/service structures
   - deterministic footprints, doors, ruin amounts, loot anchors, and NPC anchors

5. **F3O runtime compilation**
   - wall runs
   - real door positions
   - loot/container anchors
   - reserved NPC anchors
   - surface map-to-map exit-grid transitions
   - surface-to-metro transitions

6. **Engine runtime resolver**
   - `src/fo3_runtime_layout.h` loads matching `.F3O` files for generated maps
   - resolves wall, door, and container prototypes from the game's loaded prototype database
   - uses category/material preferences instead of one hardcoded PID
   - instantiates real blocking walls, scenery doors, containers, and exit-grid objects
   - generated runtime objects are `OBJECT_NO_SAVE` and are reconstructed on map entry

7. **Metro/subway generation**
   - detects FO3 metro/station locations
   - creates a concourse and platform map for each station
   - creates tunnel-segment maps between connected stations
   - writes real generated `F3Uxxxx.MAP` underground maps
   - writes matching `.F3O` transition sidecars
   - connects underground maps back to their `F3Mxxxx` surface locations
   - emits a combined surface + subway MAPS.TXT fragment

8. **Validation**
   - generated MAP files must exist and contain a valid-sized header/body
   - every generated surface/underground map must have an F3O sidecar
   - generated map IDs/names must be unique
   - world graph links must reference known maps
   - every F3O EXIT must target a known generated map ID and valid hex/elevation/rotation
   - required control imagery and structure plans must exist

## Output layout

```text
FO3_GENERATED/
  AUTO_BUILD_COMPLETE.json
  active_profile.json
  maps/
    MAPS/
      F3M0000.MAP
      F3M0000.F3O
      ...
      F3U0000.MAP
      F3U0000.F3O
      ...
    fo3_world_manifest.json
    fallout_rt_world_graph.json
    maps_txt_fragment.txt
    maps_txt_fragment_complete.txt
  control_maps/
    terrain_roughness.pgm
    settlement_density.pgm
    buildability.pgm
    control_map_meta.json
  structures/
    structure_plans.json
  subway/
    subway_graph.json
    subway_manifest.json
    subway_maps_txt_fragment.txt
  runtime/
    runtime_index.json
```

## Source and DLC policy

Fallout 1 and Fallout 2 are the primary classic construction vocabulary. Fallout Tactics is an optional DLC/source library and is not treated as binary-compatible with Fallout 2 maps. Tactics material is indexed with provenance and is intended to be translated into abstract layout/style information before being rebuilt using compatible classic engine assets.

## Current limitations / next stages

The generated surface and subway `.MAP` files still use a classic map as their binary container. Their tile layer is generated, but the original serialized object/script tail is not yet replaced by a clean native object section. The F3O runtime layer deliberately provides generated walls, doors, containers, and transitions without saving them into the inherited map tail.

Next generator stages are:

- translate FO3 actor placements into semantic `NPC` runtime records
- resolve humanoid, ghoul, mutant, robot, creature, and faction roles to suitable classic critter prototypes
- translate FO3 placed references into furniture, machinery, rubble, signs, and other scenery families
- create more station-specific metro architecture and blocked/collapsed tunnel variants
- compile FO3 quest/dialogue/faction relationships separately from physical map generation
- replace inherited classic object/script tails with clean deliberately generated native MAP sections

## Folder layout

- `control_maps/` - topography, density, and buildability imagery
- `prefabs/` - procedural structure planning and later mined component libraries
- `runtime/` - F3O compiler and format documentation
- `subway/` - metro graph and underground MAP generation
- `sources/` - optional source-game import/indexing layers
- `dlc/` - source-weight and DLC generation profiles
- `xedit/` - Fallout 3 scanner/export notes and scripts

The original tile synthesizer remains under `tools/fo3_classic_forge/`; Fallout 3-specific orchestration and expansion modules live here, while engine runtime changes remain under `src/`.
