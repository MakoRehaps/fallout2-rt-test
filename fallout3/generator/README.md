# Fallout 3 Classic Generator

This folder is the dedicated Fallout 3 -> classic Fallout 1/2 generation pipeline for `fallout2-rt-test`.

The Fallout 3 system is intentionally **not part of the main engine/mod runtime**. It is an isolated content-production pipeline: it consumes Fallout 3 xEdit scan data plus user-supplied Fallout 1/2 map archives and produces classic Fallout-compatible maps/data. The normal engine must remain usable without this folder, without FO3 source data, and without any per-frame Fallout 3 hook.

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

## Isolation contract

The generator follows these rules:

- no Fallout 3 loader in `src/`;
- no FPS-loop or other always-on engine hook;
- no requirement for FO3 JSON/xEdit files while playing;
- `.F3O` files are generator/compiler intermediates, not a permanent engine format;
- final placement data should be compiled into normal native classic Fallout map/object data;
- removing `fallout3/` and the generator tools must leave the normal engine behavior unchanged.

This makes Fallout 3 a generated content pack/total-conversion layer rather than a subsystem embedded into the base mod framework.

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

5. **F3O intermediate compilation**
   - wall runs
   - door positions
   - loot/container anchors
   - reserved NPC anchors
   - surface map-to-map transition requests
   - surface-to-metro transition requests

6. **Native object compilation**
   - generator-side prototype/style resolver selects suitable classic wall, door, container, scenery, critter, and exit-grid families
   - symbolic F3O placement requests are converted into deliberate native MAP object records
   - final generated maps should not need F3O interpretation at runtime

7. **Metro/subway generation**
   - detects FO3 metro/station locations
   - creates a concourse and platform map for each station
   - creates tunnel-segment maps between connected stations
   - writes generated `F3Uxxxx.MAP` underground maps
   - creates generator-side transition plans
   - connects underground maps back to their `F3Mxxxx` surface locations
   - emits a combined surface + subway MAPS.TXT fragment

8. **Validation**
   - generated MAP files must exist and contain a valid-sized header/body
   - generated map IDs/names must be unique
   - world graph links must reference known maps
   - intermediate EXIT requests must target known generated map IDs and valid hex/elevation/rotation
   - required control imagery and structure plans must exist
   - final native-object compilation will additionally validate object sections and prototype IDs

## Output layout

```text
FO3_GENERATED/
  AUTO_BUILD_COMPLETE.json
  active_profile.json
  maps/
    MAPS/
      F3M0000.MAP
      ...
      F3U0000.MAP
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

The `runtime/` output name currently refers to generated placement/intermediate data. It does **not** mean a required engine runtime module.

## Source and DLC policy

Fallout 1 and Fallout 2 are the primary classic construction vocabulary. Fallout Tactics is an optional DLC/source library and is not treated as binary-compatible with Fallout 2 maps. Tactics material is indexed with provenance and is intended to be translated into abstract layout/style information before being rebuilt using compatible classic engine assets.

## Current limitations / next stages

The generated surface and subway `.MAP` files still use a classic map as their binary container. Their tile layer is generated, but the inherited serialized object/script tail is not yet replaced by a clean deliberately generated object section.

The immediate architectural priority is therefore the **clean native object writer**. Once that exists, walls, doors, containers, exits, NPCs, and scenery can be compiled directly into the generated MAP and F3O can remain a build-time intermediate only.

Next generator stages are:

- write clean native MAP object/script sections instead of inheriting template tails;
- translate FO3 actor placements into semantic NPC records;
- resolve humanoid, ghoul, mutant, robot, creature, and faction roles to suitable classic critter prototypes;
- translate FO3 placed references into furniture, machinery, rubble, signs, and scenery families;
- create more station-specific metro architecture and blocked/collapsed tunnel variants;
- compile FO3 quest/dialogue/faction relationships separately from physical map generation.

## Folder layout

- `control_maps/` - topography, density, and buildability imagery
- `prefabs/` - procedural structure planning and later mined component libraries
- `runtime/` - generator-side F3O/intermediate compiler and format documentation
- `subway/` - metro graph and underground MAP generation
- `sources/` - optional source-game import/indexing layers
- `dlc/` - source-weight and DLC generation profiles
- `xedit/` - Fallout 3 scanner/export notes and scripts

The original tile synthesizer remains under `tools/fo3_classic_forge/`. Fallout 3-specific orchestration and generated-content code live under `fallout3/`; the core `src/` engine should remain Fallout 3-agnostic.
