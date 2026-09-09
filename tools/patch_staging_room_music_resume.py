from pathlib import Path

path = Path('src/map.cc')
text = path.read_text(encoding='utf-8')
marker = '// COOP_STAGING_MUSIC_RESUME_V1'
if marker in text:
    print('Staging music resume already applied')
    raise SystemExit(0)

old = '''            debugPrint("[COOP STAGING] ready; loading campaign start %s\\n", destination);\n            return mapLoadByName(destination);'''
new = '''            debugPrint("[COOP STAGING] ready; loading campaign start %s\\n", destination);\n            int stagingLoadRc = mapLoadByName(destination);\n            // COOP_STAGING_MUSIC_RESUME_V1\n            // mapLoadById normally restarts map music, but this named reload is\n            // intentional so the saved start-map filename survives staging.\n            // Restore the original game's map track after the opening movie.\n            if (stagingLoadRc == 0) {\n                wmMapMusicStart();\n            }\n            return stagingLoadRc;'''
if old not in text:
    raise SystemExit('staging destination load anchor not found')
text = text.replace(old, new, 1)
path.write_text(text, encoding='utf-8')
print('Restored map music after co-op staging transition')
