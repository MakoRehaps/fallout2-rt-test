from pathlib import Path

p = Path('src/local_coop_mobile.cc')
s = p.read_text(encoding='utf-8')
marker = '// PHOBOI_MOBILE_RELIABILITY_V3'
if marker in s:
    print('PhoBoi mobile reliability hardening already applied')
    raise SystemExit(0)

# Track overlapping reconnect sockets so an older connection closing cannot
# neutralize input owned by a newer connection.
old_fields = '''    std::atomic<int> jitterMs { 0 };\n    std::atomic<int> rttMs { 0 };\n\n    MobileSlotState()\n'''
new_fields = '''    std::atomic<int> jitterMs { 0 };\n    std::atomic<int> rttMs { 0 };\n    // PHOBOI_MOBILE_RELIABILITY_V3\n    std::atomic<int> controlConnections { 0 };\n    std::atomic<int> streamConnections { 0 };\n\n    MobileSlotState()\n'''
if old_fields not in s:
    raise SystemExit('MobileSlotState field anchor not found')
s = s.replace(old_fields, new_fields, 1)

# Replace the browser networking block with independent control/video recovery,
# session resume, connection-state UI, and a throttled HTTP input fallback.
start = s.find('let ws=null,reconnectTimer=null;')
end = s.find('async function inflateFrame(buf){', start)
if start == -1 or end == -1:
    raise SystemExit('PhoBoi browser socket block not found')
new_socket_block = r'''let ws=null,controlTimer=null,controlAttempt=0,controlMode='STARTING';
let streamTimer=null,streamAttempt=0,videoMode='STARTING',lastFrameAt=0,lastHttpSent=0;
function updateStatus(){
 const link=rtt>0?` ${rtt}ms ±${jitter}`:'';
 $('top').textContent=`PLAYER ${slot+1} | CTRL ${controlMode} | VIDEO ${videoMode}${link}`;
}
function retryDelay(attempt){return Math.min(5000,500*Math.pow(1.55,Math.min(attempt,6)))}
function scheduleControl(){if(slot<0||controlTimer)return;controlTimer=setTimeout(()=>{controlTimer=null;openSocket()},retryDelay(controlAttempt++))}
function scheduleStream(){if(slot<0||streamTimer)return;streamTimer=setTimeout(()=>{streamTimer=null;openStream()},retryDelay(streamAttempt++))}
function openSocket(){
 if(slot<0)return;
 if(ws&&(ws.readyState===WebSocket.OPEN||ws.readyState===WebSocket.CONNECTING))return;
 const scheme=location.protocol==='https:'?'wss':'ws';
 controlMode='CONNECTING';updateStatus();
 const sock=new WebSocket(`${scheme}://${location.host}/ws?slot=${slot}&token=${token}`);ws=sock;
 sock.onopen=()=>{if(ws!==sock)return;controlAttempt=0;controlMode='WS';updateStatus();navigator.vibrate?.(35)};
 sock.onmessage=ev=>{
  if(ws!==sock)return;
  const m=String(ev.data||'').split(',');if(m[0]!=='a'||m.length<3)return;
  const sent=Number(m[2]);if(!Number.isFinite(sent))return;
  const sample=Math.max(0,performance.now()-sent),delta=Math.abs(sample-lastRtt);lastRtt=sample;
  rtt=rtt?Math.round(rtt*.82+sample*.18):Math.round(sample);jitter=jitter?Math.round(jitter*.8+delta*.2):Math.round(delta);
  updateStatus();
 };
 sock.onerror=()=>{};
 sock.onclose=()=>{if(ws===sock)ws=null;controlMode='HTTP';updateStatus();scheduleControl()};
}
'''
s = s[:start] + new_socket_block + s[end:]

old_inflate_stream = r'''async function inflateFrame(buf){
 const u=new Uint8Array(buf);if(u.length<12)return;const dv=new DataView(buf);const w=dv.getUint16(4,true),h=dv.getUint16(6,true);const payload=buf.slice(12);
 const ds=new DecompressionStream('deflate');const raw=await new Response(new Blob([payload]).stream().pipeThrough(ds)).arrayBuffer();
 const rgba=new Uint8ClampedArray(raw);if(rgba.length!==w*h*4)return;const c=$('video');if(c.width!==w||c.height!==h){c.width=w;c.height=h}c.getContext('2d').putImageData(new ImageData(rgba,w,h),0,0);
}
function openStream(){
 if(slot<0||!('DecompressionStream'in window))return;const scheme=location.protocol==='https:'?'wss':'ws';stream=new WebSocket(`${scheme}://${location.host}/stream?slot=${slot}&token=${token}`);stream.binaryType='arraybuffer';
 stream.onmessage=e=>inflateFrame(e.data).catch(()=>{});stream.onclose=()=>{if(slot>=0)setTimeout(openStream,1200)};
}
'''
new_inflate_stream = r'''async function inflateFrame(buf){
 const u=new Uint8Array(buf);if(u.length<12||u[0]!==80||u[1]!==72||u[2]!==79||u[3]!==66)return;
 const dv=new DataView(buf);const w=dv.getUint16(4,true),h=dv.getUint16(6,true);if(!w||!h||w>1920||h>1080)return;
 const payload=buf.slice(12);const ds=new DecompressionStream('deflate');
 const raw=await new Response(new Blob([payload]).stream().pipeThrough(ds)).arrayBuffer();
 const rgba=new Uint8ClampedArray(raw);if(rgba.length!==w*h*4)return;
 const c=$('video');if(c.width!==w||c.height!==h){c.width=w;c.height=h}
 c.getContext('2d',{alpha:false}).putImageData(new ImageData(rgba,w,h),0,0);lastFrameAt=performance.now();videoMode='OK';updateStatus();
}
function openStream(){
 if(slot<0)return;
 if(!('DecompressionStream'in window)){videoMode='UNSUPPORTED';updateStatus();return}
 if(stream&&(stream.readyState===WebSocket.OPEN||stream.readyState===WebSocket.CONNECTING))return;
 const scheme=location.protocol==='https:'?'wss':'ws';videoMode='CONNECTING';updateStatus();
 const sock=new WebSocket(`${scheme}://${location.host}/stream?slot=${slot}&token=${token}`);stream=sock;sock.binaryType='arraybuffer';
 sock.onopen=()=>{if(stream!==sock)return;streamAttempt=0;lastFrameAt=performance.now();videoMode='WAIT';updateStatus()};
 sock.onmessage=e=>{if(stream===sock)inflateFrame(e.data).catch(()=>{videoMode='DECODE';updateStatus()})};
 sock.onerror=()=>{};
 sock.onclose=()=>{if(stream===sock)stream=null;videoMode='RETRY';updateStatus();scheduleStream()};
}
'''
if old_inflate_stream not in s:
    raise SystemExit('inflate/openStream block not found')
s = s.replace(old_inflate_stream, new_inflate_stream, 1)

old_send = r'''async function send(){
 if(slot<0)return;
 const stamp=performance.now();
 const packet=[++seq,stamp,Math.round(rtt),Math.round(jitter),axes[0],axes[1],axes[2],axes[3],axes[4],axes[5],buttons].join(',');
 if(ws&&ws.readyState===WebSocket.OPEN){ws.send(packet);return}
 if(sending)return;sending=true;
 try{const body=new URLSearchParams({slot,token,lx:axes[0],ly:axes[1],rx:axes[2],ry:axes[3],lt:axes[4],rt:axes[5],buttons});await fetch('/input',{method:'POST',body,keepalive:true})}catch(e){}
 sending=false;
}
setInterval(send,16);// PHOBOI_NO_PAGEHIDE_RELEASE_V1
// Do not release the claimed player slot on pagehide: browsers may fire pagehide
// during fullscreen/orientation transitions. The server timeout owns stale sessions.
'''
new_send = r'''async function send(){
 if(slot<0)return;
 const stamp=performance.now();
 const packet=[++seq,stamp,Math.round(rtt),Math.round(jitter),axes[0],axes[1],axes[2],axes[3],axes[4],axes[5],buttons].join(',');
 if(ws&&ws.readyState===WebSocket.OPEN){try{ws.send(packet);controlMode='WS'}catch(e){}return}
 // HTTP fallback stays responsive without flooding Cloudflare with ~60 POST/s.
 if(stamp-lastHttpSent<50||sending)return;lastHttpSent=stamp;sending=true;controlMode='HTTP';updateStatus();
 try{
  const body=new URLSearchParams({slot,token,lx:axes[0],ly:axes[1],rx:axes[2],ry:axes[3],lt:axes[4],rt:axes[5],buttons});
  const r=await fetch('/input',{method:'POST',body,cache:'no-store'});
  if(r.status===403){controlMode='EXPIRED';updateStatus()}
 }catch(e){}
 sending=false;
}
setInterval(send,16);
setInterval(()=>{
 if(slot<0)return;
 if(!ws||ws.readyState===WebSocket.CLOSED)openSocket();
 if(!stream||stream.readyState===WebSocket.CLOSED)openStream();
 if(stream&&stream.readyState===WebSocket.OPEN&&lastFrameAt&&performance.now()-lastFrameAt>5000){
  videoMode='STALLED';updateStatus();try{stream.close()}catch(e){}
 }
},1000);
['online','pageshow','orientationchange'].forEach(name=>window.addEventListener(name,()=>{if(slot>=0){openSocket();openStream()}}));
document.addEventListener('visibilitychange',()=>{if(!document.hidden&&slot>=0){openSocket();openStream()}});
// PHOBOI_NO_PAGEHIDE_RELEASE_V1: fullscreen/orientation/page lifecycle never releases the slot.
'''
if old_send not in s:
    raise SystemExit('send/fallback block not found')
s = s.replace(old_send, new_send, 1)

# Claim starts both channels independently and persists a resumable session.
old_claim = "slot=j.slotIndex;token=j.token;$('join').style.display='none';$('pad').style.display='block';$('top').textContent=`PLAYER ${j.player} — CONNECTED`;openSocket();"
new_claim = "slot=j.slotIndex;token=j.token;sessionStorage.setItem('phoboiSession',JSON.stringify({slot,token}));$('join').style.display='none';$('pad').style.display='block';controlMode='STARTING';videoMode='STARTING';updateStatus();openSocket();openStream();"
if old_claim not in s:
    raise SystemExit('claim success line not found')
s = s.replace(old_claim, new_claim, 1)

# Add auto-resume after reload before the script closes.
resume_anchor = "// PHOBOI_NO_PAGEHIDE_RELEASE_V1: fullscreen/orientation/page lifecycle never releases the slot.\n"
resume_code = r'''// Reloading the controller page should not strand a claimed slot for 60 seconds.
(async()=>{try{
 const saved=JSON.parse(sessionStorage.getItem('phoboiSession')||'null');if(!saved)return;
 const r=await fetch(`/resume?slot=${saved.slot}&token=${saved.token}`,{cache:'no-store'});const j=await r.json();if(!j.ok)return;
 slot=saved.slot;token=saved.token;$('join').style.display='none';$('pad').style.display='block';controlMode='STARTING';videoMode='STARTING';updateStatus();openSocket();openStream();
}catch(e){}})();
'''
s = s.replace(resume_anchor, resume_anchor + resume_code, 1)

# Server resume endpoint.
route_anchor = '''    MobileSlotState& state = gMobileSlots[slot];\n\n    if (method == "GET" && route == "/ws") {\n'''
route_new = '''    MobileSlotState& state = gMobileSlots[slot];\n\n    if (method == "GET" && route == "/resume") {\n        uint32_t token = mobileUnsignedValue(values, "token", 0);\n        bool ok = state.claimed.load() && token != 0 && token == state.token.load();\n        if (ok) {\n            state.lastSeen.store(mobileNow());\n            std::ostringstream body;\n            body << "{\\\"ok\\\":true,\\\"player\\\":" << slot + 1 << "}";\n            mobileSendResponse(client, "200 OK", "application/json", body.str());\n        } else {\n            mobileSendResponse(client, "403 Forbidden", "application/json", "{\\\"ok\\\":false,\\\"error\\\":\\\"Session expired\\\"}");\n        }\n        return;\n    }\n\n    if (method == "GET" && route == "/ws") {\n'''
if route_anchor not in s:
    raise SystemExit('server route anchor not found')
s = s.replace(route_anchor, route_new, 1)

# Connection accounting around control socket. Keep input when a stale/old socket
# closes while fallback or a replacement socket is active.
loop_anchor = '''    MobileSlotState& state = gMobileSlots[slot];\n    while (gMobileServerRunning.load()\n'''
loop_new = '''    MobileSlotState& state = gMobileSlots[slot];\n    state.controlConnections.fetch_add(1);\n    while (gMobileServerRunning.load()\n'''
if loop_anchor not in s:
    raise SystemExit('control connection loop anchor not found')
s = s.replace(loop_anchor, loop_new, 1)
old_reset = '''    mobileResetInput(state);\n}\n\nvoid mobileRunStreamWebSocket'''
new_reset = '''    int remaining = state.controlConnections.fetch_sub(1) - 1;\n    uint64_t age = mobileNow() - state.lastSeen.load();\n    if (remaining <= 0 && age > 250) {\n        mobileResetInput(state);\n    }\n    debugPrint("[PHOBOI WS] control closed slot=%d remaining=%d ageMs=%llu\\n",\n        slot + 1, remaining, static_cast<unsigned long long>(age));\n}\n\nvoid mobileRunStreamWebSocket'''
if old_reset not in s:
    raise SystemExit('control reset anchor not found')
s = s.replace(old_reset, new_reset, 1)

# Stream connection accounting/logging.
stream_loop = '''    uint32_t sentSequence = 0;\n    while (gMobileServerRunning.load() && gMobileSlots[slot].claimed.load() && gMobileSlots[slot].token.load() == token) {\n'''
stream_loop_new = '''    MobileSlotState& streamState = gMobileSlots[slot];\n    streamState.streamConnections.fetch_add(1);\n    uint32_t sentSequence = 0;\n    while (gMobileServerRunning.load() && streamState.claimed.load() && streamState.token.load() == token) {\n'''
if stream_loop not in s:
    raise SystemExit('stream loop anchor not found')
s = s.replace(stream_loop, stream_loop_new, 1)
stream_end = '''        sentSequence = available;\n    }\n}\n\nvoid mobileCaptureStreamFrame'''
stream_end_new = '''        sentSequence = available;\n    }\n    int remainingStreams = streamState.streamConnections.fetch_sub(1) - 1;\n    debugPrint("[PHOBOI WS] stream closed slot=%d remaining=%d\\n", slot + 1, remainingStreams);\n}\n\nvoid mobileCaptureStreamFrame'''
if stream_end not in s:
    raise SystemExit('stream end anchor not found')
s = s.replace(stream_end, stream_end_new, 1)

p.write_text(s, encoding='utf-8')
print('Applied PhoBoi mobile reliability V3')
