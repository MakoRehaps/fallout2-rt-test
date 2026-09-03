from pathlib import Path

path = Path('src/game_dialog.cc')
text = path.read_text(encoding='utf-8')
marker = '// COOP_DOS_DIALOG_CHATTER_HOOK_V1'
if marker in text:
    print('DOS dialogue chatter hook already applied')
    raise SystemExit(0)

inc = '#include "local_coop_dialog_chatter.h"\n'
anchor = '#include "lips.h"\n'
if inc not in text:
    if anchor not in text:
        raise SystemExit('dialog include anchor not found')
    text = text.replace(anchor, anchor + inc, 1)

old_msg = '''    entry->optionMessageListId = -3;\n    entry->optionMessageId = -3;\n\n    gGameDialogReviewEntriesLength++;\n\n    return 0;\n}\n\n// 0x4460B4'''
new_msg = '''    entry->optionMessageListId = -3;\n    entry->optionMessageId = -3;\n\n    // COOP_DOS_DIALOG_CHATTER_HOOK_V1\n    // Non-verbal, quiet synthetic chatter only; the original Fallout dialog\n    // art/text remains authoritative and no spoken voice line is generated.\n    localCoopDialogChatter(gGameDialogSpeaker, 42 + (messageId & 31));\n\n    gGameDialogReviewEntriesLength++;\n\n    return 0;\n}\n\n// 0x4460B4'''
if old_msg not in text:
    raise SystemExit('review message anchor not found')
text = text.replace(old_msg, new_msg, 1)

old_text = '''    entry->optionMessageListId = -3;\n    entry->optionMessageId = -3;\n    entry->optionText = nullptr;\n\n    gGameDialogReviewEntriesLength++;'''
new_text = '''    entry->optionMessageListId = -3;\n    entry->optionMessageId = -3;\n    entry->optionText = nullptr;\n\n    localCoopDialogChatter(gGameDialogSpeaker, static_cast<int>(strlen(string)));\n\n    gGameDialogReviewEntriesLength++;'''
if old_text not in text:
    raise SystemExit('review text anchor not found')
text = text.replace(old_text, new_text, 1)

path.write_text(text, encoding='utf-8')
print('Wired quiet DOS chatter to NPC and creature dialogue replies')
