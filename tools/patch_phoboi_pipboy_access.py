from pathlib import Path

# Cross-reference Fallout2-CE's existing sfall PipBoyAvailableAtGameStart
# mechanism instead of lying to worldmap state. Unified co-op forces that
# setting on during bootstrap, and phoboiOpen follows the upstream availability
# condition while preserving ordinary wmMapPipboyActive() semantics.

# ---------------------------------------------------------------------------
# sfall_config.h: restore/add the upstream setting name and force it for the
# unified co-op profile before any game-specific profile overrides.
# ---------------------------------------------------------------------------
path = Path("src/sfall_config.h")
text = path.read_text(encoding="utf-8")

constant = '#define SFALL_CONFIG_PIPBOY_AVAILABLE_AT_GAMESTART "PipBoyAvailableAtGameStart"'
if constant not in text:
    anchor = '#define SFALL_CONFIG_PATCH_FILE "PatchFile"\n'
    if anchor not in text:
        raise SystemExit("sfall PipBoy constant anchor not found")
    text = text.replace(anchor, anchor + constant + '\n', 1)

profile_marker = "COOP_UPSTREAM_PIPBOY_AVAILABLE_CONFIG_V1"
if profile_marker not in text:
    old = '''    bool initialized = sfallConfigInit(argc, argv);\n    if (!initialized || unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {\n        return initialized;\n    }\n'''
    new = '''    bool initialized = sfallConfigInit(argc, argv);\n    // COOP_UPSTREAM_PIPBOY_AVAILABLE_CONFIG_V1\n    // Fallout2-CE already exposes PipBoyAvailableAtGameStart. Unified co-op\n    // owns PhoBoi from the start in both linked campaigns, so use that proven\n    // availability path instead of modifying world-map movie state.\n    if (initialized) {\n        configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_PIPBOY_AVAILABLE_AT_GAMESTART, 1);\n    }\n    if (!initialized || unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {\n        return initialized;\n    }\n'''
    if old not in text:
        raise SystemExit("unified sfall profile bootstrap anchor not found")
    text = text.replace(old, new, 1)
path.write_text(text, encoding="utf-8")

# ---------------------------------------------------------------------------
# sfall_config.cc: match upstream's default so the setting exists before the
# config file and command line are parsed. The co-op wrapper above then forces 1.
# ---------------------------------------------------------------------------
path = Path("src/sfall_config.cc")
text = path.read_text(encoding="utf-8")
if "COOP_UPSTREAM_PIPBOY_CONFIG_DEFAULT_V1" not in text:
    anchor = '    configSetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_PATCH_FILE, "");\n'
    if anchor not in text:
        raise SystemExit("sfall config default anchor not found")
    addition = '''    // COOP_UPSTREAM_PIPBOY_CONFIG_DEFAULT_V1\n    configSetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_PIPBOY_AVAILABLE_AT_GAMESTART, 0);\n'''
    text = text.replace(anchor, anchor + addition, 1)
path.write_text(text, encoding="utf-8")

# ---------------------------------------------------------------------------
# pipboy.cc: port upstream's availability flag/read and use it in phoboiOpen.
# ---------------------------------------------------------------------------
path = Path("src/pipboy.cc")
text = path.read_text(encoding="utf-8")

if '#include "sfall_config.h"' not in text:
    anchor = '#include "scripts.h"\n'
    if anchor not in text:
        raise SystemExit("pipboy include anchor not found")
    text = text.replace(anchor, anchor + '#include "sfall_config.h"\n', 1)

if "COOP_UPSTREAM_PIPBOY_AVAILABLE_FLAG_V1" not in text:
    anchor = 'bool gPipboyWindowIsoWasEnabled = false;\n'
    if anchor not in text:
        raise SystemExit("pipboy global flag anchor not found")
    text = text.replace(anchor, anchor + '''\n// COOP_UPSTREAM_PIPBOY_AVAILABLE_FLAG_V1\n// Mirrors Fallout2-CE's proven PipBoyAvailableAtGameStart behavior.\nstatic bool gPhoBoiAvailableAtGameStart = false;\n''', 1)

if "COOP_UPSTREAM_PIPBOY_OPEN_GATE_V1" not in text:
    old = '''int phoboiOpen(int intent)\n{\n    if (!wmMapPipboyActive()) {\n'''
    new = '''int phoboiOpen(int intent)\n{\n    // COOP_UPSTREAM_PIPBOY_OPEN_GATE_V1\n    if (!wmMapPipboyActive() && !gPhoBoiAvailableAtGameStart) {\n'''
    if old not in text:
        raise SystemExit("phoboiOpen availability anchor not found")
    text = text.replace(old, new, 1)

if "COOP_UPSTREAM_PIPBOY_INIT_V1" not in text:
    old = '''void pipboyInit()\n{\n    _pip_init_();\n}\n'''
    new = '''void pipboyInit()\n{\n    _pip_init_();\n\n    // COOP_UPSTREAM_PIPBOY_INIT_V1\n    int value = 0;\n    if (configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_PIPBOY_AVAILABLE_AT_GAMESTART, &value)) {\n        gPhoBoiAvailableAtGameStart = value == 1 || value == 2;\n    } else {\n        gPhoBoiAvailableAtGameStart = false;\n    }\n}\n'''
    if old not in text:
        raise SystemExit("pipboyInit anchor not found")
    text = text.replace(old, new, 1)
path.write_text(text, encoding="utf-8")

# ---------------------------------------------------------------------------
# Undo the previous workaround if it was ever materialized into worldmap.cc.
# wmMapPipboyActive must retain its real map/movie semantics.
# ---------------------------------------------------------------------------
path = Path("src/worldmap.cc")
text = path.read_text(encoding="utf-8")
old_materialized = '''bool wmMapPipboyActive()\n{\n    // COOP_PHOBOI_ALWAYS_AVAILABLE_V1\n    // The unified co-op campaign owns PhoBoi from the start. The stock game\n    // gated Pip-Boy access on whether the Vault Suit movie had been seen,\n    // which can be false in custom/linked-world starts and makes the direct\n    // controller PhoBoi action appear broken. Do not inherit that story gate.\n    return true;\n}\n'''
if old_materialized in text:
    text = text.replace(old_materialized, '''bool wmMapPipboyActive()\n{\n    return gameMovieIsSeen(MOVIE_VSUIT);\n}\n''', 1)
if "COOP_PHOBOI_ALWAYS_AVAILABLE_V1" in text:
    raise SystemExit("legacy always-true worldmap PipBoy workaround survived")
path.write_text(text, encoding="utf-8")

for p, marker in (
    ("src/sfall_config.h", "COOP_UPSTREAM_PIPBOY_AVAILABLE_CONFIG_V1"),
    ("src/sfall_config.cc", "COOP_UPSTREAM_PIPBOY_CONFIG_DEFAULT_V1"),
    ("src/pipboy.cc", "COOP_UPSTREAM_PIPBOY_AVAILABLE_FLAG_V1"),
    ("src/pipboy.cc", "COOP_UPSTREAM_PIPBOY_OPEN_GATE_V1"),
    ("src/pipboy.cc", "COOP_UPSTREAM_PIPBOY_INIT_V1"),
):
    if marker not in Path(p).read_text(encoding="utf-8"):
        raise SystemExit(f"missing PipBoy cross-reference marker {marker} in {p}")

print("Applied upstream-style PipBoy availability without corrupting worldmap state")
