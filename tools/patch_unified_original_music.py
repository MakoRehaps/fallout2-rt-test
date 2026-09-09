from pathlib import Path

path = Path('src/game_sound.cc')
text = path.read_text(encoding='utf-8')
marker = '// UNIFIED_ORIGINAL_MUSIC_FROM_VFS_V1'
if marker in text:
    print('Original unified music VFS fix already applied')
    raise SystemExit(0)

old = '''bool _gsound_file_exists_f(const char* fname)\n{\n    FILE* f = compat_fopen(fname, "rb");\n    if (f == nullptr) {\n        return false;\n    }\n\n    fclose(f);\n\n    return true;\n}'''
new = '''bool _gsound_file_exists_f(const char* fname)\n{\n    // UNIFIED_ORIGINAL_MUSIC_FROM_VFS_V1\n    // Music is part of the original Fallout data set and may live inside the\n    // mounted master.dat rather than as a loose sound\\music file. The stock\n    // existence probe used stdio directly, bypassing the unified xfile/db search\n    // chain, so backgroundSoundLoad rejected valid ACM tracks before soundLoad\n    // ever had a chance to open them. Use the same virtual filesystem lookup as\n    // speech/SFX so both Fallout 1 and Fallout 2 original music remain usable.\n    int fileSize = 0;\n    bool exists = dbGetFileSize(fname, &fileSize) == 0 && fileSize > 0;\n    if (!exists && gGameSoundDebugEnabled) {\n        debugPrint("[UNIFIED MUSIC] missing through VFS: %s\\n", fname);\n    }\n    return exists;\n}'''
if old not in text:
    raise SystemExit('music existence probe anchor not found')
text = text.replace(old, new, 1)
path.write_text(text, encoding='utf-8')
print('Restored original Fallout music lookup through unified VFS')
