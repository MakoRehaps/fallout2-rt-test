#!/usr/bin/env python3
from pathlib import Path


def read(path):
    return Path(path).read_text(encoding="utf-8")


def write(path, text):
    Path(path).write_text(text, encoding="utf-8")


# The ghost-save hook lives early in local_coop.h, before the later archetype
# count constant is declared. Keep the persisted byte bounded without depending
# on a declaration that does not exist yet at this point in the header.
coop_path = "src/local_coop.h"
coop = read(coop_path)
bad_archetype = '''        saved.archetype = static_cast<uint8_t>(std::clamp(player.archetype, 0, kLocalCoopArchetypeCount - 1));
'''
good_archetype = '''        // COOP_SAVED_GHOST_COMPILE_FIX_V1
        int savedArchetype = std::max(0, std::min(player.archetype, 255));
        saved.archetype = static_cast<uint8_t>(savedArchetype);
'''
if "COOP_SAVED_GHOST_COMPILE_FIX_V1" not in coop:
    if bad_archetype not in coop:
        raise SystemExit("mixed-slot ghost archetype compile anchor missing")
    coop = coop.replace(bad_archetype, good_archetype, 1)
write(coop_path, coop)


# The websocket routines occur before the final helper implementations in
# local_coop_mobile.cc. Forward-declare the two helpers they call rather than
# moving the implementation and destabilizing the generated phone source.
mobile_path = "src/local_coop_mobile.cc"
mobile = read(mobile_path)
if "PHOBOI_MIXED_SLOT_COMPILE_FIX_V1" not in mobile:
    anchor = '''void mobileResetInput(MobileSlotState& state);
'''
    replacement = '''void mobileResetInput(MobileSlotState& state);
// PHOBOI_MIXED_SLOT_COMPILE_FIX_V1
void mobileMarkTransportAlive(int slot);
void mobileMarkTransportClosedIfLast(int slot);
'''
    if anchor not in mobile:
        raise SystemExit("PhoBoi early mobileResetInput declaration missing")
    mobile = mobile.replace(anchor, replacement, 1)
write(mobile_path, mobile)


coop = read(coop_path)
mobile = read(mobile_path)
if "kLocalCoopArchetypeCount - 1" in coop[coop.find("COOP_SAVED_GHOST_SLOT_V1"):coop.find("COOP_SAVED_GHOST_SLOT_V1") + 1400]:
    raise SystemExit("late archetype-count dependency survived ghost compile fix")
for marker in (
    "COOP_SAVED_GHOST_COMPILE_FIX_V1",
    "PHOBOI_MIXED_SLOT_COMPILE_FIX_V1",
):
    if marker not in (coop + mobile):
        raise SystemExit(f"missing mixed-slot compile marker {marker}")

print("Fixed mixed slot allocator declaration order and early ghost-save bounds")
