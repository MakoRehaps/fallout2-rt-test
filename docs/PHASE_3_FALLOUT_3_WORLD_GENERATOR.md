# Phase 3 Design: Fallout 3-Style World Generator

Status: design only. This phase does not authorize implementation yet.

## Goal

Build a content-generation tool that can produce a 2.5D, Fallout 3-style campaign inside the unified Fallout 2 CE engine. The tool should reuse the art, sounds, critter models, tiles, scenery, items, and interface resources already available in an installed Fallout 1/Fallout 2 data set. Phase 3 may add new maps, dialogue, scripts, encounter definitions, and generated metadata, but it should not require new visual or audio assets.

This is a generator/compiler project rather than a plan to construct every map by hand.

## Core rule: no new assets

The generator may:

- Recombine existing floor, wall, roof, scenery, critter, item, sound, and interface assets.
- Generate new MAP layouts.
- Generate new SSL/INT scripts and world-state data.
- Generate new dialogue trees and message files.
- Assign existing animation and sound sets to new roles.
- Use palette, lighting, fog, weather, elevation, and object placement to create a different atmosphere.

The generator must not assume that a particular optional mod asset exists. Every generated object is selected from a scanned asset catalog. Missing assets cause a documented substitution or the feature is skipped safely.

## Starting point: the map-to-create manifest

Every generated location starts as a small declarative manifest. The manifest describes intent; it does not contain hand-authored hex coordinates for every object.

Example:

```yaml
id: cw_springvale_01
name: Springvale Outskirts
region: capital_wasteland
size: large
elevations: 1
biome: ruined_suburb
connections:
  north: cw_school_01
  east: cw_megaton_approach
  south: wilderness
  west: wilderness
landmarks:
  - ruined_houses
  - road_crossing
  - water_tower_substitute
encounters:
  - raider_patrol
  - scavenger
  - vehicle_wreck
quests:
  - springvale_cache
density:
  blocking_scenery: low
  decoration: medium
  loot: sparse
seed: 1010301
```

A first tool command would be conceptually:

```text
f3worldgen build maps/cw_springvale_01.yml
```

The output is a generated map package plus a validation report.

## Tool architecture

The proposed tool has six stages.

1. Asset catalog

   Scan the active Fallout data profile and build a searchable catalog of existing tiles, walls, scenery, critters, items, sounds, and scripts. Tag candidates by observable properties such as material, blocking behavior, dimensions, subtype, animation support, and words in prototype names.

2. Semantic planning

   Convert the map manifest into zones: entrances, roads, combat spaces, interiors, landmarks, safe spawn areas, quest spaces, and four-direction wilderness exits. Resolve every requested landmark to a compatible existing-asset recipe.

3. Layout generation

   Generate connected floor regions, road graphs, walls, doors, cover, scenery clusters, encounter anchors, and navigation corridors. Layout is deterministic from the manifest seed. Rebuilding the same manifest produces the same base map.

4. Content generation

   Place critters, loot, containers, vehicle wrecks, quest anchors, ambient sound emitters, and dialogue actors. Generate scripts from audited templates instead of producing unrestricted script text.

5. Compilation

   Emit engine-ready MAP data, scripts, message/dialogue files, world registry entries, and a generation manifest that records every selected prototype and substitution.

6. Validation

   Load the generated package in a headless or diagnostic engine mode and verify entrances, exits, connectivity, PIDs, scripts, local variables, encounter placement, save/load behavior, combat initialization, and camera bounds.

## 2.5D representation

Fallout 3 locations are translated into Fallout 2 concepts instead of copied literally.

- Open 3D terrain becomes one or more large isometric exterior maps.
- Roads become connected navigation spines with four-way wilderness grids.
- Multi-storey buildings become elevations or linked interior maps.
- Distant skyline elements become non-blocking scenery clusters and lighting silhouettes.
- Physics interactions become script-driven state changes.
- Real-time set pieces become staged map scripts and timed encounter waves.
- First-person discovery becomes fog-of-war, map discovery flags, and scripted landmark messages.

The generator optimizes for the feeling and quest topology of a Capital Wasteland journey while remaining native to the Fallout 2 engine.

## World graph

The campaign is a graph of authored hubs and generated wilderness cells.

- Hubs contain quests, factions, services, and authored dialogue.
- Connector maps represent roads, metro approaches, ruins, and tunnels.
- Wilderness cells use the open-map generator and four exits already introduced by the unified road system.
- Temporary dungeons can be seeded into cells and expire or regenerate through the existing world-state registry.
- Vehicles unlock world-map fast travel; walking continues through physical road exits.

The graph is compiled before maps so every exit has a valid reciprocal destination. The validator rejects dangling destinations and unreachable critical locations.

## Dialogue generation

Dialogue is new content stored as source data and compiled into DLG/message/script outputs.

Each conversation is represented as a graph with:

- Speaker and faction.
- Conditions.
- Player choices.
- Skill, SPECIAL, perk, reputation, item, and quest checks.
- Consequences.
- Failure and fallback nodes.
- Companion/co-op reactions.
- Localization keys.

The compiler verifies that every node is reachable, every response has text, every referenced global exists, and every terminal node exits cleanly. Generated dialogue should be reviewed as content; it is never silently regenerated over edited approved text.

## Script generation

Scripts come from a small library of templates:

- Map enter/exit.
- Door and transition.
- Quest state.
- Dialogue actor.
- Encounter controller.
- Loot/container.
- Faction combat.
- Vehicle wreck and salvage.
- Timed event.
- Temporary dungeon.
- Ambient scene.

Templates expose typed parameters. The compiler allocates globals and local variables, resolves object IDs, and emits deterministic script source. This is safer than asking a language model to invent unrestricted engine code for every object.

## Reusing assets intelligently

The catalog uses substitution groups rather than exact art requirements.

Examples:

- Ruined suburban building: existing ruined wall set + broken furniture + dirt/concrete floors.
- Metro tunnel: cave or industrial wall set + low light + metal debris + narrow graph.
- Wasteland overpass: road tiles + wall/scenery barriers + elevation illusion.
- Super-Duper-style market: warehouse/interior set + shelves + containers.
- Vehicle encounter: any cataloged car, truck, buggy, bike, Highwayman, or Vertibird scenery prototype.

Every substitution is recorded in the report so a designer can lock a choice or override it in the manifest.

## Generated wilderness and vehicles

The Phase 3 generator builds on the current runtime wilderness work:

- Open mountain maps with unsafe corridor blockers removed.
- Four directional exits.
- Deterministic per-cell seeds.
- Safe authored entrances.
- Encounter regeneration.
- Repairable vehicle wrecks.
- Science salvage for vehicle parts.
- Vehicle-only fast travel.

For production content, runtime generation should be complemented by an offline compiler that can preview and validate layouts before shipping them.

## Safety requirements

A generated map is rejected unless it passes all mandatory checks:

- Player entrance is clear and has multiple walkable neighbors.
- Every required exit exists and points to a registered destination.
- Every critical actor and quest object uses a valid prototype.
- No critical object spawns inside blocking scenery.
- Required locations are connected by pathfinding.
- Camera scroll restrictions do not create a corridor trap.
- Random encounter maps are never persisted as stale SAV maps.
- Combat target pointers are initialized safely.
- Scripts compile and reference valid procedures, messages, globals, and locals.
- Save, load, autosave, and map transfer smoke tests succeed.
- The map can load with optional vehicle prototypes absent.

## First vertical slice

The recommended slice is deliberately small:

1. Vault-style starting interior.
2. Exterior vault entrance.
3. One ruined-suburb connector.
4. One settlement hub.
5. One metro or cave connector.
6. Two generated wilderness cells.
7. One vehicle-wreck encounter.
8. One short quest with dialogue, combat, and two outcomes.

This proves the complete pipeline: manifest, generation, dialogue, scripts, compilation, validation, play, save/load, co-op, and controller/mobile input.

## Phase gates

Phase 3A — Catalog and manifest

- Asset scanner.
- Stable tags and substitution groups.
- Manifest schema.
- Human-readable validation errors.

Phase 3B — Map compiler

- Zone planner.
- Layout generator.
- Four-way connections.
- MAP emission.
- Navigation and spawn validation.

Phase 3C — Content compiler

- Dialogue graph compiler.
- Script templates.
- Quest/global registry.
- Encounter and loot placement.

Phase 3D — Vertical slice

- Build the eight-part slice.
- Test solo, local co-op, phone controllers, autosave, combat, and map changes.
- Lock deterministic seeds for shipped maps.

Phase 3E — World production

- Expand the world graph in reviewed batches.
- Generate preview reports.
- Hand-tune only manifests, approved dialogue, and explicit overrides.
- Never hand-edit generated binaries without updating their source manifest.

## Deliverables when implementation is approved

- `tools/f3worldgen/` generator source.
- Versioned manifest schema.
- Asset catalog format and scanner.
- Script/dialogue template library.
- Generated-map validator.
- Preview and diagnostic reports.
- Vertical-slice source manifests.
- Reproducible build command.
- Documentation for adding a location without editing engine C++.

## Decision for later

Before implementation begins, choose whether Phase 3 targets:

- A new, original Capital-Wasteland-inspired campaign using new writing.
- A compatibility framework into which the user supplies their own legally obtained content definitions.
- A direct location-by-location reinterpretation.

The generator architecture supports all three, but writing scope, naming, quest design, and distribution rules differ.
