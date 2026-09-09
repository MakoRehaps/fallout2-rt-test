#!/usr/bin/env python3
from pathlib import Path

p = Path('src/local_coop_mobile.cc')
s = p.read_text(encoding='utf-8')
MARKER = '// PHOBOI_RAW_VIDEO_FALLBACK_V1'
if MARKER in s:
    print('PhoBoi raw video fallback already applied')
    raise SystemExit(0)

old = '''std::mutex gMobileStreamMutex;
std::vector<uint8_t> gMobileStreamFrame;
std::atomic<uint32_t> gMobileStreamSequence { 0 };'''
new = '''std::mutex gMobileStreamMutex;
std::vector<uint8_t> gMobileStreamFrame;
std::vector<uint8_t> gMobileStreamRawFrame;
std::atomic<uint32_t> gMobileStreamSequence { 0 };
''' + MARKER
if old not in s:
    raise SystemExit('mobile stream globals anchor not found')
s = s.replace(old, new, 1)

old = '''void mobileRunStreamWebSocket(MobileSocket socket, const std::string& request, int slot, uint32_t token)
{
    std::string key = mobileHeaderValue(request, "Sec-WebSocket-Key");'''
new = '''void mobileRunStreamWebSocket(MobileSocket socket, const std::string& request, int slot, uint32_t token)
{
    // Safari/iOS versions without DecompressionStream request raw RGBA frames.
    // The raw mode is lower resolution/FPS already because capture adapts to link quality,
    // but unlike the old path it always gives a connected phone a usable picture.
    const bool rawStream = request.find("raw=1") != std::string::npos;
    std::string key = mobileHeaderValue(request, "Sec-WebSocket-Key");'''
if old not in s:
    raise SystemExit('stream websocket function anchor not found')
s = s.replace(old, new, 1)

old = '''        std::vector<uint8_t> frame;
        { std::lock_guard<std::mutex> lock(gMobileStreamMutex); frame = gMobileStreamFrame; }
        if (!frame.empty() && !mobileSendWebSocketBinary(socket, frame)) break;'''
new = '''        std::vector<uint8_t> frame;
        {
            std::lock_guard<std::mutex> lock(gMobileStreamMutex);
            frame = rawStream ? gMobileStreamRawFrame : gMobileStreamFrame;
        }
        if (!frame.empty() && !mobileSendWebSocketBinary(socket, frame)) break;'''
if old not in s:
    raise SystemExit('stream frame selection anchor not found')
s = s.replace(old, new, 1)

old = '''    uLongf compressedSize = compressBound(static_cast<uLong>(raw.size()));
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
    gMobileStreamSequence.store(sequence);'''
new = '''    uint32_t sequence = gMobileStreamSequence.load() + 1;

    // PHOB = zlib-compressed RGBA. PHOR = raw RGBA fallback.
    std::vector<uint8_t> rawFrame(12 + raw.size());
    rawFrame[0] = 'P'; rawFrame[1] = 'H'; rawFrame[2] = 'O'; rawFrame[3] = 'R';
    rawFrame[4] = width & 0xFF; rawFrame[5] = (width >> 8) & 0xFF;
    rawFrame[6] = height & 0xFF; rawFrame[7] = (height >> 8) & 0xFF;
    rawFrame[8] = sequence & 0xFF; rawFrame[9] = (sequence >> 8) & 0xFF;
    rawFrame[10] = (sequence >> 16) & 0xFF; rawFrame[11] = (sequence >> 24) & 0xFF;
    std::copy(raw.begin(), raw.end(), rawFrame.begin() + 12);

    uLongf compressedSize = compressBound(static_cast<uLong>(raw.size()));
    std::vector<uint8_t> frame(12 + compressedSize);
    frame[0] = 'P'; frame[1] = 'H'; frame[2] = 'O'; frame[3] = 'B';
    frame[4] = width & 0xFF; frame[5] = (width >> 8) & 0xFF;
    frame[6] = height & 0xFF; frame[7] = (height >> 8) & 0xFF;
    frame[8] = sequence & 0xFF; frame[9] = (sequence >> 8) & 0xFF;
    frame[10] = (sequence >> 16) & 0xFF; frame[11] = (sequence >> 24) & 0xFF;
    if (compress2(frame.data() + 12, &compressedSize, raw.data(), static_cast<uLong>(raw.size()), Z_BEST_SPEED) != Z_OK) return;
    frame.resize(12 + compressedSize);
    {
        std::lock_guard<std::mutex> lock(gMobileStreamMutex);
        gMobileStreamFrame.swap(frame);
        gMobileStreamRawFrame.swap(rawFrame);
    }
    gMobileStreamSequence.store(sequence);'''
if old not in s:
    raise SystemExit('capture compression anchor not found')
s = s.replace(old, new, 1)

old = ''' const u=new Uint8Array(buf);if(u.length<12||u[0]!==80||u[1]!==72||u[2]!==79||u[3]!==66)return;
 const dv=new DataView(buf);const w=dv.getUint16(4,true),h=dv.getUint16(6,true);if(!w||!h||w>1920||h>1080)return;
 const payload=buf.slice(12);const ds=new DecompressionStream('deflate');
 const raw=await new Response(new Blob([payload]).stream().pipeThrough(ds)).arrayBuffer();
 const rgba=new Uint8ClampedArray(raw);if(rgba.length!==w*h*4)return;'''
new = ''' const u=new Uint8Array(buf);if(u.length<12||u[0]!==80||u[1]!==72||u[2]!==79||(u[3]!==66&&u[3]!==82))return;
 const dv=new DataView(buf);const w=dv.getUint16(4,true),h=dv.getUint16(6,true);if(!w||!h||w>1920||h>1080)return;
 const payload=buf.slice(12);let raw;
 if(u[3]===82){raw=payload}else{if(!('DecompressionStream'in window))return;const ds=new DecompressionStream('deflate');raw=await new Response(new Blob([payload]).stream().pipeThrough(ds)).arrayBuffer()}
 const rgba=new Uint8ClampedArray(raw);if(rgba.length!==w*h*4)return;'''
if old not in s:
    raise SystemExit('browser inflate anchor not found')
s = s.replace(old, new, 1)

old = ''' if(slot<0)return;
 if(!('DecompressionStream'in window)){videoMode='UNSUPPORTED';updateStatus();return}
 if(stream&&(stream.readyState===WebSocket.OPEN||stream.readyState===WebSocket.CONNECTING))return;
 const scheme=location.protocol==='https:'?'wss':'ws';videoMode='CONNECTING';updateStatus();
 const sock=new WebSocket(`${scheme}://${location.host}/stream?slot=${slot}&token=${token}`);stream=sock;sock.binaryType='arraybuffer';'''
new = ''' if(slot<0)return;
 if(stream&&(stream.readyState===WebSocket.OPEN||stream.readyState===WebSocket.CONNECTING))return;
 const scheme=location.protocol==='https:'?'wss':'ws';const rawMode=!('DecompressionStream'in window);videoMode=rawMode?'RAW CONNECTING':'CONNECTING';updateStatus();
 const sock=new WebSocket(`${scheme}://${location.host}/stream?slot=${slot}&token=${token}&raw=${rawMode?1:0}`);stream=sock;sock.binaryType='arraybuffer';'''
if old not in s:
    raise SystemExit('browser openStream anchor not found')
s = s.replace(old, new, 1)

p.write_text(s, encoding='utf-8')
print('Patched PhoBoi raw video fallback V1')
