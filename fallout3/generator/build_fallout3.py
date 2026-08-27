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
SUBWAY_MAPS = HERE / 'subway' / 'generate_subway_maps.py'
TACTICS = HERE / 'sources' / 'tactics' / 'index_tactics_source.py'
RUNTIME = HERE / 'runtime' / 'compile_runtime_layout.py'
VALIDATE = HERE / 'validate_build.py'


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

    out = a.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    profile = json.loads(a.profile.read_text(encoding='utf-8'))
    (out / 'active_profile.json').write_text(json.dumps(profile, indent=2), encoding='utf-8')

    maps = out / 'maps'
    forge_cmd = [FORGE, '--scan', a.scan, '--fo1', a.fo1, '--fo2', a.fo2, '--output', maps, '--seed', a.seed, '--start-map-id', a.start_map_id]
    if a.limit: forge_cmd += ['--limit', a.limit]
    run(forge_cmd)

    control = out / 'control_maps'
    run([CONTROL, '--scan', a.scan, '--output', control])

    manifest = maps / 'fo3_world_manifest.json'
    world_graph = maps / 'fallout_rt_world_graph.json'
    structures = out / 'structures'
    run([STRUCT, '--manifest', manifest, '--output', structures, '--seed', a.seed])

    subway = out / 'subway'; subway_graph = None; subway_manifest = None
    if profile.get('subway', {}).get('enabled', True):
        run([SUBWAY, '--manifest', manifest, '--output', subway])
        subway_graph = subway / 'subway_graph.json'
        run([
            SUBWAY_MAPS,
            '--graph', subway_graph,
            '--surface-manifest', manifest,
            '--fo1', a.fo1,
            '--fo2', a.fo2,
            '--maps-dir', maps / 'MAPS',
            '--output', subway,
            '--seed', a.seed,
        ])
        subway_manifest = subway / 'subway_manifest.json'

        surface_fragment = maps / 'maps_txt_fragment.txt'
        subway_fragment = subway / 'subway_maps_txt_fragment.txt'
        complete = ''
        if surface_fragment.exists(): complete += surface_fragment.read_text(encoding='utf-8')
        if subway_fragment.exists(): complete += subway_fragment.read_text(encoding='utf-8')
        (maps / 'maps_txt_fragment_complete.txt').write_text(complete, encoding='utf-8')

    runtime = out / 'runtime'
    runtime_cmd = [RUNTIME, '--structures', structures / 'structure_plans.json', '--world-graph', world_graph, '--output', runtime, '--maps-dir', maps / 'MAPS']
    if subway_graph: runtime_cmd += ['--subway', subway_graph]
    if subway_manifest: runtime_cmd += ['--subway-build', subway_manifest]
    run(runtime_cmd)

    tactics_manifest = None
    tactics_weight = float(profile.get('sources', {}).get('tactics', 0.0))
    if a.tactics and tactics_weight > 0.0:
        td = out / 'sources' / 'tactics'; td.mkdir(parents=True, exist_ok=True)
        tactics_manifest = td / 'tactics_source_manifest.json'
        run([TACTICS, a.tactics, '--output', tactics_manifest])

    summary = {
        'format': 'PhoBoi.Fallout3AutoBuild/6',
        'profile': profile.get('id', 'unknown'),
        'seed': a.seed,
        'inputs': {'scan': str(a.scan), 'fo1': str(a.fo1), 'fo2': str(a.fo2), 'tactics': str(a.tactics) if a.tactics else None},
        'outputs': {
            'maps': str(maps),
            'world_manifest': str(manifest),
            'world_graph': str(world_graph),
            'maps_txt_complete': str(maps / 'maps_txt_fragment_complete.txt') if subway_manifest else str(maps / 'maps_txt_fragment.txt'),
            'control_maps': str(control),
            'structure_plans': str(structures / 'structure_plans.json'),
            'subway_graph': str(subway_graph) if subway_graph else None,
            'subway_manifest': str(subway_manifest) if subway_manifest else None,
            'runtime_layouts': str(runtime / 'runtime_index.json'),
            'runtime_sidecars': str(maps / 'MAPS'),
            'tactics_manifest': str(tactics_manifest) if tactics_manifest else None,
        },
        'engine_runtime': 'Generated F3M surface and F3U underground maps load F3O sidecars. Runtime geometry resolves category-aware walls/doors/containers and creates exit-grid transitions for both the surface graph and metro network.',
        'engine_next_stage': 'Resolve FO3 actor/NPC anchors and placed scenery, add station-specific architecture, then replace inherited classic MAP object tails with clean generated native object sections.'
    }
    (out / 'AUTO_BUILD_COMPLETE.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    run([VALIDATE, '--root', out])
    print('\n[FO3 AUTO] COMPLETE:', out)
    return 0


if __name__ == '__main__': raise SystemExit(main())
