from pathlib import Path

# The first materializer was written against an older include ordering in
# perk.cc. Rewrite only that brittle source anchor in-memory, then execute the
# otherwise identical patch. Keeping this shim tiny makes the materializer
# resilient without duplicating the full generated global-perk implementation.
root = Path(__file__).resolve().parents[1]
patch_path = root / "tools" / "patch_global_coop_perks.py"
source = patch_path.read_text(encoding="utf-8")

old_anchor = "'#include \"memory.h\"\\n#include \"object.h\"'"
old_replacement = "'#include \"memory.h\"\\n#include \"local_coop.h\"\\n#include \"object.h\"'"
new_anchor = "'#include \"object.h\"'"
new_replacement = "'#include \"local_coop.h\"\\n#include \"object.h\"'"

if old_anchor not in source or old_replacement not in source:
    raise RuntimeError("expected perk.cc include-anchor literals not found in base materializer")

source = source.replace(old_anchor, new_anchor, 1)
source = source.replace(old_replacement, new_replacement, 1)
exec(compile(source, str(patch_path), "exec"), {"__name__": "__main__", "__file__": str(patch_path)})
