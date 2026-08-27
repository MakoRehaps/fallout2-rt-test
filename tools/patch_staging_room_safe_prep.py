from pathlib import Path

path = Path('src/main.cc')
text = path.read_text(encoding='utf-8')
marker = '// COOP_PREOPENING_STAGING_SAFE_HOOK_V1'
if marker in text:
    print('Safe staging hook already applied')
    raise SystemExit(0)

old = '''                        char stagingMap[] = "V13ENT.MAP";\n                        _main_load_new(stagingMap);'''
new = '''                        char stagingMap[] = "V13ENT.MAP";\n                        _main_load_new(stagingMap);\n                        // COOP_PREOPENING_STAGING_SAFE_HOOK_V1\n                        localCoopSanitizeStagingRoom();'''
if old not in text:
    raise SystemExit('staging map load anchor not found')
text = text.replace(old, new, 1)
path.write_text(text, encoding='utf-8')
print('Sanitized pre-opening co-op staging room')
