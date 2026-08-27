# Fallout 3 Classic Generator

This folder is the dedicated home for the Fallout 3 -> classic Fallout 1/2 generation pipeline used by `fallout2-rt-test`.

The goal is to rebuild Fallout 3 content as native classic Fallout maps and data rather than mixing Part 3 generation code into the normal engine source tree.

## Generator pipeline

1. **xEdit scan input**
   - WRLD / CELL / LAND / placed references
   - map markers
   - doors and destinations
   - NPC / creature bases
   - QUST / DIAL / INFO / scripts / factions
   - relationship graph

2. **Control imagery**
   - height/topography maps
   - slope maps
   - terrain gradients
   - build-density masks
   - road masks
   - no-build masks
   - landmark masks
   - ruin/damage masks

3. **Classic asset mining**
   - automatically scan Fallout 1 and Fallout 2 MAP files
   - extract reusable floor, roof, wall, doorway, fence, rubble, cave and scenery modules
   - tag compatible pieces and connectors

4. **Structure generation**
   - houses
   - shops
   - ruined buildings
   - warehouses
   - military compounds
   - vault entrances
   - metro entrances
   - bridges/checkpoints/fences
   - interiors and transition anchors

5. **Subway/metro generation**
   - station entrances
   - concourses
   - platforms
   - tunnel sections
   - maintenance/service branches
   - blocked/collapsed sections
   - underground graph linking stations to surface maps

6. **Native Fallout output**
   - generated `.MAP` files
   - MAPS.TXT fragments
   - world graph
   - object placement data
   - later quest/dialogue/script compilation

## Current implementation

The first generator implementation currently lives at:

`tools/fo3_classic_forge/`

As the generator grows, new Fallout 3-specific modules, specs, presets and generated-data definitions belong under this folder. Engine-runtime changes still belong under `src/`.

## Folder layout

- `control_maps/` - imagery/topography/gradient control system
- `prefabs/` - mined and procedural classic structure definitions
- `subway/` - metro/station/tunnel generation
- `xedit/` - scanner/export format notes and scripts

