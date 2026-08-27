#!/usr/bin/env python3
"""Build an initial classic-map metro graph from FO3 generated map manifest.

This stage identifies metro/station-related locations and links nearby stations.
It emits map-generation jobs for station, platform and tunnel segments. It does
not yet write final MAP object records.
"""

from __future__ import annotations
import argparse, json, math
from pathlib import Path

KEYS=('metro','station','subway','tunnel')


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--manifest',required=True,type=Path)
    ap.add_argument('--output',required=True,type=Path)
    a=ap.parse_args(); a.output.mkdir(parents=True,exist_ok=True)
    doc=json.loads(a.manifest.read_text(encoding='utf-8'))
    stations=[]
    for e in doc.get('generated_maps',[]):
        text=(str(e.get('marker_name',''))+' '+str(e.get('marker_type',''))).lower()
        if any(k in text for k in KEYS):
            stations.append({
                'id':e['map_name'], 'location':e.get('marker_name'),
                'cell':e.get('cell',[0,0]), 'surface_map':e['map_name']
            })
    links=[]
    for i,s in enumerate(stations):
        ds=[]
        for j,t in enumerate(stations):
            if i==j: continue
            dx=s['cell'][0]-t['cell'][0]; dy=s['cell'][1]-t['cell'][1]
            ds.append((math.hypot(dx,dy),j))
        for d,j in sorted(ds)[:2]:
            if i<j:
                links.append({'a':s['id'],'b':stations[j]['id'],'distance_cells':round(d,3),'segments':max(1,min(6,round(d)))})
    jobs=[]
    for s in stations:
        jobs += [
            {'kind':'metro_entrance','station':s['id'],'surface_map':s['surface_map']},
            {'kind':'station_concourse','station':s['id']},
            {'kind':'station_platform','station':s['id']},
        ]
    for n,l in enumerate(links):
        for seg in range(l['segments']):
            jobs.append({'kind':'tunnel_segment','id':f"METRO_LINK_{n:03d}_{seg:02d}",'from':l['a'],'to':l['b'],'segment':seg,'segment_count':l['segments']})
    out={'format':'PhoBoi.FO3MetroGraph/1','stations':stations,'links':links,'generation_jobs':jobs}
    (a.output/'subway_graph.json').write_text(json.dumps(out,indent=2),encoding='utf-8')
    print(f"Metro graph: {len(stations)} stations, {len(links)} links, {len(jobs)} generation jobs")

if __name__=='__main__': main()
