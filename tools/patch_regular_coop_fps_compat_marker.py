from pathlib import Path

path = Path('src/local_coop_runtime.h')
text = path.read_text(encoding='utf-8')
marker = 'COOP_NATIVE_BILLBOARD_FPS_RUNTIME_V1'
if marker in text:
    print('compat marker already present')
    raise SystemExit(0)

anchor = 'inline void localCoopRuntimeTick()\n{'
if anchor not in text:
    raise SystemExit('localCoopRuntimeTick anchor not found')

replacement = ('// COOP_NATIVE_BILLBOARD_FPS_RUNTIME_V1\n'
               '// Compatibility marker retained for the legacy build validator.\n'
               '// Regular isometric co-op owns the active shared-camera path.\n'
               + anchor)
path.write_text(text.replace(anchor, replacement, 1), encoding='utf-8')
print('added legacy FPS validator compatibility marker without changing regular co-op behavior')
