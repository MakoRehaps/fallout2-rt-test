#!/usr/bin/env python3
from __future__ import annotations
import argparse, json
from pathlib import Path


def fail(msg):
    raise SystemExit('[FO3 VALIDATE] ERROR: '+msg)


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--root', required=True, type=Path)
    a=ap.parse_args(); root=a.root

    summary=root/'AUTO_BUILD_COMPLETE.json'
    if not summary.exists(): fail('AUTO_BUILD_COMPLETE.json missing')
    doc=json.loads(summary.read_text(encoding='utf-8'))
    outs=doc.get('outputs',{})

    manifest=Path(outs.get('world_manifest',''))
    if not manifest.exists(): fail('world manifest missing')
    world=json.loads(manifest.read_text(encoding='utf-8'))
    generated=world.get('generated_maps',[])
    if not generated: fail('no generated maps')

    maps_dir=Path(outs.get('maps',''))/'MAPS'
    missing=[]
    for e in generated:
        p=maps_dir/(e['map_name']+'.MAP')
        if not p.exists() or p.stat().st_size < 236:
            missing.append(str(p))
    if missing: fail(f'{len(missing)} generated MAP files missing/invalid')

    runtime_index=Path(outs.get('runtime_layouts',''))
    if not runtime_index.exists(): fail('runtime layout index missing')
    rt=json.loads(runtime_index.read_text(encoding='utf-8'))
    if len(rt.get('maps',[])) != len(generated):
        fail(f"runtime map count {len(rt.get('maps',[]))} != generated map count {len(generated)}")

    control=Path(outs.get('control_maps',''))
    for n in ('terrain_roughness.pgm','settlement_density.pgm','buildability.pgm','control_map_meta.json'):
        if not (control/n).exists(): fail('control map output missing: '+n)

    structures=Path(outs.get('structure_plans',''))
    if not structures.exists(): fail('structure plans missing')

    print(f'[FO3 VALIDATE] OK: {len(generated)} maps, runtime layouts matched, control imagery present')
    return 0

if __name__=='__main__': raise SystemExit(main())
