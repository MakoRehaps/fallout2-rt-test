from pathlib import Path

p = Path('src/main.cc')
s = p.read_text(encoding='utf-8')
marker = '// COOP_RUNTIME_MAINLOOP_HOOK_V1'
if marker in s:
    print('co-op runtime main loop hook already applied')
    raise SystemExit(0)

include_anchor = '#include "local_coop_group_room.h"\n'
if include_anchor not in s:
    raise SystemExit('main.cc local_coop_group_room include anchor not found')
s = s.replace(include_anchor, include_anchor + '#include "local_coop_runtime.h"\n', 1)

loop_anchor = '''        int keyCode = inputGetInput();\n\n        // SFALL: MainLoopHook.\n'''
loop_repl = '''        int keyCode = inputGetInput();\n\n        // COOP_RUNTIME_MAINLOOP_HOOK_V1\n        // Drive the co-op runtime directly from the live Fallout gameplay loop.\n        // The ticker remains useful inside stock modal loops, but it cannot be\n        // responsible for bootstrapping itself. This guarantees controller,\n        // keyboard, phone, HUD, AI and FPS/ISO camera processing every frame.\n        localCoopRuntimeTick();\n\n        // SFALL: MainLoopHook.\n'''
if loop_anchor not in s:
    raise SystemExit('main.cc gameplay loop anchor not found')
s = s.replace(loop_anchor, loop_repl, 1)

p.write_text(s, encoding='utf-8')
print('Applied direct co-op runtime main loop hook')
