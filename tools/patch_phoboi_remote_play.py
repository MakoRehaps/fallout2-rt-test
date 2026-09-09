#!/usr/bin/env python3
from pathlib import Path

p = Path('src/local_coop_mobile.cc')
s = p.read_text(encoding='utf-8')

MARKER = '// PHOBOI_ADAPTIVE_REMOTE_PLAY_V2'
if MARKER in s:
    print('PhoBoi remote play already patched')
    raise SystemExit(0)

if '#include <zlib.h>' not in s:
    s = s.replace('#include <vector>\n', '#include <vector>\n\n#include <zlib.h>\n')
if '#include "svga.h"' not in s:
    s = s.replace('#include "local_coop.h"\n', '#include "local_coop.h"\n#include "svga.h"\n')

s = s.replace(
    'constexpr uint64_t kMobileTimeoutMs = 60000;\n',
    '''constexpr uint64_t kMobileTimeoutMs = 60000;\nconstexpr uint64_t kMobileInputNeutralMs = 750;\nconstexpr uint64_t kMobileBadLinkNeutralMs = 1400;\nconstexpr int kMobileNominalPacketMs = 16;\n''' + MARKER + '\n')

old = '''    std::array<std::atomic<int>, SDL_CONTROLLER_AXIS_MAX> axes;\n    std::atomic<uint32_t> buttons { 0 };\n\n    MobileSlotState()\n    {\n        for (auto& axis : axes) {\n            axis.store(0);\n        }\n        axes[SDL_CONTROLLER_AXIS_TRIGGERLEFT].store(SDL_JOYSTICK_AXIS_MIN);\n        axes[SDL_CONTROLLER_AXIS_TRIGGERRIGHT].store(SDL_JOYSTICK_AXIS_MIN);\n    }'''
new = '''    std::array<std::atomic<int>, SDL_CONTROLLER_AXIS_MAX> axes;\n    std::array<int, SDL_CONTROLLER_AXIS_MAX> smoothedAxes {};\n    std::atomic<uint32_t> buttons { 0 };\n    std::atomic<uint32_t> packetSequence { 0 };\n    std::atomic<uint64_t> previousArrival { 0 };\n    std::atomic<int> packetIntervalMs { kMobileNominalPacketMs };\n    std::atomic<int> jitterMs { 0 };\n    std::atomic<int> rttMs { 0 };\n\n    MobileSlotState()\n    {\n        for (auto& axis : axes) {\n            axis.store(0);\n        }\n        for (int axis = 0; axis < SDL_CONTROLLER_AXIS_MAX; axis++) {\n            smoothedAxes[axis] =\n                axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT || axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT\n                    ? SDL_JOYSTICK_AXIS_MIN\n                    : 0;\n        }\n        axes[SDL_CONTROLLER_AXIS_TRIGGERLEFT].store(SDL_JOYSTICK_AXIS_MIN);\n        axes[SDL_CONTROLLER_AXIS_TRIGGERRIGHT].store(SDL_JOYSTICK_AXIS_MIN);\n    }'''
if old not in s:
    raise SystemExit('MobileSlotState anchor not found')
s = s.replace(old, new)

anchor = 'bool gMobileNoticeShown = false;\n'
if anchor not in s:
    raise SystemExit('mobile globals anchor not found')
s = s.replace(anchor, '''bool gMobileNoticeShown = false;\nstd::mutex gMobileStreamMutex;\nstd::vector<uint8_t> gMobileStreamFrame;\nstd::atomic<uint32_t> gMobileStreamSequence { 0 };\nstd::atomic<uint64_t> gMobileLastStreamCapture { 0 };\n''')

s = s.replace(
    '#pad{display:none;position:fixed;inset:0;background:radial-gradient(circle at center,#183321,#07100b)}',
    '#pad{display:none;position:fixed;inset:0;background:radial-gradient(circle at center,#183321,#07100b)}#video{position:absolute;left:18vw;top:3vh;width:64vw;height:55vh;object-fit:contain;background:#000;border:1px solid #295f37;image-rendering:auto}')
s = s.replace(
    '<div id="pad"><div class="top" id="top">PHOBOI CONTROLLER</div>',
    '<div id="pad"><canvas id="video" width="640" height="360"></canvas><div class="top" id="top">PHOBOI CONTROLLER</div>')
s = s.replace(
    'let slot=-1,token=0,buttons=0,axes=[0,0,0,0,-32768,-32768],sending=false;',
    'let slot=-1,token=0,buttons=0,axes=[0,0,0,0,-32768,-32768],sending=false,seq=0,rtt=0,jitter=0,lastRtt=0,stream=null;')

old = ''' ws.onopen=()=>{$('top').textContent=`PLAYER ${slot+1} — LOW LATENCY`;navigator.vibrate?.(35)};\n ws.onclose=()=>{$('top').textContent='RECONNECTING…';if(slot>=0)reconnectTimer=setTimeout(openSocket,700)};\n}'''
new = ''' ws.onopen=()=>{$('top').textContent=`PLAYER ${slot+1} — CONNECTED`;navigator.vibrate?.(35);openStream()};\n ws.onmessage=ev=>{\n  const m=String(ev.data||'').split(',');if(m[0]!=='a'||m.length<3)return;\n  const sent=Number(m[2]);if(!Number.isFinite(sent))return;\n  const sample=Math.max(0,performance.now()-sent),delta=Math.abs(sample-lastRtt);lastRtt=sample;\n  rtt=rtt?Math.round(rtt*.82+sample*.18):Math.round(sample);jitter=jitter?Math.round(jitter*.8+delta*.2):Math.round(delta);\n  const grade=rtt<55&&jitter<15?'EXCELLENT':rtt<100&&jitter<30?'GOOD':rtt<220&&jitter<70?'OK':'ROUGH';\n  $('top').textContent=`PLAYER ${slot+1} — ${grade} ${rtt}ms ±${jitter}`;\n };\n ws.onclose=()=>{$('top').textContent='RECONNECTING…';if(slot>=0)reconnectTimer=setTimeout(openSocket,700)};\n}\nasync function inflateFrame(buf){\n const u=new Uint8Array(buf);if(u.length<12)return;const dv=new DataView(buf);const w=dv.getUint16(4,true),h=dv.getUint16(6,true);const payload=buf.slice(12);\n const ds=new DecompressionStream('deflate');const raw=await new Response(new Blob([payload]).stream().pipeThrough(ds)).arrayBuffer();\n const rgba=new Uint8ClampedArray(raw);if(rgba.length!==w*h*4)return;const c=$('video');if(c.width!==w||c.height!==h){c.width=w;c.height=h}c.getContext('2d').putImageData(new ImageData(rgba,w,h),0,0);\n}\nfunction openStream(){\n if(slot<0||!('DecompressionStream'in window))return;const scheme=location.protocol==='https:'?'wss':'ws';stream=new WebSocket(`${scheme}://${location.host}/stream?slot=${slot}&token=${token}`);stream.binaryType='arraybuffer';\n stream.onmessage=e=>inflateFrame(e.data).catch(()=>{});stream.onclose=()=>{if(slot>=0)setTimeout(openStream,1200)};\n}'''
if old not in s:
    raise SystemExit('browser websocket anchor not found')
s = s.replace(old, new)

old = ''' const packet=[axes[0],axes[1],axes[2],axes[3],axes[4],axes[5],buttons].join(',');\n if(ws&&ws.readyState===WebSocket.OPEN){ws.send(packet);return}'''
new = ''' const stamp=performance.now();\n const packet=[++seq,stamp,Math.round(rtt),Math.round(jitter),axes[0],axes[1],axes[2],axes[3],axes[4],axes[5],buttons].join(',');\n if(ws&&ws.readyState===WebSocket.OPEN){ws.send(packet);return}'''
if old not in s:
    raise SystemExit('browser packet anchor not found')
s = s.replace(old, new)

marker = 'bool mobileReadWebSocketText(MobileSocket socket, std::string& text)\n{'
helper = r'''bool mobileSendWebSocketText(MobileSocket socket, const std::string& text)
{
    if (text.size() > 125) return false;
    uint8_t header[2] = { 0x81, static_cast<uint8_t>(text.size()) };
#ifdef _WIN32
    if (send(socket, reinterpret_cast<const char*>(header), 2, 0) != 2) return false;
    return send(socket, text.c_str(), static_cast<int>(text.size()), 0) == static_cast<int>(text.size());
#else
    if (send(socket, header, 2, 0) != 2) return false;
    return send(socket, text.c_str(), text.size(), 0) == static_cast<ssize_t>(text.size());
#endif
}

bool mobileSendWebSocketBinary(MobileSocket socket, const std::vector<uint8_t>& bytes)
{
    if (bytes.empty() || bytes.size() > 2097152) return false;
    uint8_t header[10] = { 0x82, 0 };
    size_t headerLength = 0;
    if (bytes.size() < 126) {
        header[1] = static_cast<uint8_t>(bytes.size()); headerLength = 2;
    } else if (bytes.size() <= 65535) {
        header[1] = 126; header[2] = static_cast<uint8_t>((bytes.size() >> 8) & 0xFF); header[3] = static_cast<uint8_t>(bytes.size() & 0xFF); headerLength = 4;
    } else {
        header[1] = 127; uint64_t n = bytes.size(); for (int i = 0; i < 8; i++) header[2 + i] = static_cast<uint8_t>((n >> ((7 - i) * 8)) & 0xFF); headerLength = 10;
    }
#ifdef _WIN32
    if (send(socket, reinterpret_cast<const char*>(header), static_cast<int>(headerLength), 0) != static_cast<int>(headerLength)) return false;
    size_t offset = 0; while (offset < bytes.size()) { int sent = send(socket, reinterpret_cast<const char*>(bytes.data() + offset), static_cast<int>(bytes.size() - offset), 0); if (sent <= 0) return false; offset += static_cast<size_t>(sent); } return true;
#else
    if (send(socket, header, headerLength, 0) != static_cast<ssize_t>(headerLength)) return false;
    size_t offset = 0; while (offset < bytes.size()) { ssize_t sent = send(socket, bytes.data() + offset, bytes.size() - offset, 0); if (sent <= 0) return false; offset += static_cast<size_t>(sent); } return true;
#endif
}

void mobileRecordArrival(MobileSlotState& state, uint64_t now)
{
    uint64_t previous = state.previousArrival.exchange(now);
    if (previous == 0 || now <= previous) return;
    int sample = static_cast<int>(std::min<uint64_t>(1000, now - previous));
    int oldInterval = state.packetIntervalMs.load();
    int interval = (oldInterval * 7 + sample) / 8;
    state.packetIntervalMs.store(std::max(1, interval));
    int deviation = std::abs(sample - interval);
    int oldJitter = state.jitterMs.load();
    state.jitterMs.store((oldJitter * 7 + deviation) / 8);
}

'''
if marker not in s:
    raise SystemExit('websocket helper anchor not found')
s = s.replace(marker, helper + marker)

old = '''        int lx;\n        int ly;\n        int rx;\n        int ry;\n        int lt;\n        int rt;\n        unsigned int buttons;\n        if (sscanf(\n                message.c_str(),\n                "%d,%d,%d,%d,%d,%d,%u",\n                &lx,\n                &ly,\n                &rx,\n                &ry,\n                &lt,\n                &rt,\n                &buttons)\n            == 7) {\n            state.axes[SDL_CONTROLLER_AXIS_LEFTX].store(lx);'''
new = '''        unsigned int sequence = 0;\n        double clientStamp = 0.0;\n        int clientRtt = 0;\n        int clientJitter = 0;\n        int lx;\n        int ly;\n        int rx;\n        int ry;\n        int lt;\n        int rt;\n        unsigned int buttons;\n        if (sscanf(\n                message.c_str(),\n                "%u,%lf,%d,%d,%d,%d,%d,%d,%d,%d,%u",\n                &sequence,\n                &clientStamp,\n                &clientRtt,\n                &clientJitter,\n                &lx,\n                &ly,\n                &rx,\n                &ry,\n                &lt,\n                &rt,\n                &buttons)\n            == 11) {\n            uint64_t arrival = mobileNow();\n            mobileRecordArrival(state, arrival);\n            state.packetSequence.store(sequence);\n            state.rttMs.store(std::max(0, std::min(clientRtt, 5000)));\n            state.jitterMs.store(std::max(state.jitterMs.load(), std::max(0, std::min(clientJitter, 2000))));\n            state.axes[SDL_CONTROLLER_AXIS_LEFTX].store(lx);'''
if old not in s:
    raise SystemExit('websocket parser anchor not found')
s = s.replace(old, new)

wspos = s.find('unsigned int sequence = 0;')
old = '            state.buttons.store(buttons);\n            state.lastSeen.store(mobileNow());\n        }'
new = '            state.buttons.store(buttons);\n            state.lastSeen.store(arrival);\n            std::ostringstream ack; ack << "a," << sequence << "," << clientStamp;\n            if (!mobileSendWebSocketText(socket, ack.str())) break;\n        }'
idx = s.find(old, wspos)
if idx < 0:
    raise SystemExit('websocket ack anchor not found')
s = s[:idx] + new + s[idx + len(old):]

stream_marker = 'void mobileResetInput(MobileSlotState& state)\n{'
stream_code = r'''void mobileRunStreamWebSocket(MobileSocket socket, const std::string& request, int slot, uint32_t token)
{
    std::string key = mobileHeaderValue(request, "Sec-WebSocket-Key");
    if (key.empty()) return;
    auto digest = mobileSha1(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    std::string accept = mobileBase64(digest.data(), digest.size());
    std::ostringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " << accept << "\r\n\r\n";
    std::string bytes = response.str();
#ifdef _WIN32
    send(socket, bytes.c_str(), static_cast<int>(bytes.size()), 0);
#else
    send(socket, bytes.c_str(), bytes.size(), 0);
#endif
    uint32_t sentSequence = 0;
    while (gMobileServerRunning.load() && gMobileSlots[slot].claimed.load() && gMobileSlots[slot].token.load() == token) {
        uint32_t available = gMobileStreamSequence.load();
        if (available == sentSequence) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        std::vector<uint8_t> frame;
        { std::lock_guard<std::mutex> lock(gMobileStreamMutex); frame = gMobileStreamFrame; }
        if (!frame.empty() && !mobileSendWebSocketBinary(socket, frame)) break;
        sentSequence = available;
    }
}

void mobileCaptureStreamFrame(uint64_t now)
{
    if (gSdlSurface == nullptr) return;
    bool any = false;
    int worstRtt = 0;
    int worstJitter = 0;
    int worstInterval = 0;
    for (int slot = 1; slot < kLocalCoopMaxPlayers; slot++) {
        if (!gMobileSlots[slot].claimed.load()) continue;
        any = true;
        worstRtt = std::max(worstRtt, gMobileSlots[slot].rttMs.load());
        worstJitter = std::max(worstJitter, gMobileSlots[slot].jitterMs.load());
        worstInterval = std::max(worstInterval, gMobileSlots[slot].packetIntervalMs.load());
    }
    if (!any) return;

    int width = 480;
    int height = 270;
    int fps = 12;
    if (worstRtt <= 45 && worstJitter <= 12 && worstInterval <= 24) {
        width = 1280; height = 720; fps = 30;
    } else if (worstRtt <= 80 && worstJitter <= 20 && worstInterval <= 28) {
        width = 960; height = 540; fps = 24;
    } else if (worstRtt <= 140 && worstJitter <= 35 && worstInterval <= 40) {
        width = 640; height = 360; fps = 20;
    } else if (worstRtt <= 220 && worstJitter <= 60 && worstInterval <= 65) {
        width = 480; height = 270; fps = 12;
    } else if (worstRtt <= 350 && worstJitter <= 100 && worstInterval <= 110) {
        width = 400; height = 225; fps = 8;
    } else {
        width = 320; height = 180; fps = 5;
    }

    uint64_t previous = gMobileLastStreamCapture.load();
    if (previous != 0 && now - previous < static_cast<uint64_t>(1000 / fps)) return;
    gMobileLastStreamCapture.store(now);

    SDL_Surface* scaled = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
    if (scaled == nullptr) return;
    SDL_Rect dst { 0, 0, width, height };
    if (SDL_BlitScaled(gSdlSurface, nullptr, scaled, &dst) != 0) {
        SDL_FreeSurface(scaled);
        return;
    }
    size_t rawSize = static_cast<size_t>(width) * height * 4;
    std::vector<uint8_t> raw(rawSize);
    uint8_t* src = static_cast<uint8_t*>(scaled->pixels);
    for (int y = 0; y < height; y++) {
        std::memcpy(raw.data() + static_cast<size_t>(y) * width * 4,
            src + static_cast<size_t>(y) * scaled->pitch,
            static_cast<size_t>(width) * 4);
    }
    SDL_FreeSurface(scaled);

    uLongf compressedSize = compressBound(static_cast<uLong>(raw.size()));
    std::vector<uint8_t> frame(12 + compressedSize);
    frame[0] = 'P'; frame[1] = 'H'; frame[2] = 'O'; frame[3] = 'B';
    frame[4] = width & 0xFF; frame[5] = (width >> 8) & 0xFF;
    frame[6] = height & 0xFF; frame[7] = (height >> 8) & 0xFF;
    uint32_t sequence = gMobileStreamSequence.load() + 1;
    frame[8] = sequence & 0xFF; frame[9] = (sequence >> 8) & 0xFF;
    frame[10] = (sequence >> 16) & 0xFF; frame[11] = (sequence >> 24) & 0xFF;
    if (compress2(frame.data() + 12, &compressedSize, raw.data(), static_cast<uLong>(raw.size()), Z_BEST_SPEED) != Z_OK) return;
    frame.resize(12 + compressedSize);
    { std::lock_guard<std::mutex> lock(gMobileStreamMutex); gMobileStreamFrame.swap(frame); }
    gMobileStreamSequence.store(sequence);
}

'''
if stream_marker not in s:
    raise SystemExit('stream insertion anchor not found')
s = s.replace(stream_marker, stream_code + stream_marker)

route_anchor = '''    if (method == "GET" && route == "/ws") {\n        uint32_t token = static_cast<uint32_t>(mobileValue(values, "token", 0));\n        if (!state.claimed.load() || token == 0 || token != state.token.load()) {\n            mobileSendResponse(client, "403 Forbidden", "text/plain", "Session expired");\n            return;\n        }\n        mobileRunWebSocket(client, request, slot, token);\n        return;\n    }\n'''
route_new = route_anchor + '''\n    if (method == "GET" && route == "/stream") {\n        uint32_t token = static_cast<uint32_t>(mobileValue(values, "token", 0));\n        if (!state.claimed.load() || token == 0 || token != state.token.load()) {\n            mobileSendResponse(client, "403 Forbidden", "text/plain", "Session expired");\n            return;\n        }\n        mobileRunStreamWebSocket(client, request, slot, token);\n        return;\n    }\n'''
if route_anchor not in s:
    raise SystemExit('stream route anchor not found')
s = s.replace(route_anchor, route_new)

old = '''    for (int axis = 0; axis < SDL_CONTROLLER_AXIS_MAX; axis++) {\n        int value = state.axes[axis].load();\n        value = std::max(static_cast<int>(SDL_JOYSTICK_AXIS_MIN), std::min(static_cast<int>(SDL_JOYSTICK_AXIS_MAX), value));\n        SDL_JoystickSetVirtualAxis(device.joystick, axis, static_cast<Sint16>(value));\n    }'''
new = '''    int rtt = state.rttMs.load();\n    int jitter = state.jitterMs.load();\n    int interval = state.packetIntervalMs.load();\n    int smoothingDivisor = (rtt > 300 || jitter > 90 || interval > 100) ? 4\n        : ((rtt > 160 || jitter > 45 || interval > 55) ? 3 : 2);\n    for (int axis = 0; axis < SDL_CONTROLLER_AXIS_MAX; axis++) {\n        int target = state.axes[axis].load();\n        target = std::max(static_cast<int>(SDL_JOYSTICK_AXIS_MIN), std::min(static_cast<int>(SDL_JOYSTICK_AXIS_MAX), target));\n        int current = state.smoothedAxes[axis];\n        int value = current + (target - current) / smoothingDivisor;\n        if (std::abs(target - current) < 512) value = target;\n        state.smoothedAxes[axis] = value;\n        SDL_JoystickSetVirtualAxis(device.joystick, axis, static_cast<Sint16>(value));\n    }'''
if old not in s:
    raise SystemExit('axis smoothing anchor not found')
s = s.replace(old, new)

old = '        if (state.claimed.load() && inputAge > 250) {'
new = '        uint64_t neutralAfter = (state.rttMs.load() > 300 || state.jitterMs.load() > 90 || state.packetIntervalMs.load() > 100) ? kMobileBadLinkNeutralMs : kMobileInputNeutralMs;\n        if (state.claimed.load() && inputAge > neutralAfter) {'
if old not in s:
    raise SystemExit('neutral timeout anchor not found')
s = s.replace(old, new)
s = s.replace(
    "// Keep the player's reservation for sixty seconds, but neutralize the\n            // virtual pad almost immediately.",
    "// Keep the player's reservation for sixty seconds. Neutralization is\n            // adaptive so high-latency links are not mistaken for disconnects.")

tick_anchor = '    SDL_JoystickUpdate();\n\n    if (gMobileHostWindow != -1'
if tick_anchor not in s:
    raise SystemExit('stream capture tick anchor not found')
s = s.replace(tick_anchor, '    SDL_JoystickUpdate();\n    mobileCaptureStreamFrame(now);\n\n    if (gMobileHostWindow != -1')

p.write_text(s, encoding='utf-8')
print('Patched PhoBoi adaptive remote play V2')
