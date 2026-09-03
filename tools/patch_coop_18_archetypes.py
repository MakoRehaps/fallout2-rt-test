from pathlib import Path

p = Path('src/local_coop.h')
s = p.read_text(encoding='utf-8')

# The first version expands the four coarse templates into 18 Fallout-style
# builds. Keep this patch idempotent because CI applies helpers on every build.
if '// COOP_18_ARCHETYPES_V1' not in s:
    old = '''inline constexpr int kLocalCoopArchetypeCount = 4;
inline constexpr const char* kLocalCoopArchetypeNames[kLocalCoopArchetypeCount] = {
    "WASTELAND FIGHTER",
    "SCOUT",
    "MEDIC",
    "TECH SPECIALIST",
};

inline constexpr int kLocalCoopArchetypeStats[kLocalCoopArchetypeCount][PRIMARY_STAT_COUNT] = {
    { 7, 5, 7, 4, 5, 7, 5 },
    { 5, 8, 5, 4, 6, 8, 4 },
    { 4, 6, 5, 6, 8, 6, 5 },
    { 4, 6, 4, 5, 9, 6, 6 },
};
'''
    new = '''// COOP_18_ARCHETYPES_V1
inline constexpr int kLocalCoopArchetypeCount = 18;
inline constexpr const char* kLocalCoopArchetypeNames[kLocalCoopArchetypeCount] = {
    "BRUISER",
    "GUNSLINGER",
    "SNIPER",
    "COMMANDO",
    "HEAVY GUNNER",
    "MELEE RAIDER",
    "UNARMED BRAWLER",
    "SCOUT",
    "SURVIVALIST",
    "MEDIC",
    "SCIENTIST",
    "MECHANIC",
    "DIPLOMAT",
    "THIEF",
    "DEMOLITIONIST",
    "RANGER",
    "LUCKY DRIFTER",
    "ENERGY SPECIALIST",
};

inline constexpr const char* kLocalCoopArchetypeRoles[kLocalCoopArchetypeCount] = {
    "TOUGH / CLOSE COMBAT",
    "PISTOLS / CRITICALS",
    "RIFLES / LONG RANGE",
    "AUTOMATICS / MOBILITY",
    "BIG GUNS / ENDURANCE",
    "MELEE / ARMOR",
    "UNARMED / SPEED",
    "SCOUTING / SMALL GUNS",
    "OUTDOORS / SELF RELIANCE",
    "FIRST AID / DOCTOR",
    "SCIENCE / ENERGY",
    "REPAIR / UTILITY",
    "SPEECH / BARTER",
    "LOCKPICK / STEAL",
    "TRAPS / EXPLOSIVES",
    "RIFLES / SURVIVAL",
    "LUCK / GENERALIST",
    "ENERGY WEAPONS / SCIENCE",
};

inline constexpr int kLocalCoopArchetypeStats[kLocalCoopArchetypeCount][PRIMARY_STAT_COUNT] = {
    { 8, 5, 8, 3, 4, 7, 5 },
    { 4, 8, 5, 5, 6, 8, 4 },
    { 4, 9, 4, 4, 7, 7, 5 },
    { 6, 7, 6, 3, 5, 8, 5 },
    { 8, 5, 8, 3, 4, 6, 6 },
    { 8, 5, 8, 4, 4, 7, 4 },
    { 7, 6, 7, 4, 5, 8, 3 },
    { 5, 8, 5, 4, 6, 8, 4 },
    { 6, 7, 7, 4, 6, 6, 4 },
    { 4, 6, 5, 6, 8, 6, 5 },
    { 3, 6, 4, 5, 10, 6, 6 },
    { 5, 6, 5, 4, 9, 6, 5 },
    { 3, 6, 4, 10, 7, 5, 5 },
    { 4, 7, 4, 5, 6, 9, 5 },
    { 5, 7, 5, 3, 7, 7, 6 },
    { 6, 8, 6, 4, 6, 6, 4 },
    { 4, 6, 5, 6, 5, 6, 8 },
    { 4, 7, 4, 4, 9, 7, 5 },
};
'''
    if old not in s:
        raise SystemExit('archetype block not found')
    s = s.replace(old, new, 1)

if '// COOP_ARCHETYPE_STARTER_KITS_V1' not in s:
    include_anchor = '#include "party_member.h"\n'
    if '#include "platform_compat.h"\n' not in s:
        if include_anchor not in s:
            raise SystemExit('platform compat include anchor not found')
        s = s.replace(include_anchor, include_anchor + '#include "platform_compat.h"\n', 1)

    kit_anchor = '''};

inline bool localCoopApplyPlayerOneArchetype(int archetype, int gender)
'''
    kit_block = '''};

// COOP_ARCHETYPE_STARTER_KITS_V1
// P1 keeps the campaign's normal opening equipment. Each newly-created P2-P4
// contributes one modest kit to the one shared inventory, once, when its slot
// becomes permanently locked. Reconnect/load paths see saved.locked and never
// grant the kit again.
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
            debugPrint("[COOP KIT] slot=%d archetype=%s item-missing=%s\\n",
                slot, kLocalCoopArchetypeNames[archetype], entry.itemName);
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
        debugPrint("[COOP KIT] slot=%d archetype=%s added=%s x%d\\n",
            slot, kLocalCoopArchetypeNames[archetype], entry.itemName, entry.quantity);
    }
}

inline bool localCoopApplyPlayerOneArchetype(int archetype, int gender)
'''
    if kit_anchor not in s:
        raise SystemExit('starter kit insertion anchor not found')
    s = s.replace(kit_anchor, kit_block, 1)

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
        raise SystemExit('create actor starter flag anchor not found')
    s = s.replace(create_anchor, create_new, 1)

    duplicate_saved = '''    LocalCoopCharacterSlotState& saved =
        localCoopCharacterStateGet().slots[slot];
    saved.locked = 1;
'''
    if duplicate_saved not in s:
        raise SystemExit('saved slot declaration anchor not found')
    s = s.replace(duplicate_saved, '''    saved.locked = 1;
''', 1)

    grant_anchor = '''    snprintf(
        saved.controllerGuid,
        sizeof(saved.controllerGuid),
        "%s",
        player.controllerGuid);

    debugPrint(
'''
    grant_new = '''    snprintf(
        saved.controllerGuid,
        sizeof(saved.controllerGuid),
        "%s",
        player.controllerGuid);

    if (grantStarterKit) {
        localCoopGrantStarterKit(slot, archetype);
    }

    debugPrint(
'''
    if grant_anchor not in s:
        raise SystemExit('starter grant anchor not found')
    s = s.replace(grant_anchor, grant_new, 1)

if '// COOP_18_ARCHETYPES_MENU_V1' not in s:
    old_draw = '''    windowDrawText(player.joinWindow, choice, 380, 20, 92, _colorTable[992]);

    const int* stats = kLocalCoopArchetypeStats[player.archetype];
'''
    new_draw = '''    // COOP_18_ARCHETYPES_MENU_V1
    windowDrawText(player.joinWindow, choice, 380, 20, 82, _colorTable[992]);
    windowDrawText(player.joinWindow, kLocalCoopArchetypeRoles[player.archetype], 380, 20, 106, _colorTable[992]);

    const int* stats = kLocalCoopArchetypeStats[player.archetype];
'''
    if old_draw not in s:
        raise SystemExit('join menu role anchor not found')
    s = s.replace(old_draw, new_draw, 1)
    s = s.replace('''        126,\n        _colorTable[992]);''', '''        136,\n        _colorTable[992]);''', 1)
    s = s.replace('''        154,\n        _colorTable[992]);''', '''        164,\n        _colorTable[992]);''', 1)
    s = s.replace('''        194,\n        _colorTable[992]);''', '''        204,\n        _colorTable[992]);''', 1)

p.write_text(s, encoding='utf-8')
print('Expanded local co-op to 18 archetypes with one-time shared starter kits')
