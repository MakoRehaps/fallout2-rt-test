#!/usr/bin/env python3
"""Generate classic Fallout MAP files for the Fallout 3 metro graph.

The subway graph describes stations and tunnel links. This pass allocates real
classic map IDs/names, synthesizes underground tile layers from the user's FO1/
FO2 MAP archives, writes MAPS.TXT entries, and creates F3O exit sidecars for the
underground network. Surface->metro exits are emitted in subway_manifest.json so
the main runtime compiler can add them to the matching F3M sidecars.
"""

from __future__ import annotations
import argparse, importlib.util, json, random, sys
from pathlib import Path

HERE=Path(__file__).resolve()
REPO=HERE.parents[3]
FORGE_PATH=REPO/'tools'/'fo3_classic_forge'/'fo3_classic_forge.py'


def load_forge():
    spec=importlib.util.spec_from_file_location('fo3_classic_forge_runtime',FORGE_PATH)
    if spec is None or spec.loader is None: raise RuntimeError('cannot load fo3_classic_forge.py')
    mod=importlib.util.module_from_spec(spec); sys.modules[spec.name]=mod; spec.loader.exec_module(mod)
    return mod


def edge_exit(side,target_id,target_name):
    spots={
        'north':(50,2,3), 'south':(50,97,0),
        'west':(2,50,1), 'east':(97,50,4),
    }
    x,y,rot=spots[side]
    return f'EXIT {side} {x} {y} {target_id} 20100 0 {rot} target={target_name}'


def write_f3o(path,name,category,neighbors):
    lines=['F3O 2',f'MAP {name}',f'CATEGORY {category}']
    sides=['north','east','south','west']
    for i,n in enumerate(neighbors):
        lines.append(edge_exit(sides[i%len(sides)],n['map_id'],n['map_name']))
    path.write_text('\n'.join(lines)+'\n',encoding='utf-8')


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--graph',required=True,type=Path)
    ap.add_argument('--surface-manifest',required=True,type=Path)
    ap.add_argument('--fo1',required=True,type=Path)
    ap.add_argument('--fo2',required=True,type=Path)
    ap.add_argument('--maps-dir',required=True,type=Path)
    ap.add_argument('--output',required=True,type=Path)
    ap.add_argument('--seed',type=int,default=3003)
    a=ap.parse_args(); a.output.mkdir(parents=True,exist_ok=True); a.maps_dir.mkdir(parents=True,exist_ok=True)

    graph=json.loads(a.graph.read_text(encoding='utf-8'))
    surface=json.loads(a.surface_manifest.read_text(encoding='utf-8'))
    surface_entries=surface.get('generated_maps',[])
    if not surface_entries: raise SystemExit('surface manifest has no maps')
    surface_by_name={e['map_name']:e for e in surface_entries}
    next_id=max(int(e['map_id']) for e in surface_entries)+1

    forge=load_forge(); lib=forge.Library(); lib.add('FO1',a.fo1); lib.add('FO2',a.fo2)
    rng=random.Random(a.seed ^ 0xF03A11)

    maps=[]; name_counter=0
    station_nodes={}

    def make_map(kind,station=None,category='cave'):
        nonlocal next_id,name_counter
        name=f'F3U{name_counter:04d}'; name_counter+=1
        map_id=next_id; next_id+=1
        rough=6.5 if category=='cave' else 3.5
        template=lib.template(category,20)
        tiles,used=forge.synth(lib,category,rough,rng,20)
        (a.maps_dir/(name+'.MAP')).write_bytes(forge.patch(template,name,tiles))
        rec={'map_name':name,'map_id':map_id,'kind':kind,'station':station,'category':category,'classic_chunks':used,'template':template.base}
        maps.append(rec)
        return rec

    for s in graph.get('stations',[]):
        sid=s['id']
        concourse=make_map('station_concourse',sid,'industrial')
        platform=make_map('station_platform',sid,'industrial')
        station_nodes[sid]={'surface':sid,'concourse':concourse,'platform':platform,'location':s.get('location')}

    link_chains=[]
    for li,link in enumerate(graph.get('links',[])):
        a_id=link['a']; b_id=link['b']
        if a_id not in station_nodes or b_id not in station_nodes: continue
        count=max(1,int(link.get('segments',1)))
        segs=[make_map('tunnel_segment',f'{a_id}->{b_id}','cave') for _ in range(count)]
        chain=[station_nodes[a_id]['platform'],*segs,station_nodes[b_id]['platform']]
        link_chains.append({'a':a_id,'b':b_id,'maps':[m['map_name'] for m in chain]})

    neighbors={m['map_name']:[] for m in maps}
    surface_exits=[]
    def connect(a_rec,b_rec):
        neighbors[a_rec['map_name']].append({'map_name':b_rec['map_name'],'map_id':b_rec['map_id']})
        neighbors[b_rec['map_name']].append({'map_name':a_rec['map_name'],'map_id':a_rec['map_id']})

    for sid,n in station_nodes.items():
        connect(n['concourse'],n['platform'])
        surf=surface_by_name.get(sid)
        if surf:
            surface_exits.append({
                'surface_map':sid,
                'surface_map_id':int(surf['map_id']),
                'target_map':n['concourse']['map_name'],
                'target_map_id':n['concourse']['map_id'],
                'x':50,'y':50,'side':'metro','target_tile':20100,'target_elevation':0,'target_rotation':0,
            })
            neighbors[n['concourse']['map_name']].append({'map_name':sid,'map_id':int(surf['map_id'])})

    for chain_info in link_chains:
        chain=[]
        for name in chain_info['maps']:
            rec=next((m for m in maps if m['map_name']==name),None)
            if rec is not None: chain.append(rec)
        for x,y in zip(chain,chain[1:]): connect(x,y)

    for m in maps:
        write_f3o(a.maps_dir/(m['map_name']+'.F3O'),m['map_name'],m['category'],neighbors[m['map_name']][:4])

    maps_txt=[]
    for m in maps:
        maps_txt.append(f"[Map {m['map_id']:03d}]\nlookup_name=FO3 Metro: {m['kind']}\nmap_name={m['map_name']}\nmusic=07desert\nsaved=Yes\ndead_bodies_age=Yes\ncan_rest_here=Yes,Yes,Yes\npipboy_active=Yes\n\n")
    (a.output/'subway_maps_txt_fragment.txt').write_text(''.join(maps_txt),encoding='utf-8')

    out={
        'format':'PhoBoi.FO3SubwayBuild/1',
        'maps':maps,
        'stations':station_nodes,
        'link_chains':link_chains,
        'surface_exits':surface_exits,
        'first_map_id':min((m['map_id'] for m in maps),default=None),
        'next_map_id':next_id,
    }
    (a.output/'subway_manifest.json').write_text(json.dumps(out,indent=2),encoding='utf-8')
    print(f"Subway maps: {len(maps)}; stations={len(station_nodes)}; surface exits={len(surface_exits)}")

if __name__=='__main__': main()
