from pathlib import Path

main = Path('src/main.cc')
mapcc = Path('src/map.cc')
main_text = main.read_text(encoding='utf-8')
map_text = mapcc.read_text(encoding='utf-8')
marker = '// COOP_PREOPENING_STAGING_ROOM_HOOK_V1'
if marker in main_text and marker in map_text:
    print('Pre-opening staging room already applied')
    raise SystemExit(0)

# main.cc: load a safe existing Fallout 1 Vault 13 entrance map first, without
# playing the opening sequence. The real requested start map is remembered and
# loaded only after the party exits the staging room.
if '#include "local_coop_staging_room.h"\n' not in main_text:
    anchor = '#include "loadsave.h"\n'
    if anchor not in main_text:
        raise SystemExit('main include anchor not found')
    main_text = main_text.replace(anchor, anchor + '#include "local_coop_staging_room.h"\n', 1)

old = '''                if (characterSelectorOpen() == 2) {\n                    gameMoviePlay(MOVIE_ELDER, GAME_MOVIE_STOP_MUSIC);\n                    randomSeedPrerandom(-1);\n\n                    // SFALL: Override starting map.\n                    char* mapName = nullptr;\n                    if (configGetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_STARTING_MAP_KEY, &mapName)) {\n                        if (*mapName == '\\0') {\n                            mapName = nullptr;\n                        }\n                    }\n\n                    char* mapNameCopy = compat_strdup(mapName != nullptr ? mapName : _mainMap);\n                    _main_load_new(mapNameCopy);\n                    free(mapNameCopy);'''
new = '''                if (characterSelectorOpen() == 2) {\n                    randomSeedPrerandom(-1);\n\n                    // SFALL: Override starting map.\n                    char* mapName = nullptr;\n                    if (configGetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_STARTING_MAP_KEY, &mapName)) {\n                        if (*mapName == '\\0') {\n                            mapName = nullptr;\n                        }\n                    }\n\n                    char* mapNameCopy = compat_strdup(mapName != nullptr ? mapName : _mainMap);\n\n                    // COOP_PREOPENING_STAGING_ROOM_HOOK_V1\n                    // Give the party a quiet preparation map before any opening\n                    // movie/story scene. V13ENT is an original Fallout 1 asset\n                    // already present in the mounted data and works as a small\n                    // contained staging space. Walking out consumes staging,\n                    // plays the normal opening sequence once, then loads the real\n                    // requested campaign start map. Fallout 2 keeps its stock\n                    // Elder/Temple opening path.\n                    bool useCoopStaging = unifiedCampaignGetActiveGame() == UnifiedGameId::Fallout1;\n                    if (useCoopStaging) {\n                        localCoopBeginStagingRoom(mapNameCopy);\n                        char stagingMap[] = "V13ENT.MAP";\n                        _main_load_new(stagingMap);\n                    } else {\n                        gameMoviePlay(MOVIE_ELDER, GAME_MOVIE_STOP_MUSIC);\n                        _main_load_new(mapNameCopy);\n                    }\n                    free(mapNameCopy);'''
if marker not in main_text:
    if old not in main_text:
        raise SystemExit('new game opening anchor not found')
    main_text = main_text.replace(old, new, 1)

if '#include "local_coop_staging_room.h"\n' not in map_text:
    anchor = '#include "local_coop_ai_realtime.h"\n'
    if anchor not in map_text:
        raise SystemExit('map include anchor not found')
    map_text = map_text.replace(anchor, anchor + '#include "local_coop_staging_room.h"\n', 1)

old_map = '''int mapHandleTransition()\n{\n    if (gMapTransition.map == 0) {\n        return 0;\n    }\n\n    gameMouseObjectsHide();'''
new_map = '''int mapHandleTransition()\n{\n    if (gMapTransition.map == 0) {\n        return 0;\n    }\n\n    // COOP_PREOPENING_STAGING_ROOM_HOOK_V1\n    // The first outward transition from the preparation room is the party's\n    // explicit READY action. Do not enter the world graph yet: show the normal\n    // opening movie once and move into the real campaign start map instead.\n    if (localCoopConsumeStagingRoomExit()) {\n        animationStop();\n        memset(&gMapTransition, 0, sizeof(gMapTransition));\n        gameMoviePlay(MOVIE_ELDER, GAME_MOVIE_STOP_MUSIC);\n        if (gLocalCoopStagingDestinationMap[0] != '\\0') {\n            char destination[COMPAT_MAX_PATH];\n            std::strncpy(destination, gLocalCoopStagingDestinationMap, sizeof(destination) - 1);\n            destination[sizeof(destination) - 1] = '\\0';\n            debugPrint("[COOP STAGING] ready; loading campaign start %s\\n", destination);\n            return mapLoadByName(destination);\n        }\n        return 0;\n    }\n\n    gameMouseObjectsHide();'''
if marker not in map_text:
    if old_map not in map_text:
        raise SystemExit('mapHandleTransition anchor not found')
    map_text = map_text.replace(old_map, new_map, 1)

main.write_text(main_text, encoding='utf-8')
mapcc.write_text(map_text, encoding='utf-8')
print('Applied pre-opening co-op staging room')
