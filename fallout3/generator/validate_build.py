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
    ap=argparse.ArgumentParser(); ap.add_argument('--root', required=True, type=Path)
    a=ap.parse_args(); root=a.root

    summary=root/'AUTO_BUILD_COMPLETE.json'
    if not summary.exists(): fail('AUTO_BUILD_COMPLETE.json missing')
    doc=json.loads(summary.read_text(encoding='utf-8')); outs=doc.get('outputs',{})

    manifest=Path(outs.get('world_manifest',''))
    if not manifest.exists(): fail('world manifest missing')
    world=json.loads(manifest.read_text(encoding='utf-8')); generated=world.get('generated_maps',[])
    if not generated: fail('no generated maps')

    surface_names={e['map_name'] for e in generated}; surface_ids=[int(e['map_id']) for e in generated]
    if len(surface_names)!=len(generated): fail('duplicate generated surface map names')
    if len(set(surface_ids))!=len(surface_ids): fail('duplicate generated surface map ids')

    subway_maps=[]; surface_metro_exits=0
    subway_manifest_value=outs.get('subway_manifest')
    if subway_manifest_value:
        subway_manifest=Path(subway_manifest_value)
        if not subway_manifest.exists(): fail('subway manifest missing')
        subway_doc=json.loads(subway_manifest.read_text(encoding='utf-8'))
        subway_maps=subway_doc.get('maps',[])
        surface_metro_exits=len(subway_doc.get('surface_exits',[]))

    all_entries=[*generated,*subway_maps]
    all_names=[e['map_name'] for e in all_entries]; all_ids=[int(e['map_id']) for e in all_entries]
    if len(set(all_names))!=len(all_names): fail('duplicate map name across surface/subway sets')
    if len(set(all_ids))!=len(all_ids): fail('duplicate map id across surface/subway sets')
    valid_ids=set(all_ids)

    maps_dir=Path(outs.get('maps',''))/'MAPS'
    missing_maps=[]; missing_sidecars=[]
    for e in all_entries:
        mp=maps_dir/(e['map_name']+'.MAP')
        if not mp.exists() or mp.stat().st_size < 236: missing_maps.append(str(mp))
        f3o=maps_dir/(e['map_name']+'.F3O')
        if not f3o.exists() or f3o.stat().st_size == 0: missing_sidecars.append(str(f3o))
    if missing_maps: fail(f'{len(missing_maps)} generated MAP files missing/invalid')
    if missing_sidecars: fail(f'{len(missing_sidecars)} generated F3O sidecars missing/invalid')

    runtime_index=Path(outs.get('runtime_layouts',''))
    if not runtime_index.exists(): fail('runtime layout index missing')
    rt=json.loads(runtime_index.read_text(encoding='utf-8')); rt_maps=rt.get('maps',[])
    if len(rt_maps) != len(generated): fail(f"surface runtime map count {len(rt_maps)} != generated surface map count {len(generated)}")

    graph_path=Path(outs.get('world_graph','')); world_directed=0
    if graph_path.exists():
        graph=json.loads(graph_path.read_text(encoding='utf-8'))
        graph_nodes={n.get('map_name') for n in graph.get('nodes',[]) if n.get('map_name')}
        if not surface_names.issubset(graph_nodes): fail('world graph is missing generated surface map nodes')
        valid_links=set()
        for link in graph.get('links',[]):
            if not isinstance(link,list) or len(link)!=2: fail('malformed world graph link')
            an,bn=link
            if an not in surface_names or bn not in surface_names: fail(f'world graph link targets unknown surface map: {link}')
            if an==bn: fail(f'world graph self-link: {an}')
            valid_links.add(tuple(sorted((an,bn))))
        world_directed=len(valid_links)*2

    emitted_surface=sum(int(m.get('exits',0)) for m in rt_maps)
    expected_surface=world_directed+surface_metro_exits
    if emitted_surface != expected_surface:
        fail(f'surface runtime exits {emitted_surface} != expected {expected_surface} (world={world_directed}, metro={surface_metro_exits})')

    total_exit_lines=0
    for e in all_entries:
        f3o=maps_dir/(e['map_name']+'.F3O')
        for line in f3o.read_text(encoding='utf-8',errors='replace').splitlines():
            if line.startswith('EXIT '):
                check_exit_line(line,f3o,valid_ids); total_exit_lines+=1

    control=Path(outs.get('control_maps',''))
    for n in ('terrain_roughness.pgm','settlement_density.pgm','buildability.pgm','control_map_meta.json'):
        if not (control/n).exists(): fail('control map output missing: '+n)

    structures=Path(outs.get('structure_plans',''))
    if not structures.exists(): fail('structure plans missing')

    maps_txt=outs.get('maps_txt_complete')
    if maps_txt and not Path(maps_txt).exists(): fail('complete MAPS.TXT fragment missing')

    print(f'[FO3 VALIDATE] OK: surface={len(generated)}, subway={len(subway_maps)}, exits={total_exit_lines}, sidecars/runtime/control imagery present')
    return 0

if __name__=='__main__': raise SystemExit(main())
