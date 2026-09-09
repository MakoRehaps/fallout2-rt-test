from pathlib import Path

path = Path('src/game_sound.cc')
text = path.read_text(encoding='utf-8')
marker = '// COOP_NO_VOICE_ACTING_V1'
if marker in text:
    print('No-voice-acting policy already applied')
    raise SystemExit(0)

old = '''void speechSetVolume(int volume)\n{\n    if (!gGameSoundInitialized) {\n        return;\n    }\n\n    if (volume < VOLUME_MIN || volume > VOLUME_MAX) {'''
new = '''void speechSetVolume(int volume)\n{\n    if (!gGameSoundInitialized) {\n        return;\n    }\n\n    // COOP_NO_VOICE_ACTING_V1\n    // Keep the original speech streams/lipsync machinery alive for dialogue\n    // timing and talking-head compatibility, but the co-op presentation is\n    // text + synthetic non-verbal chatter only. Never make recorded speech\n    // audible even if an old preferences file requests a non-zero value.\n    volume = 0;\n\n    if (volume < VOLUME_MIN || volume > VOLUME_MAX) {'''
if old not in text:
    raise SystemExit('speech volume function anchor not found')
text = text.replace(old, new, 1)
path.write_text(text, encoding='utf-8')
print('Muted recorded voice acting while preserving speech timing')
