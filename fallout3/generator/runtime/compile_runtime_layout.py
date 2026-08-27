#!/usr/bin/env python3
"""Compile FO3 generator plans into build-time F3O placement files.

F3O is an isolated generator/compiler intermediate. It is never copied into the
playable MAPS directory and the core engine does not load it.
"""

from __future__ import annotations
import argparse, json
from collections import defaultdict
from pathlib import Path


def emit_structure(lines, s):
    f=s['footprint']; x=int(f['x']); y=int(f['y']); w=int(f['w']); h=int(f['h'])
    typ=s.get('type','structure'); ruin=float(s.get('ruin',0.0)); sid=s.get('id','structure')
    lines.append(f'STRUCTURE {sid} {typ} {x} {y} {w} {h} ruin={ruin:.3f}')
    lines.append(f'WALL_RUN {sid} north {x} {y} {w}')
    lines.append(f'WALL_RUN {sid} south {x} {y+h-1} {w}')
    lines.append(f'WALL_RUN {sid} west {x} {y} {h}')
    lines.append(f'WALL_RUN {sid} east {x+w-1} {y} {h}')
    d=s.get('door') or {}
    if d: lines.append(f"DOOR {sid} {int(d['x'])} {int(d['y'])} {d.get('side','north')}")
    cx=x+w//2; cy=y+h//2
    lines.append(f'ANCHOR {sid} center {cx} {cy}')
    lines.append(f'ANCHOR {sid} loot {max(x+1,cx-1)} {max(y+1,cy-1)}')
    lines.append(f'ANCHOR {sid} npc {min(x+w-2,cx+1)} {min(y+h-2,cy+1)}')


def exit_side(a,b):
    dx=int(b.get('x',0))-int(a.get('x',0)); dy=int(b.get('y',0))-int(a.get('y',0))
    if abs(dx)>=abs(dy): return 'east' if dx>=0 else 'west'
    return 'north' if dy>=0 else 'south'


def exit_position(side,ordinal=0):
    offset=max(-20,min(20,(ordinal//2+1)*6*(1 if ordinal%2==0 else -1))) if ordinal else 0
    if side=='east': return 98,max(10,min(90,50+offset)),1
    if side=='west': return 1,max(10,min(90,50+offset)),4
    if side=='north': return max(10,min(90,50+offset)),1,0
    return max(10,min(90,50+offset)),98,3


def build_exits(path):
    if not path or not path.exists(): return defaultdict(list)
    doc=json.loads(path.read_text(encoding='utf-8'))
    nodes={n['map_name']:n for n in doc.get('nodes',[]) if n.get('map_name')}
    outgoing=defaultdict(list); side_counts=defaultdict(lambda:defaultdict(int)); seen=set()
    for link in doc.get('links',[]):
        if not isinstance(link,list) or len(link)!=2: continue
        aa,bb=link
        if aa not in nodes or bb not in nodes or aa==bb: continue
        key=tuple(sorted((aa,bb)))
        if key in seen: continue
        seen.add(key)
        for src,dst in ((aa,bb),(bb,aa)):
            a=nodes[src]; b=nodes[dst]; side=exit_side(a,b)
            ordinal=side_counts[src][side]; side_counts[src][side]+=1
            x,y,rot=exit_position(side,ordinal)
            outgoing[src].append({'side':side,'x':x,'y':y,'rotation':rot,'target_map':int(b['map_id']),'target_name':dst,'target_tile':20100,'target_elevation':0,'target_rotation':(rot+3)%6})
    return outgoing


def add_subway_surface_exits(outgoing,path):
    if not path or not path.exists(): return 0
    doc=json.loads(path.read_text(encoding='utf-8')); added=0
    for e in doc.get('surface_exits',[]):
        name=e.get('surface_map')
        if not name: continue
        outgoing[name].append({'side':'metro','x':int(e.get('x',50)),'y':int(e.get('y',50)),'rotation':0,'target_map':int(e['target_map_id']),'target_name':e['target_map'],'target_tile':int(e.get('target_tile',20100)),'target_elevation':int(e.get('target_elevation',0)),'target_rotation':int(e.get('target_rotation',0))}); added+=1
    return added


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--structures',required=True,type=Path)
    ap.add_argument('--subway',type=Path)
    ap.add_argument('--subway-build',type=Path)
    ap.add_argument('--world-graph',type=Path)
    ap.add_argument('--output',required=True,type=Path)
    a=ap.parse_args(); a.output.mkdir(parents=True,exist_ok=True)

    exits=build_exits(a.world_graph); metro_surface_exits=add_subway_surface_exits(exits,a.subway_build)
    doc=json.loads(a.structures.read_text(encoding='utf-8'))
    index={'format':'PhoBoi.F3OIndex/5','scope':'build-time-only','maps':[]}
    for mp in doc.get('maps',[]):
        name=mp['map_name']; lines=['F3O 2',f'MAP {name}',f"CATEGORY {mp.get('category','wasteland')}"]
        for s in mp.get('structures',[]): emit_structure(lines,s)
        for e in exits.get(name,[]): lines.append(f"EXIT {e['side']} {e['x']} {e['y']} {e['target_map']} {e['target_tile']} {e['target_elevation']} {e['target_rotation']} target={e['target_name']}")
        out=a.output/(name+'.F3O'); out.write_text('\n'.join(lines)+'\n',encoding='utf-8')
        index['maps'].append({'map_name':name,'file':out.name,'commands':len(lines)-3,'exits':len(exits.get(name,[]))})

    if a.subway and a.subway.exists():
        subway=json.loads(a.subway.read_text(encoding='utf-8'))
        (a.output/'subway_runtime_jobs.json').write_text(json.dumps({'format':'PhoBoi.FO3SubwayRuntimeJobs/2','scope':'build-time-only','jobs':subway.get('generation_jobs',[]),'links':subway.get('links',[])},indent=2),encoding='utf-8')

    index['metro_surface_exits']=metro_surface_exits
    (a.output/'runtime_index.json').write_text(json.dumps(index,indent=2),encoding='utf-8')
    print(f"Build-time layouts: {len(index['maps'])} maps -> {a.output}")
    print(f"Planned surface exits: {sum(len(v) for v in exits.values())} (metro={metro_surface_exits})")

if __name__=='__main__': main()
