#!/usr/bin/env python3
from __future__ import annotations
import argparse, json, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GEN = Path(__file__).resolve().parent

def run(cmd):
    print('[FO3 AUTO]', ' '.join(str(x) for x in cmd), flush=True)
    subprocess.check_call([str(x) for x in cmd])

def main():
    p=argparse.ArgumentParser(description='One-command Fallout 3 -> classic Fallout build pipeline')
    p.add_argument('--scan', required=True, type=Path)
    p.add_argument('--fo1', required=True, type=Path)
    p.add_argument('--fo2', required=True, type=Path)
    p.add_argument('--output', type=Path, default=Path('FO3_GENERATED'))
    p.add_argument('--limit', type=int, default=0)
    p.add_argument('--seed', type=int, default=3003)
    p.add_argument('--start-map-id', type=int, default=200)
    a=p.parse_args()
    out=a.output.resolve(); out.mkdir(parents=True, exist_ok=True)

    # 1. Control imagery from FO3 geography/refs.
    run([sys.executable, GEN/'control_maps'/'generate_control_maps.py', '--scan', a.scan, '--output', out/'control_maps'])

    # 2. Mine reusable visual construction chunks from Fallout 1/2 maps.
    run([sys.executable, GEN/'prefabs'/'mine_prefabs.py', '--fo1', a.fo1, '--fo2', a.fo2, '--output', out/'prefabs'])

    # 3. Generate base classic maps using the existing forge.
    forge=ROOT/'tools'/'fo3_classic_forge'/'fo3_classic_forge.py'
    cmd=[sys.executable, forge, '--scan', a.scan, '--fo1', a.fo1, '--fo2', a.fo2, '--output', out/'maps', '--seed', str(a.seed), '--start-map-id', str(a.start_map_id)]
    if a.limit: cmd += ['--limit', str(a.limit)]
    run(cmd)

    # 4. Generate procedural structure/building plans for each generated map.
    run([sys.executable, GEN/'structures'/'generate_structures.py', '--manifest', out/'maps'/'fo3_world_manifest.json', '--control-root', out/'control_maps', '--prefabs', out/'prefabs'/'prefab_catalog.json', '--output', out/'structures', '--seed', str(a.seed)])

    # 5. Build underground metro/subway graph from FO3 location data.
    run([sys.executable, GEN/'subway'/'generate_subway.py', '--scan', a.scan, '--output', out/'subway'])

    summary={
      'format':'PhoBoi.Fallout3AutoBuild/1',
      'scan':str(a.scan), 'fo1':str(a.fo1), 'fo2':str(a.fo2),
      'maps':str(out/'maps'), 'control_maps':str(out/'control_maps'),
      'prefabs':str(out/'prefabs'), 'structures':str(out/'structures'),
      'subway':str(out/'subway')
    }
    (out/'AUTO_BUILD_COMPLETE.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
    print('\n[FO3 AUTO] COMPLETE:', out)
    return 0

if __name__=='__main__': raise SystemExit(main())
