from pathlib import Path

root = Path(__file__).resolve().parents[1]
coop_h = root / "src" / "local_coop.h"
mobile_cc = root / "src" / "local_coop_mobile.cc"

h = coop_h.read_text(encoding="utf-8")
cc = mobile_cc.read_text(encoding="utf-8")

# Let local_coop.h ask the mobile backend whether a controller handle is owned
# by a virtual phone device. Mobile-owned handles are closed exactly once by the
# mobile backend instead of by both localCoopClearController and mobileDetach.
anchor = "void scriptsRequestWorldMap();\n"
if "bool localCoopMobileOwnsController(SDL_GameController* controller);" not in h:
    h = h.replace(anchor, anchor + "bool localCoopMobileOwnsController(SDL_GameController* controller);\n", 1)

old = """inline void localCoopClearController(LocalCoopPlayer& player)\n{\n    if (player.controller != nullptr) {\n        SDL_GameControllerClose(player.controller);\n    }\n"""
new = """inline void localCoopClearController(LocalCoopPlayer& player)\n{\n    // PHOBOI_MOBILE_CONTROLLER_SINGLE_OWNER_V1\n    // Virtual phone controllers are owned by local_coop_mobile.cc. Closing the\n    // same SDL_GameController here and again during virtual-device detach causes\n    // an MSVC debug-heap double free (0xDD freed-memory pattern). Physical pads\n    // remain owned and closed here.\n    if (player.controller != nullptr && !localCoopMobileOwnsController(player.controller)) {\n        SDL_GameControllerClose(player.controller);\n    }\n"""
if old in h:
    h = h.replace(old, new, 1)
elif "PHOBOI_MOBILE_CONTROLLER_SINGLE_OWNER_V1" not in h:
    raise SystemExit("localCoopClearController anchor not found")

# Export ownership query from the mobile backend.
insert_anchor = "namespace fallout {\nnamespace {\n"
if "bool localCoopMobileOwnsController(SDL_GameController* controller)" not in cc:
    # Function must live outside the anonymous namespace, so insert declaration
    # before it and definition after the device table is available through a
    # tiny internal helper.
    cc = cc.replace(insert_anchor, "namespace fallout {\n\nbool localCoopMobileOwnsController(SDL_GameController* controller);\n\nnamespace {\n", 1)

helper_anchor = "std::array<MobileVirtualDevice, kLocalCoopMaxPlayers> gMobileDevices;\n"
if "mobileOwnsControllerInternal" not in cc:
    cc = cc.replace(helper_anchor, helper_anchor + "\nbool mobileOwnsControllerInternal(SDL_GameController* controller)\n{\n    if (controller == nullptr) {\n        return false;\n    }\n    for (const MobileVirtualDevice& device : gMobileDevices) {\n        if (device.controller == controller) {\n            return true;\n        }\n    }\n    return false;\n}\n", 1)

# Find end of anonymous namespace and place exported wrapper immediately after it.
# Existing file has a single anonymous namespace used by the backend; this exact
# marker occurs before public localCoopMobile* functions.
public_marker = "} // namespace\n\nvoid localCoopMobileStartServer()"
if "bool localCoopMobileOwnsController(SDL_GameController* controller)\n{" not in cc:
    replacement = "} // namespace\n\nbool localCoopMobileOwnsController(SDL_GameController* controller)\n{\n    return mobileOwnsControllerInternal(controller);\n}\n\nvoid localCoopMobileStartServer()"
    if public_marker not in cc:
        raise SystemExit("mobile public namespace marker not found")
    cc = cc.replace(public_marker, replacement, 1)

# Ensure mobile detach closes the owned handle exactly once after clearing the
# player binding, regardless of whether the player still points at it.
old_detach = """    LocalCoopPlayer& player = gLocalCoopPlayers[slot];\n    if (player.controller == device.controller) {\n        localCoopClearController(player);\n    } else if (device.controller != nullptr) {\n        SDL_GameControllerClose(device.controller);\n    }\n\n    SDL_JoystickDetachVirtual(device.deviceIndex);\n"""
new_detach = """    LocalCoopPlayer& player = gLocalCoopPlayers[slot];\n    if (player.controller == device.controller) {\n        localCoopClearController(player);\n    }\n\n    // PHOBOI_MOBILE_CONTROLLER_SINGLE_OWNER_V1\n    // The mobile device owns this handle and closes it exactly once.\n    if (device.controller != nullptr) {\n        SDL_GameControllerClose(device.controller);\n        device.controller = nullptr;\n        device.joystick = nullptr;\n    }\n\n    SDL_JoystickDetachVirtual(device.deviceIndex);\n"""
if old_detach in cc:
    cc = cc.replace(old_detach, new_detach, 1)
elif "The mobile device owns this handle and closes it exactly once." not in cc:
    raise SystemExit("mobile detach anchor not found")

coop_h.write_text(h, encoding="utf-8")
mobile_cc.write_text(cc, encoding="utf-8")
print("patched mobile controller single-owner lifetime")
