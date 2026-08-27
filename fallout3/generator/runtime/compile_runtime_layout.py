#!/usr/bin/env python3
"""Compile FO3 generator plans into per-map runtime placement files.

.F3O is consumed directly by fallout2-rt-test through src/fo3_runtime_layout.h.
The compiler can also mirror each sidecar into the generated MAPS directory so
copying the generated game data is enough to activate the structures.
"""

from __future__ import annotations
import argparse, json, shutil
from pathlib import Path


def emit_structure(lines, s):
    f=s['footprint']; x=int(f['x']); y=int(f['y']); w=int(f['w']); h=int(f['h'])
    typ=s.get('type','structure'); ruin=float(s.get('ruin',0.0))
    sid=s.get('id','structure')
    lines.append(f'STRUCTURE {sid} {typ} {x} {y} {w} {h} ruin={ruin:.3f}')
    lines.append(f'WALL_RUN {sid} north {x} {y} {w}')
    lines.append(f'WALL_RUN {sid} south {x} {y+h-1} {w}')
    lines.append(f'WALL_RUN {sid} west {x} {y} {h}')
    lines.append(f'WALL_RUN {sid} east {x+w-1} {y} {h}')
    d=s.get('door') or {}
    if d:
        lines.append(f"DOOR {sid} {int(d['x'])} {int(d['y'])} {d.get('side','north')}")
    cx=x+w//2; cy=y+h//2
    lines.append(f'ANCHOR {sid} center {cx} {cy}')
    lines.append(f'ANCHOR {sid} loot {max(x+1,cx-1)} {max(y+1,cy-1)}')
    lines.append(f'ANCHOR {sid} npc {min(x+w-2,cx+1)} {min(y+h-2,cy+1)}')


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--structures', required=True, type=Path)
    ap.add_argument('--subway', type=Path)
    ap.add_argument('--output', required=True, type=Path)
    ap.add_argument('--maps-dir', type=Path, help='Optional generated MAPS directory to receive matching .F3O sidecars')
    a=ap.parse_args(); a.output.mkdir(parents=True,exist_ok=True)
    if a.maps_dir: a.maps_dir.mkdir(parents=True,exist_ok=True)

    doc=json.loads(a.structures.read_text(encoding='utf-8'))
    index={'format':'PhoBoi.F3OIndex/2','maps':[]}
    for mp in doc.get('maps',[]):
        name=mp['map_name']; lines=['F3O 1', f'MAP {name}', f"CATEGORY {mp.get('category','wasteland')}"]
        for s in mp.get('structures',[]): emit_structure(lines,s)
        out=a.output/(name+'.F3O')
        out.write_text('\n'.join(lines)+'\n',encoding='utf-8')
        installed=None
        if a.maps_dir:
            installed=a.maps_dir/out.name
            shutil.copy2(out, installed)
        index['maps'].append({
            'map_name':name,
            'file':out.name,
            'installed_sidecar':str(installed) if installed else None,
            'commands':len(lines)-3,
        })

    if a.subway and a.subway.exists():
        subway=json.loads(a.subway.read_text(encoding='utf-8'))
        (a.output/'subway_runtime_jobs.json').write_text(json.dumps({
            'format':'PhoBoi.FO3SubwayRuntimeJobs/1',
            'jobs':subway.get('generation_jobs',[]),
            'links':subway.get('links',[])
        },indent=2),encoding='utf-8')

    (a.output/'runtime_index.json').write_text(json.dumps(index,indent=2),encoding='utf-8')
    print(f"Runtime layouts: {len(index['maps'])} maps -> {a.output}")
    if a.maps_dir:
        print(f"Installed F3O sidecars -> {a.maps_dir}")

if __name__=='__main__': main()
