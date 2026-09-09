from pathlib import Path

p = Path('src/local_coop.h')
s = p.read_text(encoding='utf-8')
marker = '// COOP_18_KITS_REPAIR_V1'
if marker in s:
    print('18 kit repair already applied')
    raise SystemExit(0)

if 'inline constexpr int kLocalCoopArchetypeCount = 18;' not in s:
    raise SystemExit('18 archetypes are not present')
if 'kLocalCoopArchetypeRoles' not in s:
    raise SystemExit('18 archetype join roles are not present')

if '#include "platform_compat.h"\n' not in s:
    anchor = '#include "party_member.h"\n'
    if anchor not in s:
        raise SystemExit('party_member include anchor missing')
    s = s.replace(anchor, anchor + '#include "platform_compat.h"\n', 1)

insert_anchor = '''};

inline bool localCoopApplyPlayerOneArchetype(int archetype, int gender)
'''
kit_block = '''};

// COOP_18_KITS_REPAIR_V1
struct LocalCoopStarterKitEntry {
    const char* itemName;
    int quantity;
};

inline constexpr int kLocalCoopStarterKitItems = 4;
inline constexpr LocalCoopStarterKitEntry kLocalCoopStarterKits[kLocalCoopArchetypeCount][kLocalCoopStarterKitItems] = {
    { { "Sledgehammer", 1 }, { "Leather Jacket", 1 }, { "Stimpak", 2 }, { nullptr, 0 } },
    { { "10mm Pistol", 1 }, { "10mm JHP", 2 }, { "Stimpak", 1 }, { nullptr, 0 } },
    { { "Hunting Rifle", 1 }, { ".223 FMJ", 2 }, { "Stimpak", 1 }, { nullptr, 0 } },
    { { "10mm SMG", 1 }, { "10mm JHP", 2 }, { "Stimpak", 1 }, { nullptr, 0 } },
    { { "Flamer", 1 }, { "Flamethrower Fuel", 1 }, { "Leather Jacket", 1 }, { nullptr, 0 } },
    { { "Spear", 1 }, { "Leather Jacket", 1 }, { "Stimpak", 1 }, { nullptr, 0 } },
    { { "Brass Knuckles", 1 }, { "Leather Jacket", 1 }, { "Stimpak", 1 }, { nullptr, 0 } },
    { { "10mm Pistol", 1 }, { "10mm JHP", 1 }, { "Rope", 1 }, { "Stimpak", 1 } },
    { { "Spear", 1 }, { "Rope", 1 }, { "Antidote", 1 }, { "Stimpak", 1 } },
    { { "10mm Pistol", 1 }, { "Stimpak", 4 }, { "First Aid Kit", 1 }, { "Doctor's Bag", 1 } },
    { { "10mm Pistol", 1 }, { "Mentats", 1 }, { "Stimpak", 1 }, { nullptr, 0 } },
    { { "10mm Pistol", 1 }, { "Tool", 1 }, { "Stimpak", 1 }, { nullptr, 0 } },
    { { "10mm Pistol", 1 }, { "Mentats", 1 }, { "Stimpak", 1 }, { nullptr, 0 } },
    { { "Knife", 1 }, { "Lockpicks", 1 }, { "Stimpak", 1 }, { nullptr, 0 } },
    { { "10mm Pistol", 1 }, { "Dynamite", 1 }, { "Stimpak", 1 }, { nullptr, 0 } },
    { { "Hunting Rifle", 1 }, { ".223 FMJ", 1 }, { "Rope", 1 }, { "Stimpak", 1 } },
    { { "10mm Pistol", 1 }, { "10mm JHP", 1 }, { "Stimpak", 2 }, { nullptr, 0 } },
    { { "Laser Pistol", 1 }, { "Small Energy Cell", 2 }, { "Stimpak", 1 }, { nullptr, 0 } },
};

inline int localCoopFindStarterItemPid(const char* wantedName)
{
    if (wantedName == nullptr || *wantedName == '\\0') {
        return -1;
    }
    int maxItemId = proto_max_id(OBJ_TYPE_ITEM);
    for (int id = 0; id <= maxItemId; id++) {
        int pid = (OBJ_TYPE_ITEM << 24) | id;
        Proto* proto = nullptr;
        if (protoGetProto(pid, &proto) != 0 || proto == nullptr) {
            continue;
        }
        const char* name = protoGetName(pid);
        if (name != nullptr && compat_stricmp(name, wantedName) == 0) {
            return pid;
        }
    }
    return -1;
}

inline void localCoopGrantStarterKit(int slot, int archetype)
{
    if (slot <= 0 || slot >= kLocalCoopMaxPlayers || gDude == nullptr) {
        return;
    }
    archetype = std::max(0, std::min(archetype, kLocalCoopArchetypeCount - 1));
    for (int index = 0; index < kLocalCoopStarterKitItems; index++) {
        const LocalCoopStarterKitEntry& entry = kLocalCoopStarterKits[archetype][index];
        if (entry.itemName == nullptr || entry.quantity <= 0) {
            continue;
        }
        int pid = localCoopFindStarterItemPid(entry.itemName);
        if (pid == -1) {
            debugPrint("[COOP KIT] slot=%d archetype=%s item-missing=%s\\n", slot, kLocalCoopArchetypeNames[archetype], entry.itemName);
            continue;
        }
        Object* item = nullptr;
        if (objectCreateWithPid(&item, pid) != 0 || item == nullptr) {
            continue;
        }
        if (itemAdd(gDude, item, entry.quantity) != 0) {
            objectDestroy(item, nullptr);
            continue;
        }
        debugPrint("[COOP KIT] slot=%d archetype=%s added=%s x%d\\n", slot, kLocalCoopArchetypeNames[archetype], entry.itemName, entry.quantity);
    }
}

inline bool localCoopApplyPlayerOneArchetype(int archetype, int gender)
'''
if insert_anchor not in s:
    raise SystemExit('kit insertion anchor missing')
s = s.replace(insert_anchor, kit_block, 1)

create_anchor = '''    LocalCoopPlayer& player = gLocalCoopPlayers[slot];
    int archetype = std::max(0, std::min(player.archetype, kLocalCoopArchetypeCount - 1));
    int pid = protoConfigureLocalCoopPlayer(
'''
create_new = '''    LocalCoopPlayer& player = gLocalCoopPlayers[slot];
    int archetype = std::max(0, std::min(player.archetype, kLocalCoopArchetypeCount - 1));
    LocalCoopCharacterSlotState& saved = localCoopCharacterStateGet().slots[slot];
    bool grantStarterKit = saved.locked == 0;
    int pid = protoConfigureLocalCoopPlayer(
'''
if create_anchor not in s:
    raise SystemExit('player creation anchor missing')
s = s.replace(create_anchor, create_new, 1)

saved_anchor = '''    LocalCoopCharacterSlotState& saved =
        localCoopCharacterStateGet().slots[slot];
    saved.locked = 1;
'''
if saved_anchor not in s:
    raise SystemExit('saved slot anchor missing')
s = s.replace(saved_anchor, '''    saved.locked = 1;
''', 1)

grant_anchor = '''        "%s",
        player.controllerGuid);

    debugPrint(
        "[COOP JOIN] slot=%d locked archetype=%s pid=%d tile=%d\\n",
'''
grant_new = '''        "%s",
        player.controllerGuid);

    if (grantStarterKit) {
        localCoopGrantStarterKit(slot, archetype);
    }

    debugPrint(
        "[COOP JOIN] slot=%d locked archetype=%s pid=%d tile=%d\\n",
'''
if grant_anchor not in s:
    raise SystemExit('kit grant anchor missing')
s = s.replace(grant_anchor, grant_new, 1)

p.write_text(s, encoding='utf-8')
print('Repaired 18 one-time archetype starter kits')
