from pathlib import Path

path = Path("src/worldmap.cc")
text = path.read_text(encoding="utf-8")

marker = "COOP_PHOBOI_ALWAYS_AVAILABLE_V1"
if marker in text:
    print("Unified co-op PhoBoi access already applied")
    raise SystemExit(0)

old = '''bool wmMapPipboyActive()\n{\n    return gameMovieIsSeen(MOVIE_VSUIT);\n}\n'''
new = '''bool wmMapPipboyActive()\n{\n    // COOP_PHOBOI_ALWAYS_AVAILABLE_V1\n    // The unified co-op campaign owns PhoBoi from the start. The stock game\n    // gated Pip-Boy access on whether the Vault Suit movie had been seen,\n    // which can be false in custom/linked-world starts and makes the direct\n    // controller PhoBoi action appear broken. Do not inherit that story gate.\n    return true;\n}\n'''

if old not in text:
    raise SystemExit("wmMapPipboyActive legacy movie gate anchor not found")

text = text.replace(old, new, 1)
path.write_text(text, encoding="utf-8")
print("Removed legacy Vault Suit movie gate from unified co-op PhoBoi")
