#!/usr/bin/env python3
"""Build image control maps from PhoBoi Fallout 3 xEdit scan data.

Outputs portable grayscale PGM images (no third-party Python dependency):
  terrain_roughness.pgm
  settlement_density.pgm
  buildability.pgm

These are generator control images, not game art. A later placement stage samples
these images to choose terrain chunks and structure density.
"""

from __future__ import annotations
import argparse, json, math, zipfile
from collections import defaultdict
from pathlib import Path

CELL = 4096.0


def jlines(z, suffix):
    names = [n for n in z.namelist() if n == suffix or n.endswith('/' + suffix)]
    if not names:
        return
    with z.open(names[0]) as f:
        for raw in f:
            try:
                yield json.loads(raw)
            except Exception:
                pass


def rough_blob(s):
    try: b = bytes.fromhex(s)
    except Exception: return 0.0
    vals = [x - 256 if x > 127 else x for x in b[4:]]
    if not vals: return 0.0
    mean = sum(vals) / len(vals)
    am = sum(abs(x) for x in vals) / len(vals)
    sd = math.sqrt(sum((x - mean) ** 2 for x in vals) / len(vals))
    return am + sd * 0.35


def save_pgm(path, grid):
    h = len(grid); w = len(grid[0]) if h else 0
    raw = bytearray()
    for row in grid:
        raw.extend(max(0, min(255, int(v))) for v in row)
    path.write_bytes(f'P5\n{w} {h}\n255\n'.encode('ascii') + raw)


def normalize(values, lo=0, hi=255):
    flat = [v for row in values for v in row]
    if not flat: return values
    mn, mx = min(flat), max(flat)
    if mx <= mn: return [[lo for _ in row] for row in values]
    scale = (hi - lo) / (mx - mn)
    return [[lo + (v - mn) * scale for v in row] for row in values]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--scan', required=True, type=Path)
    ap.add_argument('--output', required=True, type=Path)
    args = ap.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)

    rough = defaultdict(list)
    density = defaultdict(int)

    with zipfile.ZipFile(args.scan) as z:
        for r in jlines(z, 'terrain.jsonl') or []:
            x, y = int(r.get('grid_x', 0)), int(r.get('grid_y', 0))
            blob = ''
            for f in r.get('vhgt_fields') or []:
                if f.get('path') == 'VHGT':
                    blob = f.get('value', ''); break
            rough[(x, y)].append(rough_blob(blob))

        for r in jlines(z, 'references.jsonl') or []:
            try:
                x = math.floor(float(r.get('pos_x', 0)) / CELL)
                y = math.floor(float(r.get('pos_y', 0)) / CELL)
            except Exception:
                continue
            density[(x, y)] += 1

    keys = set(rough) | set(density)
    if not keys:
        raise SystemExit('No exterior scan data found')
    xs = [k[0] for k in keys]; ys = [k[1] for k in keys]
    minx, maxx, miny, maxy = min(xs), max(xs), min(ys), max(ys)
    w, h = maxx-minx+1, maxy-miny+1

    rg = [[0.0]*w for _ in range(h)]
    dg = [[0.0]*w for _ in range(h)]
    for (x,y), vals in rough.items(): rg[maxy-y][x-minx] = sum(vals)/len(vals)
    for (x,y), n in density.items(): dg[maxy-y][x-minx] = math.log1p(n)

    rn = normalize(rg)
    dn = normalize(dg)
    # Buildability favors flatter terrain and populated/built regions.
    bg = []
    for yy in range(h):
        row=[]
        for xx in range(w):
            flatness = 255 - rn[yy][xx]
            settlement = dn[yy][xx]
            row.append(flatness * 0.65 + settlement * 0.35)
        bg.append(row)

    save_pgm(args.output/'terrain_roughness.pgm', rn)
    save_pgm(args.output/'settlement_density.pgm', dn)
    save_pgm(args.output/'buildability.pgm', bg)
    (args.output/'control_map_meta.json').write_text(json.dumps({
        'format':'PhoBoi.FO3ControlMaps/1',
        'cell_bounds':{'min_x':minx,'max_x':maxx,'min_y':miny,'max_y':maxy},
        'image_size':[w,h],
        'files':['terrain_roughness.pgm','settlement_density.pgm','buildability.pgm']
    }, indent=2), encoding='utf-8')
    print(f'Control maps: {w}x{h} -> {args.output}')

if __name__ == '__main__': main()
