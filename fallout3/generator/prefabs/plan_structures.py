#!/usr/bin/env python3
"""Create deterministic building/structure plans for generated FO3 classic maps.

This stage does not yet serialize Fallout object records. It creates the exact
footprints/connectors/roles that the MAP object writer will consume.
"""

from __future__ import annotations
import argparse, json, random
from pathlib import Path

TYPES = {
    'urban': ['ruined_house','shop','apartment_shell','clinic','diner','row_building'],
    'industrial': ['warehouse','workshop','power_room','fenced_yard','checkpoint','factory_shell'],
    'vault': ['vault_entry','security_room','service_block'],
    'cave': ['cave_chamber','service_tunnel','collapsed_room'],
    'wasteland': ['shack','camp','ruin','roadblock'],
}


def clamp(v,a,b): return max(a,min(b,v))


def make_plan(entry, seed):
    rng = random.Random(seed + int(entry['index']) * 7919)
    cat = entry.get('category','wasteland')
    rough = float(entry.get('roughness',0))
    if cat in ('urban','industrial'):
        count = rng.randint(6,14)
    elif cat == 'vault':
        count = rng.randint(3,7)
    else:
        count = rng.randint(1,6)

    structures=[]
    occupied=[]
    for i in range(count):
        for _ in range(100):
            w=rng.randint(6,18); h=rng.randint(5,15)
            x=rng.randint(6,94-w); y=rng.randint(6,94-h)
            if rough > 7 and rng.random() < .35:
                continue
            box=(x,y,x+w,y+h)
            if any(not (box[2]+2<o[0] or o[2]+2<box[0] or box[3]+2<o[1] or o[3]+2<box[1]) for o in occupied):
                continue
            occupied.append(box)
            side=rng.choice(['north','south','east','west'])
            door = {
                'north':[x+w//2,y], 'south':[x+w//2,y+h-1],
                'west':[x,y+h//2], 'east':[x+w-1,y+h//2]
            }[side]
            structures.append({
                'id':f"{entry['map_name']}_S{i:02d}",
                'type':rng.choice(TYPES.get(cat,TYPES['wasteland'])),
                'footprint':{'x':x,'y':y,'w':w,'h':h},
                'door':{'x':door[0],'y':door[1],'side':side},
                'ruin':round(clamp(.35 + rough/20 + rng.uniform(-.15,.2),0,1),3),
                'connectors':['doorway','floor','wall','roof'],
                'source_request':{
                    'themes':[cat,'ruined' if rough>4 else 'intact'],
                    'allow_mixed_sources':True
                }
            })
            break
    return {'map_name':entry['map_name'],'location':entry.get('marker_name'),'category':cat,'structures':structures}


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--manifest',required=True,type=Path)
    ap.add_argument('--output',required=True,type=Path)
    ap.add_argument('--seed',type=int,default=3003)
    a=ap.parse_args(); a.output.mkdir(parents=True,exist_ok=True)
    doc=json.loads(a.manifest.read_text(encoding='utf-8'))
    plans=[make_plan(e,a.seed) for e in doc.get('generated_maps',[])]
    (a.output/'structure_plans.json').write_text(json.dumps({'format':'PhoBoi.FO3StructurePlans/1','seed':a.seed,'maps':plans},indent=2),encoding='utf-8')
    print(f'Structure plans: {len(plans)} maps -> {a.output}')

if __name__=='__main__': main()
