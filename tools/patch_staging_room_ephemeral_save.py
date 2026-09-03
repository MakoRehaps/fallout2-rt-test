from pathlib import Path

main = Path('src/main.cc')
mapcc = Path('src/map.cc')
main_text = main.read_text(encoding='utf-8')
map_text = mapcc.read_text(encoding='utf-8')
marker = '// COOP_STAGING_EPHEMERAL_SAVE_V1'
if marker in main_text and marker in map_text:
    print('Ephemeral staging save policy already applied')
    raise SystemExit(0)

old_main = '''                        char stagingMap[] = "V13ENT.MAP";\n                        _main_load_new(stagingMap);'''
new_main = '''                        char stagingMap[] = "V13ENT.MAP";\n                        // COOP_STAGING_EPHEMERAL_SAVE_V1\n                        // Never inherit a prior temporary staging save. The prep\n                        // room is a disposable view of an original Fallout map.\n                        _MapDirEraseFile_("MAPS\\\\", "V13ENT.SAV");\n                        _main_load_new(stagingMap);'''
if marker not in main_text:
    if old_main not in main_text:
        raise SystemExit('staging main load anchor not found')
    main_text = main_text.replace(old_main, new_main, 1)

# Insert cleanup after the staging room has been saved during the raw map reload.
old_map = '''            int stagingLoadRc = mapLoadByName(destination);\n            // COOP_STAGING_MUSIC_RESUME_V1'''
new_map = '''            int stagingLoadRc = mapLoadByName(destination);\n            // COOP_STAGING_EPHEMERAL_SAVE_V1\n            // mapLoad saves the map being left before reading the destination.\n            // That save contains the hidden staging critters, so discard it now.\n            _MapDirEraseFile_("MAPS\\\\", "V13ENT.SAV");\n            // COOP_STAGING_MUSIC_RESUME_V1'''
if marker not in map_text:
    if old_map not in map_text:
        raise SystemExit('staging post-load cleanup anchor not found; music resume patch must run first')
    map_text = map_text.replace(old_map, new_map, 1)

main.write_text(main_text, encoding='utf-8')
mapcc.write_text(map_text, encoding='utf-8')
print('Made pre-opening staging map ephemeral')
