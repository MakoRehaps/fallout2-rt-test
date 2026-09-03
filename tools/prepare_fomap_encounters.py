#!/usr/bin/env python3
"""Extract safe encounter-layout references from a user-owned FOnline map archive.

The runtime never bundles these maps. This tool copies only the plain Fallout 2
random-encounter FOMAP layouts we intentionally support into GameData so the CE
runtime can use their tile/roof geometry while keeping CE actors and scripts.
Mountain layouts are deliberately excluded: mountain terrain reuses desert
layouts in the unified road system.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import sys
import zipfile


DESERT = tuple(f"desert{i}.fomap" for i in range(1, 13))
CITY = tuple(f"city{i}.fomap" for i in range(1, 10))
COAST = tuple(f"coast{i}.fomap" for i in (*range(1, 5), *range(6, 13)))
SUPPORTED = frozenset(DESERT + CITY + COAST)
SOURCE_PREFIX = "Fallout 2/RE/"


def safe_member_name(name: str) -> str | None:
    if not name.startswith(SOURCE_PREFIX):
        return None
    base = name[len(SOURCE_PREFIX):]
    if "/" in base or "\\" in base:
        return None
    lowered = base.lower()
    return lowered if lowered in SUPPORTED else None


def extract_archive(archive: Path, destination: Path) -> int:
    destination.mkdir(parents=True, exist_ok=True)
    copied = 0
    found: set[str] = set()

    with zipfile.ZipFile(archive) as zf:
        for info in zf.infolist():
            target_name = safe_member_name(info.filename)
            if target_name is None or info.is_dir():
                continue
            target = destination / target_name
            with zf.open(info, "r") as src, target.open("wb") as dst:
                shutil.copyfileobj(src, dst)
            found.add(target_name)
            copied += 1

    missing = sorted(SUPPORTED - found)
    if missing:
        print("warning: archive did not contain: " + ", ".join(missing), file=sys.stderr)
    return copied


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Prepare plain FOMAP encounter layouts for Fallout Unified Co-op."
    )
    parser.add_argument("archive", type=Path, help="Path to the user-owned official maps.zip")
    parser.add_argument(
        "--game-root",
        type=Path,
        default=Path("."),
        help="Unified game install root (default: current directory)",
    )
    args = parser.parse_args()

    if not args.archive.is_file():
        parser.error(f"archive not found: {args.archive}")

    destination = args.game_root / "GameData" / "EncounterLayouts" / "Fallout2" / "RE"
    copied = extract_archive(args.archive, destination)
    print(f"prepared {copied} encounter layouts in {destination}")
    print("mountain maps were intentionally skipped; mountain terrain reuses desert layouts")
    return 0 if copied else 1


if __name__ == "__main__":
    raise SystemExit(main())