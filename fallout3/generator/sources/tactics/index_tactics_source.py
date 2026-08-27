#!/usr/bin/env python3
"""Inventory a Fallout Tactics source tree/ZIP for later prefab extraction.

This is deliberately format-safe: it does not assume Tactics files are directly
compatible with Fallout 1/2. It creates a provenance manifest that later source-
specific parsers can consume.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import zipfile
from collections import Counter
from pathlib import Path


def sha1_bytes(data: bytes) -> str:
    return hashlib.sha1(data).hexdigest()


def classify(path: str) -> str:
    p = path.lower()
    if p.endswith((".mis", ".map")):
        return "map_candidate"
    if p.endswith((".spr", ".frm", ".png", ".bmp", ".tga")):
        return "art_candidate"
    if p.endswith((".pro", ".ent", ".xml", ".txt", ".ini")):
        return "definition_candidate"
    if p.endswith((".wav", ".acm", ".ogg")):
        return "audio"
    return "other"


def from_zip(path: Path):
    with zipfile.ZipFile(path) as z:
        for info in z.infolist():
            if info.is_dir():
                continue
            data = z.read(info.filename)
            yield {
                "path": info.filename.replace("\\", "/"),
                "size": len(data),
                "sha1": sha1_bytes(data),
                "kind": classify(info.filename),
            }


def from_dir(path: Path):
    for p in path.rglob("*"):
        if not p.is_file():
            continue
        data = p.read_bytes()
        rel = p.relative_to(path).as_posix()
        yield {
            "path": rel,
            "size": len(data),
            "sha1": sha1_bytes(data),
            "kind": classify(rel),
        }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("source", type=Path, help="Fallout Tactics folder or ZIP")
    ap.add_argument("--output", type=Path, default=Path("tactics_source_manifest.json"))
    args = ap.parse_args()

    if args.source.is_dir():
        files = list(from_dir(args.source))
    elif zipfile.is_zipfile(args.source):
        files = list(from_zip(args.source))
    else:
        raise SystemExit("source must be a directory or ZIP")

    counts = Counter(x["kind"] for x in files)
    doc = {
        "format": "PhoBoi.TacticsSourceManifest/1",
        "source": str(args.source),
        "counts": dict(counts),
        "files": files,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(doc, indent=2), encoding="utf-8")
    print(f"Indexed {len(files)} files -> {args.output}")
    print(dict(counts))


if __name__ == "__main__":
    main()
