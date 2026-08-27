#!/usr/bin/env python3
"""Compile FO3 generator plans into deterministic per-map runtime placement files.

The .F3O format is an engine-facing intermediate representation. It deliberately
keeps symbolic asset roles (wall, door, clutter, exit) separate from concrete
Fallout PIDs/FIDs. A later resolver/runtime hook can map those roles to mined
FO1/FO2/Tactics-compatible assets without changing the high-level plans.
"""

from __future__ import annotations
import argparse, json
from pathlib import Path


def emit_structure(lines, s):
    f=s['footprint']; x=int(f['x']); y=int(f['y']); w=int(f['w']); h=int(f['h'])
    typ=s.get('type','structure'); ruin=float(s.get('ruin',0.0))
    sid=s.get('id','structure')
    lines.append(f'STRUCTURE {sid} {typ} {x} {y} {w} {h} ruin={ruin:.3f}')
    # Four wall runs. Door cutout is described separately so the resolver can
    # select compatible wall/door pieces from the mined classic asset library.
    lines.append(f'WALL_RUN {sid} north {x} {y} {w}')
    lines.append(f'WALL_RUN {sid} south {x} {y+h-1} {w}')
    lines.append(f'WALL_RUN {sid} west {x} {y} {h}')
    lines.append(f'WALL_RUN {sid} east {x+w-1} {y} {h}')
    d=s.get('door') or {}
    if d:
        lines.append(f"DOOR {sid} {int(d['x'])} {int(d['y'])} {d.get('side','north')}")
    # Interior anchors are deterministic and can become containers/NPC/quest refs.
    cx=x+w//2; cy=y+h//2
    lines.append(f'ANCHOR {sid} center {cx} {cy}')
    lines.append(f'ANCHOR {sid} loot {max(x+1,cx-1)} {max(y+1,cy-1)}')
    lines.append(f'ANCHOR {sid} npc {min(x+w-2,cx+1)} {min(y+h-2,cy+1)}')


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--structures', required=True, type=Path)
    ap.add_argument('--subway', type=Path)
    ap.add_argument('--output', required=True, type=Path)
    a=ap.parse_args(); a.output.mkdir(parents=True,exist_ok=True)

    doc=json.loads(a.structures.read_text(encoding='utf-8'))
    index={'format':'PhoBoi.F3OIndex/1','maps':[]}
    for mp in doc.get('maps',[]):
        name=mp['map_name']; lines=['F3O 1', f'MAP {name}', f"CATEGORY {mp.get('category','wasteland')}"]
        for s in mp.get('structures',[]): emit_structure(lines,s)
        out=a.output/(name+'.F3O')
        out.write_text('\n'.join(lines)+'\n',encoding='utf-8')
        index['maps'].append({'map_name':name,'file':out.name,'commands':len(lines)-3})

    if a.subway and a.subway.exists():
        subway=json.loads(a.subway.read_text(encoding='utf-8'))
        (a.output/'subway_runtime_jobs.json').write_text(json.dumps({
            'format':'PhoBoi.FO3SubwayRuntimeJobs/1',
            'jobs':subway.get('generation_jobs',[]),
            'links':subway.get('links',[])
        },indent=2),encoding='utf-8')

    (a.output/'runtime_index.json').write_text(json.dumps(index,indent=2),encoding='utf-8')
    print(f"Runtime layouts: {len(index['maps'])} maps -> {a.output}")

if __name__=='__main__': main()
