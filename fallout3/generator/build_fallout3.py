#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
FORGE = REPO / 'tools' / 'fo3_classic_forge' / 'fo3_classic_forge.py'
CONTROL = HERE / 'control_maps' / 'build_control_maps.py'
STRUCT = HERE / 'prefabs' / 'plan_structures.py'
SUBWAY = HERE / 'subway' / 'build_subway_graph.py'
TACTICS = HERE / 'sources' / 'tactics' / 'index_tactics_source.py'


def run(args):
    cmd = [sys.executable, *map(str, args)]
    print('[FO3 AUTO]', ' '.join(cmd), flush=True)
    subprocess.run(cmd, check=True)


def main():
    p = argparse.ArgumentParser(description='One-command Fallout 3 -> classic Fallout build pipeline')
    p.add_argument('--scan', required=True, type=Path)
    p.add_argument('--fo1', required=True, type=Path)
    p.add_argument('--fo2', required=True, type=Path)
    p.add_argument('--tactics', type=Path, help='Optional Fallout Tactics folder/ZIP for DLC source indexing')
    p.add_argument('--profile', type=Path, default=HERE / 'dlc' / 'base_fo3.json')
    p.add_argument('--output', type=Path, default=Path('FO3_GENERATED'))
    p.add_argument('--limit', type=int, default=0)
    p.add_argument('--seed', type=int, default=3003)
    p.add_argument('--start-map-id', type=int, default=200)
    a = p.parse_args()

    out = a.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    profile = json.loads(a.profile.read_text(encoding='utf-8'))
    (out / 'active_profile.json').write_text(json.dumps(profile, indent=2), encoding='utf-8')

    # 1. Generate native classic MAP prototypes from the FO3 xEdit scan.
    maps = out / 'maps'
    forge_cmd = [
        FORGE, '--scan', a.scan, '--fo1', a.fo1, '--fo2', a.fo2,
        '--output', maps, '--seed', a.seed, '--start-map-id', a.start_map_id,
    ]
    if a.limit:
        forge_cmd += ['--limit', a.limit]
    run(forge_cmd)

    # 2. Generate geographic imagery used by later placement passes.
    control = out / 'control_maps'
    run([CONTROL, '--scan', a.scan, '--output', control])

    # 3. Generate deterministic building/structure footprints for every map.
    manifest = maps / 'fo3_world_manifest.json'
    structures = out / 'structures'
    run([STRUCT, '--manifest', manifest, '--output', structures, '--seed', a.seed])

    # 4. Generate the underground metro graph when the active profile permits it.
    subway = out / 'subway'
    subway_graph = None
    if profile.get('subway', {}).get('enabled', True):
        run([SUBWAY, '--manifest', manifest, '--output', subway])
        subway_graph = subway / 'subway_graph.json'

    # 5. Optional Tactics DLC source inventory. Tactics is source material only;
    #    it is never assumed binary-compatible with Fallout 1/2 MAP files.
    tactics_manifest = None
    tactics_weight = float(profile.get('sources', {}).get('tactics', 0.0))
    if a.tactics and tactics_weight > 0.0:
        td = out / 'sources' / 'tactics'
        td.mkdir(parents=True, exist_ok=True)
        tactics_manifest = td / 'tactics_source_manifest.json'
        run([TACTICS, a.tactics, '--output', tactics_manifest])

    summary = {
        'format': 'PhoBoi.Fallout3AutoBuild/2',
        'profile': profile.get('id', 'unknown'),
        'seed': a.seed,
        'inputs': {
            'scan': str(a.scan),
            'fo1': str(a.fo1),
            'fo2': str(a.fo2),
            'tactics': str(a.tactics) if a.tactics else None,
        },
        'outputs': {
            'maps': str(maps),
            'world_manifest': str(manifest),
            'control_maps': str(control),
            'structure_plans': str(structures / 'structure_plans.json'),
            'subway_graph': str(subway_graph) if subway_graph else None,
            'tactics_manifest': str(tactics_manifest) if tactics_manifest else None,
        },
        'engine_next_stage': 'Native MAP object writer: walls, doors, scenery, blockers, exits, containers and NPC anchors.'
    }
    (out / 'AUTO_BUILD_COMPLETE.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    print('\n[FO3 AUTO] COMPLETE:', out)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
