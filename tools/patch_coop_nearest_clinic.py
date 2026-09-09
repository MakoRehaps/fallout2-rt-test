from pathlib import Path

MARKER = '// COOP_NEAREST_CLINIC_V1'

# Expose read-only Fallout 2 town coordinates so medical rescue can choose the
# geographically closest town instead of using a fixed destination.
p = Path('src/worldmap.h')
s = p.read_text(encoding='utf-8')
if 'int wmGetAreaWorldPos(int areaIdx, int* xPtr, int* yPtr);' not in s:
    anchor = 'int wmGetPartyWorldPos(int* xPtr, int* yPtr);\n'
    if anchor not in s:
        raise SystemExit('worldmap.h party world-pos declaration not found')
    s = s.replace(anchor, anchor + 'int wmGetAreaWorldPos(int areaIdx, int* xPtr, int* yPtr);\n', 1)
    p.write_text(s, encoding='utf-8')

p = Path('src/worldmap.cc')
s = p.read_text(encoding='utf-8')
if MARKER not in s:
    anchor = '''int wmGetPartyWorldPos(int* xPtr, int* yPtr)\n{\n    if (xPtr != nullptr) {\n        *xPtr = wmGenData.worldPosX;\n    }\n\n    if (yPtr != nullptr) {\n        *yPtr = wmGenData.worldPosY;\n    }\n\n    return 0;\n}\n'''
    if anchor not in s:
        raise SystemExit('worldmap.cc wmGetPartyWorldPos implementation not found')
    addition = anchor + '''\n// COOP_NEAREST_CLINIC_V1\nint wmGetAreaWorldPos(int areaIdx, int* xPtr, int* yPtr)\n{\n    if (areaIdx < 0 || areaIdx >= wmMaxAreaNum || wmAreaInfoList == nullptr) {\n        return -1;\n    }\n\n    const CityInfo& city = wmAreaInfoList[areaIdx];\n    if (xPtr != nullptr) {\n        *xPtr = city.x;\n    }\n    if (yPtr != nullptr) {\n        *yPtr = city.y;\n    }\n    return 0;\n}\n'''
    s = s.replace(anchor, addition, 1)
    p.write_text(s, encoding='utf-8')

# Replace the simple world-map evacuation with a nearest real medical town map.
p = Path('src/local_coop_runtime.h')
s = p.read_text(encoding='utf-8')
if MARKER not in s:
    if '#include "worldmap.h"\n' not in s:
        anchor = '#include "unified_campaign_transition.h"\n'
        if anchor not in s:
            raise SystemExit('runtime include anchor not found')
        s = s.replace(anchor, anchor + '#include "unified_worldmap_state_profile.h"\n#include "worldmap.h"\n', 1)

    evac_anchor = '''inline void localCoopMedicalEvacuate(LocalCoopPlayer& player, const char* reason)\n{\n'''
    if evac_anchor not in s:
        raise SystemExit('runtime medical evacuation function not found')

    helpers = r'''// COOP_NEAREST_CLINIC_V1
struct LocalCoopMedicalDestination {
    int area = -1;
    int map = -1;
    const char* f1MapName = nullptr;
    int f1LoadIndex = 0;
    int f1Section = 0;
    const char* label = "clinic";
};

inline long long localCoopMedicalDistanceSquared(int ax, int ay, int bx, int by)
{
    long long dx = static_cast<long long>(ax) - bx;
    long long dy = static_cast<long long>(ay) - by;
    return dx * dx + dy * dy;
}

inline LocalCoopMedicalDestination localCoopNearestFallout1Clinic()
{
    // Towns/maps containing an established doctor, clinic, Brotherhood medical
    // facility, or Followers medical care in the original Fallout 1 world.
    static constexpr LocalCoopMedicalDestination clinics[] = {
        { 2, -1, "SHADYW.MAP", 1, 0, "Shady Sands - Razlo" },
        { 3, -1, "JUNKENT.MAP", 3, 0, "Junktown - Doc Morbid" },
        { 6, -1, "HUBDWNTN.MAP", 3, 1, "The Hub medical district" },
        { 7, -1, "BROHD34.MAP", 4, 3, "Brotherhood medical bay" },
        { 10, -1, "LAFOLLWR.MAP", 5, 2, "Followers medical clinic" },
    };

    const UnifiedFallout1WorldMapState& state = unifiedFallout1WorldMapGetStateConst();
    LocalCoopMedicalDestination best = clinics[0];
    long long bestDistance = 0x7FFFFFFFFFFFFFFFLL;
    for (const auto& clinic : clinics) {
        int x = unifiedFallout1TownWorldX(clinic.area);
        int y = unifiedFallout1TownWorldY(clinic.area);
        long long distance = localCoopMedicalDistanceSquared(state.worldX, state.worldY, x, y);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = clinic;
        }
    }
    return best;
}

inline LocalCoopMedicalDestination localCoopNearestFallout2Clinic()
{
    // Maps chosen are the safe town maps containing, or immediately serving,
    // their settlement's medical provider. This keeps rescue out of encounter
    // maps while preserving each town's normal scripts/NPCs.
    static constexpr LocalCoopMedicalDestination clinics[] = {
        { CITY_DEN, MAP_DEN_RESIDENTIAL, nullptr, 0, 0, "Den clinic" },
        { CITY_KLAMATH, MAP_KLAMATH_1, nullptr, 0, 0, "Klamath medical care" },
        { CITY_MODOC, MAP_MODOC_MAINSTREET, nullptr, 0, 0, "Modoc medical care" },
        { CITY_VAULT_CITY, MAP_VAULTCITY_DOWNTOWN, nullptr, 0, 0, "Vault City clinic" },
        { CITY_GECKO, MAP_GECKO_SETTLEMENT, nullptr, 0, 0, "Gecko medical care" },
        { CITY_BROKEN_HILLS, MAP_BROKEN_HILLS1, nullptr, 0, 0, "Broken Hills clinic" },
        { CITY_NEW_RENO, MAP_NEW_RENO_1, nullptr, 0, 0, "New Reno medical care" },
        { CITY_NEW_CALIFORNIA_REPUBLIC, MAP_NCR_DOWNTOWN, nullptr, 0, 0, "NCR medical care" },
        { CITY_REDDING, MAP_REDDING_DOWNTOWN, nullptr, 0, 0, "Redding clinic" },
        { CITY_SAN_FRANCISCO, MAP_SAN_FRAN_CHINATOWN, nullptr, 0, 0, "San Francisco medical care" },
    };

    int partyX = 0;
    int partyY = 0;
    wmGetPartyWorldPos(&partyX, &partyY);

    LocalCoopMedicalDestination best = clinics[0];
    long long bestDistance = 0x7FFFFFFFFFFFFFFFLL;
    for (const auto& clinic : clinics) {
        int x = 0;
        int y = 0;
        if (wmGetAreaWorldPos(clinic.area, &x, &y) != 0) {
            continue;
        }
        long long distance = localCoopMedicalDistanceSquared(partyX, partyY, x, y);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = clinic;
        }
    }
    return best;
}

inline bool localCoopTransferPartyToClinic(const LocalCoopMedicalDestination& clinic)
{
    if (unifiedCampaignGetActiveGame() == UnifiedGameId::Fallout1) {
        if (!unifiedFallout1TownIndexIsValid(clinic.area) || clinic.f1MapName == nullptr) {
            return false;
        }

        UnifiedFallout1WorldMapState& state = unifiedFallout1WorldMapGetState();
        state.worldX = unifiedFallout1TownWorldX(clinic.area);
        state.worldY = unifiedFallout1TownWorldY(clinic.area);
        state.currentTown = clinic.area;
        state.currentSection = clinic.f1Section;
        unifiedFallout1MarkTownKnown(clinic.area, true);
        if (gGameGlobalVars != nullptr
            && kUnifiedFallout1LoadMapIndexGvar >= 0
            && kUnifiedFallout1LoadMapIndexGvar < gGameGlobalVarsLength) {
            gGameGlobalVars[kUnifiedFallout1LoadMapIndexGvar] = clinic.f1LoadIndex;
        }

        char mapName[32];
        std::snprintf(mapName, sizeof(mapName), "%s", clinic.f1MapName);
        return mapLoadByName(mapName) == 0;
    }

    if (clinic.area < 0 || clinic.map < 0) {
        return false;
    }
    wmAreaSetVisibleState(clinic.area, CITY_STATE_KNOWN, true);
    wmAreaMarkVisited(clinic.area);
    if (wmTeleportToArea(clinic.area) != 0) {
        return false;
    }
    return mapLoadById(clinic.map) == 0;
}

'''
    s = s.replace(evac_anchor, helpers + evac_anchor, 1)

    old = '''    // The party shares one loaded map, so a medical rescue evacuates the whole\n    // co-op group out of the lethal encounter. The world/town layer can then\n    // place the party at its safe medical destination without splitting players\n    // across maps.\n    localCoopDangerEnd();\n    localCoopRealtimeAiReset();\n    scriptsRequestWorldMap();\n\n    if (actor == gDude && gInterfaceBarWindow != -1) {\n'''
    new = '''    // The party shares one loaded map, so a rescue moves the complete local\n    // co-op party to the geographically nearest safe medical settlement.\n    localCoopDangerEnd();\n    localCoopRealtimeAiReset();\n\n    LocalCoopMedicalDestination clinic = unifiedCampaignGetActiveGame() == UnifiedGameId::Fallout1\n        ? localCoopNearestFallout1Clinic()\n        : localCoopNearestFallout2Clinic();\n    bool transferred = localCoopTransferPartyToClinic(clinic);\n    if (!transferred) {\n        // Defensive fallback: never leave a rescued player in a lethal map just\n        // because an optional clinic map was unavailable in the installed data.\n        scriptsRequestWorldMap();\n    }\n    debugPrint("[COOP MEDICAL] destination=%s area=%d map=%d transferred=%d\\n",\n        clinic.label,\n        clinic.area,\n        clinic.map,\n        transferred ? 1 : 0);\n\n    if (actor == gDude && gInterfaceBarWindow != -1) {\n'''
    if old not in s:
        raise SystemExit('runtime old world-map evacuation block not found')
    s = s.replace(old, new, 1)
    p.write_text(s, encoding='utf-8')

print('Applied nearest clinic medical evacuation')
