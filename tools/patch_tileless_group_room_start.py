from pathlib import Path

path = Path('src/main.cc')
text = path.read_text(encoding='utf-8')
marker = '// COOP_TILELESS_GROUP_ROOM_START_V1'
if marker in text:
    print('Tileless group-room start already applied')
    raise SystemExit(0)

if '#include "local_coop_group_room.h"\n' not in text:
    anchor = '#include "local_coop_staging_room.h"\n'
    if anchor not in text:
        anchor = '#include "loadsave.h"\n'
    if anchor not in text:
        raise SystemExit('main include anchor not found')
    text = text.replace(anchor, anchor + '#include "local_coop_group_room.h"\n', 1)

old = '''                    // COOP_PREOPENING_STAGING_ROOM_HOOK_V1\n                    // Give the party a quiet preparation map before any opening\n                    // movie/story scene. V13ENT is an original Fallout 1 asset\n                    // already present in the mounted data and works as a small\n                    // contained staging space. Walking out consumes staging,\n                    // plays the normal opening sequence once, then loads the real\n                    // requested campaign start map. Fallout 2 keeps its stock\n                    // Elder/Temple opening path.\n                    bool useCoopStaging = unifiedCampaignGetActiveGame() == UnifiedGameId::Fallout1;\n                    if (useCoopStaging) {\n                        localCoopBeginStagingRoom(mapNameCopy);\n                        char stagingMap[] = "V13ENT.MAP";\n                        // COOP_STAGING_EPHEMERAL_SAVE_V1\n                        // Never inherit a prior temporary staging save. The prep\n                        // room is a disposable view of an original Fallout map.\n                        _MapDirEraseFile_("MAPS\\\\", "V13ENT.SAV");\n                        _main_load_new(stagingMap);\n                        // COOP_PREOPENING_STAGING_SAFE_HOOK_V1\n                        localCoopSanitizeStagingRoom();\n                    } else {\n                        gameMoviePlay(MOVIE_ELDER, GAME_MOVIE_STOP_MUSIC);\n                        _main_load_new(mapNameCopy);\n                    }'''

new = '''                    // COOP_TILELESS_GROUP_ROOM_START_V1\n                    // Unified co-op always begins in Fallout 1. The grouping\n                    // scene is a pure black UI room: no map, no tiles, no critters\n                    // and no V13ENT reuse. Start joins a controller; after release,\n                    // Start again votes READY. Only after all joined players are\n                    // ready do we enter the real Fallout 1 opening/start flow.\n                    if (unifiedCampaignIsEnabled()) {\n                        unifiedCampaignSetActiveGame(UnifiedGameId::Fallout1);\n                        gUnifiedCampaignRuntime.requestedContentGame = UnifiedGameId::Fallout1;\n                        gUnifiedCampaignRuntime.loadedSaveRequiresContentReload = false;\n\n                        if (!localCoopRunTilelessGroupRoom()) {\n                            free(mapNameCopy);\n                            mainMenuWindowInit();\n                            break;\n                        }\n\n                        // Do NOT play MOVIE_ELDER here. That is Fallout 2's elder\n                        // movie and was the cause of the new-game path looking like\n                        // Fallout 2. Load the actual Fallout 1 campaign start; its\n                        // normal Fallout 1 intro/Overseer scripting remains in\n                        // charge from this point onward.\n                        _main_load_new(mapNameCopy);\n                    } else {\n                        gameMoviePlay(MOVIE_ELDER, GAME_MOVIE_STOP_MUSIC);\n                        _main_load_new(mapNameCopy);\n                    }'''

if old not in text:
    raise SystemExit('old pre-opening staging block not found')
text = text.replace(old, new, 1)
path.write_text(text, encoding='utf-8')
print('Applied tileless group-room unified start')
