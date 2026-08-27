#!/usr/bin/env python3
from __future__ import annotations
import argparse, json
from pathlib import Path


def fail(msg):
    raise SystemExit('[FO3 VALIDATE] ERROR: '+msg)


def check_exit_line(line,f3o,valid_ids):
    parts=line.split()
    if len(parts)<8: fail(f'malformed EXIT in {f3o.name}: {line}')
    try:
        target_map=int(parts[4]); target_tile=int(parts[5]); elev=int(parts[6]); rot=int(parts[7])
    except ValueError:
        fail(f'non-numeric EXIT fields in {f3o.name}: {line}')
    if target_map not in valid_ids: fail(f'EXIT in {f3o.name} targets unknown map id {target_map}')
    if target_tile < 0 or target_tile >= 40000: fail(f'EXIT in {f3o.name} has invalid hex {target_tile}')
    if elev < 0 or elev > 2: fail(f'EXIT in {f3o.name} has invalid elevation {elev}')
    if rot < 0 or rot > 5: fail(f'EXIT in {f3o.name} has invalid rotation {rot}')


def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--root',required=True,type=Path)
    a=ap.parse_args(); root=a.root
    summary=root/'AUTO_BUILD_COMPLETE.json'
    if not summary.exists(): fail('AUTO_BUILD_COMPLETE.json missing')
    doc=json.loads(summary.read_text(encoding='utf-8')); outs=doc.get('outputs',{})
    if doc.get('architecture')!='isolated-content-generator': fail('build is not marked isolated-content-generator')

    manifest=Path(outs.get('world_manifest',''))
    if not manifest.exists(): fail('world manifest missing')
    world=json.loads(manifest.read_text(encoding='utf-8')); generated=world.get('generated_maps',[])
    if not generated: fail('no generated maps')

    surface_names={e['map_name'] for e in generated}; surface_ids=[int(e['map_id']) for e in generated]
    if len(surface_names)!=len(generated): fail('duplicate generated surface map names')
    if len(set(surface_ids))!=len(surface_ids): fail('duplicate generated surface map ids')

    subway_maps=[]
    subway_manifest_value=outs.get('subway_manifest')
    if subway_manifest_value:
        subway_manifest=Path(subway_manifest_value)
        if not subway_manifest.exists(): fail('subway manifest missing')
        subway_doc=json.loads(subway_manifest.read_text(encoding='utf-8')); subway_maps=subway_doc.get('maps',[])

    all_entries=[*generated,*subway_maps]
    all_names=[e['map_name'] for e in all_entries]; all_ids=[int(e['map_id']) for e in all_entries]
    if len(set(all_names))!=len(all_names): fail('duplicate map name across surface/subway sets')
    if len(set(all_ids))!=len(all_ids): fail('duplicate map id across surface/subway sets')
    valid_ids=set(all_ids)

    maps_dir=Path(outs.get('maps',''))/'MAPS'
    for e in all_entries:
        mp=maps_dir/(e['map_name']+'.MAP')
        if not mp.exists() or mp.stat().st_size<236: fail(f'missing/invalid MAP: {mp}')
        leaked=maps_dir/(e['map_name']+'.F3O')
        if leaked.exists(): fail(f'generator intermediate leaked into playable MAPS: {leaked}')

    f3o_dirs=[]
    for key in ('f3o_surface_workspace','f3o_subway_workspace'):
        val=outs.get(key)
        if val: f3o_dirs.append(Path(val))
    if not f3o_dirs: fail('F3O build workspace missing')
    total_exit_lines=0
    for folder in f3o_dirs:
        if not folder.exists(): fail(f'F3O workspace missing: {folder}')
        for f3o in folder.glob('*.F3O'):
            for line in f3o.read_text(encoding='utf-8',errors='replace').splitlines():
                if line.startswith('EXIT '): check_exit_line(line,f3o,valid_ids); total_exit_lines+=1

    actor_plans=Path(outs.get('actor_plans',''))
    if not actor_plans.exists(): fail('actor plans missing')
    actor_doc=json.loads(actor_plans.read_text(encoding='utf-8'))
    if actor_doc.get('scope')!='build-time-only': fail('actor plans are not marked build-time-only')
    for actor in actor_doc.get('placements',[]):
        if actor.get('target_map') not in surface_names: fail(f"actor targets unknown surface map {actor.get('target_map')}")
        if int(actor.get('target_map_id',-1)) not in set(surface_ids): fail(f"actor targets unknown surface map id {actor.get('target_map_id')}")

    control=Path(outs.get('control_maps',''))
    for n in ('terrain_roughness.pgm','settlement_density.pgm','buildability.pgm','control_map_meta.json'):
        if not (control/n).exists(): fail('control map output missing: '+n)
    structures=Path(outs.get('structure_plans',''))
    if not structures.exists(): fail('structure plans missing')
    maps_txt=outs.get('maps_txt_complete')
    if maps_txt and not Path(maps_txt).exists(): fail('complete MAPS.TXT fragment missing')

    print(f"[FO3 VALIDATE] OK: isolated build, surface={len(generated)}, subway={len(subway_maps)}, actors={len(actor_doc.get('placements',[]))}, planned exits={total_exit_lines}, no F3O in playable MAPS")
    return 0

if __name__=='__main__': raise SystemExit(main())
