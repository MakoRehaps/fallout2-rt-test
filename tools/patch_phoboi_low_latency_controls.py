from pathlib import Path

p = Path('src/local_coop_mobile.cc')
s = p.read_text(encoding='utf-8')
marker = '// PHOBOI_LOW_LATENCY_CONTROLS_V1'
if marker in s:
    print('PhoBoi low-latency controls already applied')
    raise SystemExit(0)

# 1) Make touch/button state changes transmit immediately instead of waiting
# for the next periodic tick.
s = s.replace(
" const e=$(id); const down=ev=>{ev.preventDefault();e.setPointerCapture?.(ev.pointerId);if(axis!==undefined)axes[axis]=32767;else buttons|=(1<<bit)};\n const up=ev=>{ev.preventDefault();if(axis!==undefined)axes[axis]=-32768;else buttons&=~(1<<bit)};",
" const e=$(id); const down=ev=>{ev.preventDefault();e.setPointerCapture?.(ev.pointerId);if(axis!==undefined)axes[axis]=32767;else buttons|=(1<<bit);send(true)};\n const up=ev=>{ev.preventDefault();if(axis!==undefined)axes[axis]=-32768;else buttons&=~(1<<bit);send(true)};"
)
s = s.replace(
"  axes[ax]=Math.round(x*32767);axes[ay]=Math.round(y*32767);n.style.transform=`translate(${x*r.width*.28}px,${y*r.height*.28}px)`;",
"  axes[ax]=Math.round(x*32767);axes[ay]=Math.round(y*32767);n.style.transform=`translate(${x*r.width*.28}px,${y*r.height*.28}px)`;send(true);"
)
s = s.replace(
" e.addEventListener('pointermove',move);function end(ev){if(active!==ev.pointerId)return;active=null;axes[ax]=axes[ay]=0;n.style.transform=''}",
" e.addEventListener('pointermove',move);function end(ev){if(active!==ev.pointerId)return;active=null;axes[ax]=axes[ay]=0;n.style.transform='';send(true)}"
)

# 2) Decode at most one video frame at a time. If another arrives while the
# phone is busy, keep only the newest frame. This prevents video decompression
# from blocking pointer events for hundreds of milliseconds or seconds.
s = s.replace(
"let streamTimer=null,streamAttempt=0,videoMode='STARTING',lastFrameAt=0,lastHttpSent=0;",
"let streamTimer=null,streamAttempt=0,videoMode='STARTING',lastFrameAt=0,lastHttpSent=0,decodeBusy=false,pendingFrame=null;\n// PHOBOI_LOW_LATENCY_CONTROLS_V1"
)
s = s.replace(
" sock.onmessage=e=>{if(stream===sock)inflateFrame(e.data).catch(()=>{videoMode='DECODE';updateStatus()})};",
" sock.onmessage=e=>{\n  if(stream!==sock)return;\n  if(decodeBusy){pendingFrame=e.data;return}\n  const decode=async data=>{decodeBusy=true;try{await inflateFrame(data)}catch(_){videoMode='DECODE';updateStatus()}finally{decodeBusy=false;if(pendingFrame){const latest=pendingFrame;pendingFrame=null;decode(latest)}}};\n  decode(e.data);\n };"
)

# 3) WebSocket input is latest-state data, not a historical event stream. Do
# not queue stale packets behind a congested tunnel. Immediate touch sends plus
# the 16 ms heartbeat will deliver the newest state as soon as the socket drains.
s = s.replace(
"async function send(){\n if(slot<0)return;",
"async function send(immediate=false){\n if(slot<0)return;"
)
s = s.replace(
" if(ws&&ws.readyState===WebSocket.OPEN){try{ws.send(packet);controlMode='WS'}catch(e){}return}",
" if(ws&&ws.readyState===WebSocket.OPEN){\n  if(ws.bufferedAmount>1024)return;\n  try{ws.send(packet);controlMode='WS'}catch(e){}return\n }"
)

# 4) The old top tier (960x540@24 using zlib RGBA) can saturate a remote uplink
# and monopolize a phone CPU. Cap quality to a control-friendly envelope and
# degrade quickly when RTT/jitter rises.
old_quality = """    int width = 480;\n    int height = 270;\n    int fps = 12;\n    if (worstRtt <= 45 && worstJitter <= 12 && worstInterval <= 24) {\n        width = 960; height = 540; fps = 24;\n    } else if (worstRtt <= 80 && worstJitter <= 20 && worstInterval <= 28) {\n        width = 960; height = 540; fps = 24;\n    } else if (worstRtt <= 140 && worstJitter <= 35 && worstInterval <= 40) {\n        width = 640; height = 360; fps = 20;\n    } else if (worstRtt <= 220 && worstJitter <= 60 && worstInterval <= 65) {\n        width = 480; height = 270; fps = 12;\n    } else if (worstRtt <= 350 && worstJitter <= 100 && worstInterval <= 110) {\n        width = 400; height = 225; fps = 8;\n    } else {\n        width = 320; height = 180; fps = 5;\n    }"""
new_quality = """    int width = 320;\n    int height = 180;\n    int fps = 8;\n    // Controls always win over picture quality. zlib RGBA is intentionally\n    // capped well below the old 960x540@24 mode to avoid tunnel bufferbloat.\n    if (worstRtt <= 55 && worstJitter <= 15 && worstInterval <= 26) {\n        width = 640; height = 360; fps = 15;\n    } else if (worstRtt <= 110 && worstJitter <= 30 && worstInterval <= 40) {\n        width = 480; height = 270; fps = 12;\n    } else if (worstRtt <= 180 && worstJitter <= 55 && worstInterval <= 70) {\n        width = 400; height = 225; fps = 10;\n    } else if (worstRtt <= 300 && worstJitter <= 100 && worstInterval <= 120) {\n        width = 320; height = 180; fps = 6;\n    } else {\n        width = 256; height = 144; fps = 4;\n    }"""
if old_quality not in s:
    raise SystemExit('expected adaptive video block not found')
s = s.replace(old_quality, new_quality)

p.write_text(s, encoding='utf-8')
print('Applied PhoBoi low-latency controls-first patch')
