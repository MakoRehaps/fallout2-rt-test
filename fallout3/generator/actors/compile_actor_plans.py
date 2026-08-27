#!/usr/bin/env python3
"""Compile Fallout 3 actor/reference scan data into isolated semantic actor plans.

This tool is build-time only. It does not touch the engine and does not emit
runtime hooks. Output is JSON intended for the later clean native MAP writer.
"""

from __future__ import annotations

import argparse
import json
import math
import zipfile
from collections import defaultdict
from pathlib import Path

CELL = 4096.0


def iter_jsonl_from_zip(path: Path, suffix: str):
    with zipfile.ZipFile(path) as z:
        names=[n for n in z.namelist() if n.endswith('/'+suffix) or n==suffix]
        if not names:
            return
        with z.open(names[0]) as f:
            for raw in f:
                try:
                    yield json.loads(raw)
                except Exception:
                    continue


def fid(v):
    if v is None:
        return ''
    return str(v).strip().upper()


def text_blob(record):
    parts=[]
    for key in ('editor_id','name','full_name','race','class','base_signature','signature','type'):
        value=record.get(key)
        if value not in (None,''):
            parts.append(str(value))
    raw=record.get('raw') or {}
    for field in raw.get('fields',[]) or []:
        value=field.get('value')
        if value not in (None,''):
            parts.append(str(value))
    return ' '.join(parts).lower()


def classify_actor(record):
    s=text_blob(record)
    if any(k in s for k in ('supermutant','super mutant','mutant')):
        return 'super_mutant'
    if 'ghoul' in s:
        return 'ghoul'
    if any(k in s for k in ('robot','protectron','robobrain','sentry','mister handy','mr handy')):
        return 'robot'
    if any(k in s for k in ('dog','molerat','mole rat','radscorpion','yao guai','deathclaw','brahmin','bloatfly','ant','radroach','creature')):
        return 'creature'
    return 'human'


def actor_name(record):
    for key in ('name','full_name','editor_id','form_id'):
        value=record.get(key)
        if value not in (None,''):
            return str(value)
    return 'FO3 Actor'


def pos(record):
    try:
        x=float(record.get('pos_x',0.0)); y=float(record.get('pos_y',0.0)); z=float(record.get('pos_z',0.0))
    except Exception:
        x=y=z=0.0
    return x,y,z


def nearest_map(entries,cx,cy):
    if not entries:
        return None
    return min(entries,key=lambda e: abs(int(e['cell'][0])-cx)+abs(int(e['cell'][1])-cy))


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--scan',required=True,type=Path)
    ap.add_argument('--manifest',required=True,type=Path)
    ap.add_argument('--output',required=True,type=Path)
    a=ap.parse_args(); a.output.parent.mkdir(parents=True,exist_ok=True)

    manifest=json.loads(a.manifest.read_text(encoding='utf-8'))
    maps=manifest.get('generated_maps',[])
    if not maps:
        raise SystemExit('manifest contains no generated maps')

    bases={}
    for r in iter_jsonl_from_zip(a.scan,'actors.jsonl') or []:
        form=fid(r.get('form_id'))
        if form and form not in bases:
            bases[form]=r

    placements=[]; seen=set(); per_map=defaultdict(int)
    for r in iter_jsonl_from_zip(a.scan,'references.jsonl') or []:
        base=fid(r.get('base_form_id') or r.get('base_id') or r.get('base'))
        if base not in bases:
            continue
        ref=fid(r.get('form_id'))
        if ref and ref in seen:
            continue
        if ref:
            seen.add(ref)
        x,y,z=pos(r); cx=math.floor(x/CELL); cy=math.floor(y/CELL)
        target=nearest_map(maps,cx,cy)
        if target is None:
            continue
        base_rec=bases[base]
        role=classify_actor(base_rec)
        per_map[target['map_name']]+=1
        placements.append({
            'reference_form_id':ref,
            'base_form_id':base,
            'name':actor_name(base_rec),
            'semantic_class':role,
            'source_position':[x,y,z],
            'source_cell':[cx,cy],
            'target_map':target['map_name'],
            'target_map_id':int(target['map_id']),
            'target_category':target.get('category','wasteland'),
            'placement_policy':'nearest-generated-location',
            'prototype_role':{
                'human':'classic_humanoid',
                'ghoul':'classic_ghoul',
                'super_mutant':'classic_super_mutant',
                'robot':'classic_robot',
                'creature':'classic_creature',
            }[role],
        })

    out={
        'format':'PhoBoi.FO3ActorPlans/1',
        'scope':'build-time-only',
        'actor_bases':len(bases),
        'placements':placements,
        'counts_by_map':dict(sorted(per_map.items())),
        'note':'Semantic actor plans only. Concrete classic PIDs and native MAP object records are resolved by later generator-side compilation.',
    }
    a.output.write_text(json.dumps(out,indent=2),encoding='utf-8')
    print(f"Actor plans: bases={len(bases)} placements={len(placements)} -> {a.output}")


if __name__=='__main__': main()
