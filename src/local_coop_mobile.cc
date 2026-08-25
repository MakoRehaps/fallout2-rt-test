#include "local_coop_mobile.h"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <zlib.h>

#include "qrcodegen.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "debug.h"
#include "display_monitor.h"
#include "local_coop.h"
#include "svga.h"
#include "kb.h"

namespace fallout {
namespace {

constexpr int kMobilePort = 27888;
constexpr uint64_t kMobileTimeoutMs = 60000;
constexpr uint64_t kMobileInputNeutralMs = 750;
constexpr uint64_t kMobileBadLinkNeutralMs = 1400;
constexpr int kMobileNominalPacketMs = 16;
// PHOBOI_ADAPTIVE_REMOTE_PLAY_V2

#ifdef _WIN32
using MobileSocket = SOCKET;
constexpr MobileSocket kInvalidMobileSocket = INVALID_SOCKET;
#else
using MobileSocket = int;
constexpr MobileSocket kInvalidMobileSocket = -1;
#endif

struct MobileSlotState {
    std::atomic<bool> available { false };
    std::atomic<bool> claimed { false };
    std::atomic<uint32_t> token { 0 };
    std::atomic<uint64_t> lastSeen { 0 };
    std::array<std::atomic<int>, SDL_CONTROLLER_AXIS_MAX> axes;
    std::array<int, SDL_CONTROLLER_AXIS_MAX> smoothedAxes {};
    std::atomic<uint32_t> buttons { 0 };
    std::atomic<uint32_t> packetSequence { 0 };
    std::atomic<uint64_t> previousArrival { 0 };
    std::atomic<int> packetIntervalMs { kMobileNominalPacketMs };
    std::atomic<int> jitterMs { 0 };
    std::atomic<int> rttMs { 0 };

    MobileSlotState()
    {
        for (auto& axis : axes) {
            axis.store(0);
        }
        for (int axis = 0; axis < SDL_CONTROLLER_AXIS_MAX; axis++) {
            smoothedAxes[axis] =
                axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT || axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT
                    ? SDL_JOYSTICK_AXIS_MIN
                    : 0;
        }
        axes[SDL_CONTROLLER_AXIS_TRIGGERLEFT].store(SDL_JOYSTICK_AXIS_MIN);
        axes[SDL_CONTROLLER_AXIS_TRIGGERRIGHT].store(SDL_JOYSTICK_AXIS_MIN);
    }
};

struct MobileVirtualDevice {
    int deviceIndex = -1;
    SDL_GameController* controller = nullptr;
    SDL_Joystick* joystick = nullptr;
};

std::array<MobileSlotState, kLocalCoopMaxPlayers> gMobileSlots;
std::array<MobileVirtualDevice, kLocalCoopMaxPlayers> gMobileDevices;
std::atomic<bool> gMobileServerRunning { false };
std::atomic<bool> gMobileServerStarted { false };
std::thread gMobileServerThread;
std::vector<std::thread> gMobileClientThreads;
MobileSocket gMobileListenSocket = kInvalidMobileSocket;
int gMobilePin = 0;
bool gMobileNoticeShown = false;
std::mutex gMobileStreamMutex;
std::vector<uint8_t> gMobileStreamFrame;
std::atomic<uint32_t> gMobileStreamSequence { 0 };
std::atomic<uint64_t> gMobileLastStreamCapture { 0 };

#ifdef _WIN32
HANDLE gCloudflareProcess = nullptr;
HANDLE gCloudflareOutput = nullptr;
std::thread gCloudflareOutputThread;
std::mutex gCloudflareStateMutex;
std::string gCloudflarePublicUrl;
std::string gCloudflareStatus = "OFF";
#endif

void mobileResetInput(MobileSlotState& state);

int gMobileHostWindow = -1;
uint64_t gMobileHostWindowLastDraw = 0;

uint64_t mobileNow()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
}

void mobileCloseSocket(MobileSocket socket)
{
#ifdef _WIN32
    closesocket(socket);
#else
    close(socket);
#endif
}

int mobileValue(const std::string& values, const char* key, int fallback)
{
    std::string prefix = std::string(key) + "=";
    size_t start = 0;
    while (start < values.size()) {
        size_t end = values.find('&', start);
        if (end == std::string::npos) {
            end = values.size();
        }

        if (values.compare(start, prefix.size(), prefix) == 0) {
            return std::atoi(values.substr(start + prefix.size(), end - start - prefix.size()).c_str());
        }

        start = end + 1;
    }

    return fallback;
}

uint32_t mobileNextToken()
{
    static std::atomic<uint32_t> seed { 0x50484F42u };
    uint32_t value = seed.fetch_add(0x9E3779B9u);
    value ^= static_cast<uint32_t>(mobileNow());
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    return value != 0 ? value : 1;
}

const char* mobileControllerHtml()
{
    return R"PHOBOI(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<meta name="apple-mobile-web-app-capable" content="yes">
<title>PhoBoi Mobile Controller</title>
<style>
*{box-sizing:border-box;touch-action:none}body{margin:0;background:#07100b;color:#9cff9c;font-family:monospace;overflow:hidden}
#join{position:fixed;inset:0;display:flex;align-items:center;justify-content:center;background:#07100b;z-index:5}
.panel{border:2px solid #4dbd68;padding:20px;width:min(92vw,420px);box-shadow:0 0 28px #174d29}
h1{margin:0 0 14px;font-size:25px}.row{display:flex;gap:8px;margin-top:10px}input,select,button{font:inherit;color:#d7ffd7;background:#102218;border:1px solid #4dbd68;padding:12px}
input{width:100%}select{flex:1}button{font-weight:bold}.status{height:22px;margin-top:10px;color:#ffd56a}
#pad{display:none;position:fixed;inset:0;background:radial-gradient(circle at center,#183321,#07100b)}#video{position:absolute;left:18vw;top:3vh;width:64vw;height:55vh;object-fit:contain;background:#000;border:1px solid #295f37;image-rendering:auto}
.top{position:absolute;left:0;right:0;top:7px;text-align:center;font-size:13px}
.stick{position:absolute;width:31vw;height:31vw;max-width:220px;max-height:220px;border:3px solid #4dbd68;border-radius:50%;background:#0b1c11aa}
#ls{left:5vw;bottom:8vh}#rs{right:31vw;bottom:8vh}.nub{position:absolute;width:38%;height:38%;left:31%;top:31%;border-radius:50%;background:#68dd82}
.btn{position:absolute;border-radius:50%;width:14vw;height:14vw;max-width:88px;max-height:88px;padding:0;background:#173923cc}
#ba{right:5vw;bottom:18vh}#bb{right:17vw;bottom:7vh}#bx{right:17vw;bottom:29vh}#by{right:29vw;bottom:18vh}
.small{width:11vw;height:9vw;max-width:74px;max-height:54px;border-radius:10px}
#lb{left:4vw;top:7vh}#lt{left:17vw;top:7vh}#rb{right:17vw;top:7vh}#rt{right:4vw;top:7vh}
#back{left:39vw;top:12vh}#start{right:39vw;top:12vh}#skill{left:44.5vw;top:22vh;width:11vw}
.dpad{position:absolute;left:39vw;bottom:34vh;display:grid;grid-template-columns:repeat(3,44px);grid-template-rows:repeat(3,44px)}
.dpad button{padding:0}.du{grid-column:2}.dl{grid-column:1;grid-row:2}.dr{grid-column:3;grid-row:2}.dd{grid-column:2;grid-row:3}
@media (orientation:portrait){#rs{right:5vw;bottom:31vh}.dpad{left:36vw;bottom:45vh}#ba{bottom:17vh}#bx{bottom:29vh}}
</style>
</head>
<body>
<div id="join"><div class="panel"><h1>NOKIA BLACKBERRY PHOBOI</h1>
<div>Connect this phone as a controller.</div>
<div class="row"><input id="pin" inputmode="numeric" maxlength="6" placeholder="Session PIN"></div>
<div class="row"><select id="slot"><option value="1">Player 2</option><option value="2">Player 3</option><option value="3">Player 4</option></select><button id="connect">CONNECT</button></div>
<div class="status" id="msg"></div></div></div>
<div id="pad"><canvas id="video" width="640" height="360"></canvas><div class="top" id="top">PHOBOI CONTROLLER</div>
<button class="small" id="lb">LB</button><button class="small" id="lt">LT</button>
<button class="small" id="rb">RB</button><button class="small" id="rt">RT</button>
<button class="small" id="back">PHOBOI</button><button class="small" id="start">START</button><button class="small" id="skill">SKILLDEX</button>
<div class="stick" id="ls"><div class="nub"></div></div><div class="stick" id="rs"><div class="nub"></div></div>
<button class="btn" id="ba">A</button><button class="btn" id="bb">B</button><button class="btn" id="bx">X</button><button class="btn" id="by">Y</button>
<div class="dpad"><button class="du" id="du">▲</button><button class="dl" id="dl">◀</button><button class="dr" id="dr">▶</button><button class="dd" id="dd">▼</button></div>
</div>
<script>
let slot=-1,token=0,buttons=0,axes=[0,0,0,0,-32768,-32768],sending=false,seq=0,rtt=0,jitter=0,lastRtt=0,stream=null;
const $=id=>document.getElementById(id);
$('pin').value=new URLSearchParams(location.search).get('pin')||'';
function bindButton(id,bit,axis){
 const e=$(id); const down=ev=>{ev.preventDefault();e.setPointerCapture?.(ev.pointerId);if(axis!==undefined)axes[axis]=32767;else buttons|=(1<<bit)};
 const up=ev=>{ev.preventDefault();if(axis!==undefined)axes[axis]=-32768;else buttons&=~(1<<bit)};
 e.addEventListener('pointerdown',down);e.addEventListener('pointerup',up);e.addEventListener('pointercancel',up);e.addEventListener('pointerleave',up);
}
[['ba',0],['bb',1],['bx',2],['by',3],['back',4],['start',6],['skill',8],['lb',9],['rb',10],['du',11],['dd',12],['dl',13],['dr',14]].forEach(x=>bindButton(x[0],x[1]));
bindButton('lt',0,4);bindButton('rt',0,5);
function bindStick(id,ax,ay){
 const e=$(id),n=e.querySelector('.nub');let active=null;
 function move(ev){if(active!==ev.pointerId)return;ev.preventDefault();const r=e.getBoundingClientRect(),cx=r.left+r.width/2,cy=r.top+r.height/2;
  let x=(ev.clientX-cx)/(r.width*.5),y=(ev.clientY-cy)/(r.height*.5),m=Math.hypot(x,y);if(m>1){x/=m;y/=m}
  axes[ax]=Math.round(x*32767);axes[ay]=Math.round(y*32767);n.style.transform=`translate(${x*r.width*.28}px,${y*r.height*.28}px)`;
 }
 e.addEventListener('pointerdown',ev=>{active=ev.pointerId;e.setPointerCapture(ev.pointerId);move(ev)});
 e.addEventListener('pointermove',move);function end(ev){if(active!==ev.pointerId)return;active=null;axes[ax]=axes[ay]=0;n.style.transform=''}
 e.addEventListener('pointerup',end);e.addEventListener('pointercancel',end);
}
bindStick('ls',0,1);bindStick('rs',2,3);
$('connect').onclick=async()=>{try{const body=new URLSearchParams({slot:$('slot').value,pin:$('pin').value});
 const r=await fetch('/claim',{method:'POST',body});const j=await r.json();if(!j.ok)throw new Error(j.error||'Unable to connect');
 slot=j.slotIndex;token=j.token;$('join').style.display='none';$('pad').style.display='block';$('top').textContent=`PLAYER ${j.player} — CONNECTED`;openSocket();
 if(document.documentElement.requestFullscreen)document.documentElement.requestFullscreen().catch(()=>{});
 if(screen.orientation?.lock)screen.orientation.lock('landscape').catch(()=>{});
}catch(e){$('msg').textContent=e.message}};
let ws=null,reconnectTimer=null;
function openSocket(){
 if(slot<0)return;
 const scheme=location.protocol==='https:'?'wss':'ws';
 ws=new WebSocket(`${scheme}://${location.host}/ws?slot=${slot}&token=${token}`);
 ws.onopen=()=>{$('top').textContent=`PLAYER ${slot+1} — CONNECTED`;navigator.vibrate?.(35);openStream()};
 ws.onmessage=ev=>{
  const m=String(ev.data||'').split(',');if(m[0]!=='a'||m.length<3)return;
  const sent=Number(m[2]);if(!Number.isFinite(sent))return;
  const sample=Math.max(0,performance.now()-sent),delta=Math.abs(sample-lastRtt);lastRtt=sample;
  rtt=rtt?Math.round(rtt*.82+sample*.18):Math.round(sample);jitter=jitter?Math.round(jitter*.8+delta*.2):Math.round(delta);
  const grade=rtt<55&&jitter<15?'EXCELLENT':rtt<100&&jitter<30?'GOOD':rtt<220&&jitter<70?'OK':'ROUGH';
  $('top').textContent=`PLAYER ${slot+1} — ${grade} ${rtt}ms ±${jitter}`;
 };
 ws.onclose=()=>{$('top').textContent='RECONNECTING…';if(slot>=0)reconnectTimer=setTimeout(openSocket,700)};
}
async function inflateFrame(buf){
 const u=new Uint8Array(buf);if(u.length<12)return;const dv=new DataView(buf);const w=dv.getUint16(4,true),h=dv.getUint16(6,true);const payload=buf.slice(12);
 const ds=new DecompressionStream('deflate');const raw=await new Response(new Blob([payload]).stream().pipeThrough(ds)).arrayBuffer();
 const rgba=new Uint8ClampedArray(raw);if(rgba.length!==w*h*4)return;const c=$('video');if(c.width!==w||c.height!==h){c.width=w;c.height=h}c.getContext('2d').putImageData(new ImageData(rgba,w,h),0,0);
}
function openStream(){
 if(slot<0||!('DecompressionStream'in window))return;const scheme=location.protocol==='https:'?'wss':'ws';stream=new WebSocket(`${scheme}://${location.host}/stream?slot=${slot}&token=${token}`);stream.binaryType='arraybuffer';
 stream.onmessage=e=>inflateFrame(e.data).catch(()=>{});stream.onclose=()=>{if(slot>=0)setTimeout(openStream,1200)};
}
async function send(){
 if(slot<0)return;
 const stamp=performance.now();
 const packet=[++seq,stamp,Math.round(rtt),Math.round(jitter),axes[0],axes[1],axes[2],axes[3],axes[4],axes[5],buttons].join(',');
 if(ws&&ws.readyState===WebSocket.OPEN){ws.send(packet);return}
 if(sending)return;sending=true;
 try{const body=new URLSearchParams({slot,token,lx:axes[0],ly:axes[1],rx:axes[2],ry:axes[3],lt:axes[4],rt:axes[5],buttons});await fetch('/input',{method:'POST',body,keepalive:true})}catch(e){}
 sending=false;
}
setInterval(send,16);addEventListener('pagehide',()=>{slot<0||navigator.sendBeacon('/release',new URLSearchParams({slot,token}))});
</script></body></html>)PHOBOI";
}

std::string mobileReadRequest(MobileSocket socket)
{
    std::string request;
    char buffer[4096];
    size_t expected = 0;

    while (request.size() < 32768) {
#ifdef _WIN32
        int count = recv(socket, buffer, static_cast<int>(sizeof(buffer)), 0);
#else
        int count = static_cast<int>(recv(socket, buffer, sizeof(buffer), 0));
#endif
        if (count <= 0) {
            break;
        }

        request.append(buffer, count);
        size_t headerEnd = request.find("\r\n\r\n");
        if (headerEnd != std::string::npos) {
            if (expected == 0) {
                size_t lengthAt = request.find("Content-Length:");
                if (lengthAt == std::string::npos) {
                    lengthAt = request.find("content-length:");
                }
                if (lengthAt != std::string::npos) {
                    expected = headerEnd + 4 + static_cast<size_t>(
                        std::atoi(request.c_str() + lengthAt + 15));
                } else {
                    expected = headerEnd + 4;
                }
            }
            if (request.size() >= expected) {
                break;
            }
        }
    }

    return request;
}

void mobileSendResponse(MobileSocket socket, const char* status, const char* type, const std::string& body)
{
    std::ostringstream response;
    response << "HTTP/1.1 " << status << "\r\n"
             << "Content-Type: " << type << "\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Cache-Control: no-store\r\n"
             << "X-Content-Type-Options: nosniff\r\n"
             << "Connection: close\r\n\r\n"
             << body;
    std::string bytes = response.str();
#ifdef _WIN32
    send(socket, bytes.c_str(), static_cast<int>(bytes.size()), 0);
#else
    send(socket, bytes.c_str(), bytes.size(), 0);
#endif
}

uint32_t mobileRotateLeft(uint32_t value, int amount)
{
    return (value << amount) | (value >> (32 - amount));
}

std::array<uint8_t, 20> mobileSha1(const std::string& input)
{
    std::vector<uint8_t> bytes(input.begin(), input.end());
    uint64_t bitLength = static_cast<uint64_t>(bytes.size()) * 8;
    bytes.push_back(0x80);
    while ((bytes.size() % 64) != 56) {
        bytes.push_back(0);
    }
    for (int shift = 56; shift >= 0; shift -= 8) {
        bytes.push_back(static_cast<uint8_t>(bitLength >> shift));
    }

    uint32_t h0 = 0x67452301;
    uint32_t h1 = 0xEFCDAB89;
    uint32_t h2 = 0x98BADCFE;
    uint32_t h3 = 0x10325476;
    uint32_t h4 = 0xC3D2E1F0;

    for (size_t chunk = 0; chunk < bytes.size(); chunk += 64) {
        uint32_t words[80] {};
        for (int i = 0; i < 16; i++) {
            size_t at = chunk + i * 4;
            words[i] =
                (static_cast<uint32_t>(bytes[at]) << 24)
                | (static_cast<uint32_t>(bytes[at + 1]) << 16)
                | (static_cast<uint32_t>(bytes[at + 2]) << 8)
                | static_cast<uint32_t>(bytes[at + 3]);
        }
        for (int i = 16; i < 80; i++) {
            words[i] = mobileRotateLeft(
                words[i - 3] ^ words[i - 8] ^ words[i - 14] ^ words[i - 16],
                1);
        }

        uint32_t a = h0;
        uint32_t b = h1;
        uint32_t d = h3;
        uint32_t e = h4;
        uint32_t f;
        uint32_t k;
        uint32_t cc = h2;

        for (int i = 0; i < 80; i++) {
            if (i < 20) {
                f = (b & cc) | ((~b) & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ cc ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & cc) | (b & d) | (cc & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ cc ^ d;
                k = 0xCA62C1D6;
            }

            uint32_t temp = mobileRotateLeft(a, 5) + f + e + k + words[i];
            e = d;
            d = cc;
            cc = mobileRotateLeft(b, 30);
            b = a;
            a = temp;
        }

        h0 += a;
        h1 += b;
        h2 += cc;
        h3 += d;
        h4 += e;
    }

    std::array<uint8_t, 20> digest {};
    uint32_t hashes[5] = { h0, h1, h2, h3, h4 };
    for (int i = 0; i < 5; i++) {
        digest[i * 4] = static_cast<uint8_t>(hashes[i] >> 24);
        digest[i * 4 + 1] = static_cast<uint8_t>(hashes[i] >> 16);
        digest[i * 4 + 2] = static_cast<uint8_t>(hashes[i] >> 8);
        digest[i * 4 + 3] = static_cast<uint8_t>(hashes[i]);
    }
    return digest;
}

std::string mobileBase64(const uint8_t* data, size_t length)
{
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    for (size_t i = 0; i < length; i += 3) {
        uint32_t value = static_cast<uint32_t>(data[i]) << 16;
        if (i + 1 < length) {
            value |= static_cast<uint32_t>(data[i + 1]) << 8;
        }
        if (i + 2 < length) {
            value |= data[i + 2];
        }

        result.push_back(alphabet[(value >> 18) & 63]);
        result.push_back(alphabet[(value >> 12) & 63]);
        result.push_back(i + 1 < length ? alphabet[(value >> 6) & 63] : '=');
        result.push_back(i + 2 < length ? alphabet[value & 63] : '=');
    }
    return result;
}

std::string mobileHeaderValue(const std::string& request, const char* name)
{
    std::string prefix = std::string(name) + ":";
    size_t at = request.find(prefix);
    if (at == std::string::npos) {
        return "";
    }
    at += prefix.size();
    while (at < request.size() && (request[at] == ' ' || request[at] == '\t')) {
        at++;
    }
    size_t end = request.find("\r\n", at);
    return request.substr(at, end == std::string::npos ? std::string::npos : end - at);
}

bool mobileRecvExact(MobileSocket socket, uint8_t* bytes, size_t length)
{
    size_t received = 0;
    while (received < length) {
#ifdef _WIN32
        int count = recv(
            socket,
            reinterpret_cast<char*>(bytes + received),
            static_cast<int>(length - received),
            0);
#else
        int count = static_cast<int>(recv(socket, bytes + received, length - received, 0));
#endif
        if (count <= 0) {
            return false;
        }
        received += static_cast<size_t>(count);
    }
    return true;
}

bool mobileWaitReadable(MobileSocket socket, int milliseconds)
{
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(socket, &readSet);
    timeval timeout {
        milliseconds / 1000,
        (milliseconds % 1000) * 1000
    };
#ifdef _WIN32
    return select(0, &readSet, nullptr, nullptr, &timeout) > 0;
#else
    return select(socket + 1, &readSet, nullptr, nullptr, &timeout) > 0;
#endif
}

bool mobileSendWebSocketText(MobileSocket socket, const std::string& text)
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

bool mobileReadWebSocketText(MobileSocket socket, std::string& text)
{
    uint8_t header[2];
    if (!mobileRecvExact(socket, header, sizeof(header))) {
        return false;
    }

    int opcode = header[0] & 0x0F;
    bool masked = (header[1] & 0x80) != 0;
    uint64_t length = header[1] & 0x7F;
    if (length == 126) {
        uint8_t extended[2];
        if (!mobileRecvExact(socket, extended, sizeof(extended))) {
            return false;
        }
        length = (static_cast<uint64_t>(extended[0]) << 8) | extended[1];
    } else if (length == 127) {
        uint8_t extended[8];
        if (!mobileRecvExact(socket, extended, sizeof(extended))) {
            return false;
        }
        length = 0;
        for (uint8_t byte : extended) {
            length = (length << 8) | byte;
        }
    }

    if (!masked || length > 1024) {
        return false;
    }

    uint8_t mask[4];
    if (!mobileRecvExact(socket, mask, sizeof(mask))) {
        return false;
    }

    std::vector<uint8_t> payload(static_cast<size_t>(length));
    if (length > 0 && !mobileRecvExact(socket, payload.data(), payload.size())) {
        return false;
    }
    for (size_t i = 0; i < payload.size(); i++) {
        payload[i] ^= mask[i % 4];
    }

    if (opcode == 8) {
        return false;
    }
    if (opcode != 1) {
        text.clear();
        return true;
    }

    text.assign(payload.begin(), payload.end());
    return true;
}

void mobileRunWebSocket(
    MobileSocket socket,
    const std::string& request,
    int slot,
    uint32_t token)
{
    std::string key = mobileHeaderValue(request, "Sec-WebSocket-Key");
    if (key.empty()) {
        mobileSendResponse(socket, "400 Bad Request", "text/plain", "Missing WebSocket key");
        return;
    }

    std::array<uint8_t, 20> digest = mobileSha1(
        key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    std::string accept = mobileBase64(digest.data(), digest.size());

    std::ostringstream response;
    response << "HTTP/1.1 101 Switching Protocols\r\n"
             << "Upgrade: websocket\r\n"
             << "Connection: Upgrade\r\n"
             << "Sec-WebSocket-Accept: " << accept << "\r\n\r\n";
    std::string bytes = response.str();
#ifdef _WIN32
    send(socket, bytes.c_str(), static_cast<int>(bytes.size()), 0);
#else
    send(socket, bytes.c_str(), bytes.size(), 0);
#endif

    MobileSlotState& state = gMobileSlots[slot];
    while (gMobileServerRunning.load()
        && state.claimed.load()
        && state.token.load() == token) {
        if (!mobileWaitReadable(socket, 500)) {
            continue;
        }

        std::string message;
        if (!mobileReadWebSocketText(socket, message)) {
            break;
        }
        if (message.empty()) {
            continue;
        }

        unsigned int sequence = 0;
        double clientStamp = 0.0;
        int clientRtt = 0;
        int clientJitter = 0;
        int lx;
        int ly;
        int rx;
        int ry;
        int lt;
        int rt;
        unsigned int buttons;
        if (sscanf(
                message.c_str(),
                "%u,%lf,%d,%d,%d,%d,%d,%d,%d,%d,%u",
                &sequence,
                &clientStamp,
                &clientRtt,
                &clientJitter,
                &lx,
                &ly,
                &rx,
                &ry,
                &lt,
                &rt,
                &buttons)
            == 11) {
            uint64_t arrival = mobileNow();
            mobileRecordArrival(state, arrival);
            state.packetSequence.store(sequence);
            state.rttMs.store(std::max(0, std::min(clientRtt, 5000)));
            state.jitterMs.store(std::max(state.jitterMs.load(), std::max(0, std::min(clientJitter, 2000))));
            state.axes[SDL_CONTROLLER_AXIS_LEFTX].store(lx);
            state.axes[SDL_CONTROLLER_AXIS_LEFTY].store(ly);
            state.axes[SDL_CONTROLLER_AXIS_RIGHTX].store(rx);
            state.axes[SDL_CONTROLLER_AXIS_RIGHTY].store(ry);
            state.axes[SDL_CONTROLLER_AXIS_TRIGGERLEFT].store(lt);
            state.axes[SDL_CONTROLLER_AXIS_TRIGGERRIGHT].store(rt);
            state.buttons.store(buttons);
            state.lastSeen.store(arrival);
            std::ostringstream ack; ack << "a," << sequence << "," << clientStamp;
            if (!mobileSendWebSocketText(socket, ack.str())) break;
        }
    }

    mobileResetInput(state);
}

void mobileRunStreamWebSocket(MobileSocket socket, const std::string& request, int slot, uint32_t token)
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

void mobileResetInput(MobileSlotState& state)
{
    for (int axis = 0; axis < SDL_CONTROLLER_AXIS_MAX; axis++) {
        state.axes[axis].store(
            axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT || axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT
                ? SDL_JOYSTICK_AXIS_MIN
                : 0);
    }
    state.buttons.store(0);
}

void mobileHandleClient(MobileSocket client)
{
    std::string request = mobileReadRequest(client);
    size_t firstSpace = request.find(' ');
    size_t secondSpace = firstSpace == std::string::npos ? std::string::npos : request.find(' ', firstSpace + 1);
    if (firstSpace == std::string::npos || secondSpace == std::string::npos) {
        mobileSendResponse(client, "400 Bad Request", "text/plain", "Bad request");
        return;
    }

    std::string method = request.substr(0, firstSpace);
    std::string target = request.substr(firstSpace + 1, secondSpace - firstSpace - 1);
    size_t queryAt = target.find('?');
    std::string route = target.substr(0, queryAt);
    std::string values = queryAt == std::string::npos ? "" : target.substr(queryAt + 1);
    size_t bodyAt = request.find("\r\n\r\n");
    if (bodyAt != std::string::npos && bodyAt + 4 < request.size()) {
        if (!values.empty()) {
            values += "&";
        }
        values += request.substr(bodyAt + 4);
    }

    if (method == "GET" && route == "/") {
        mobileSendResponse(client, "200 OK", "text/html; charset=utf-8", mobileControllerHtml());
        return;
    }

    int slot = mobileValue(values, "slot", -1);
    if (slot <= 0 || slot >= kLocalCoopMaxPlayers) {
        mobileSendResponse(client, "400 Bad Request", "application/json", "{\"ok\":false,\"error\":\"Invalid player slot\"}");
        return;
    }

    MobileSlotState& state = gMobileSlots[slot];

    if (method == "GET" && route == "/ws") {
        uint32_t token = static_cast<uint32_t>(mobileValue(values, "token", 0));
        if (!state.claimed.load() || token == 0 || token != state.token.load()) {
            mobileSendResponse(client, "403 Forbidden", "text/plain", "Session expired");
            return;
        }
        mobileRunWebSocket(client, request, slot, token);
        return;
    }

    if (method == "GET" && route == "/stream") {
        uint32_t token = static_cast<uint32_t>(mobileValue(values, "token", 0));
        if (!state.claimed.load() || token == 0 || token != state.token.load()) {
            mobileSendResponse(client, "403 Forbidden", "text/plain", "Session expired");
            return;
        }
        mobileRunStreamWebSocket(client, request, slot, token);
        return;
    }

    if (method == "POST" && route == "/claim") {
        int pin = mobileValue(values, "pin", -1);
        if (pin != gMobilePin) {
            mobileSendResponse(client, "403 Forbidden", "application/json", "{\"ok\":false,\"error\":\"Wrong session PIN\"}");
            return;
        }
        bool expected = false;
        if (!state.available.load() || !state.claimed.compare_exchange_strong(expected, true)) {
            mobileSendResponse(client, "409 Conflict", "application/json", "{\"ok\":false,\"error\":\"That player slot is busy\"}");
            return;
        }

        uint32_t token = mobileNextToken();
        state.token.store(token);
        state.lastSeen.store(mobileNow());
        mobileResetInput(state);

        std::ostringstream body;
        body << "{\"ok\":true,\"slotIndex\":" << slot
             << ",\"player\":" << slot + 1
             << ",\"token\":" << token << "}";
        mobileSendResponse(client, "200 OK", "application/json", body.str());
        return;
    }

    uint32_t token = static_cast<uint32_t>(mobileValue(values, "token", 0));
    if (!state.claimed.load() || token == 0 || token != state.token.load()) {
        mobileSendResponse(client, "403 Forbidden", "application/json", "{\"ok\":false,\"error\":\"Session expired\"}");
        return;
    }

    if (method == "POST" && route == "/input") {
        state.axes[SDL_CONTROLLER_AXIS_LEFTX].store(mobileValue(values, "lx", 0));
        state.axes[SDL_CONTROLLER_AXIS_LEFTY].store(mobileValue(values, "ly", 0));
        state.axes[SDL_CONTROLLER_AXIS_RIGHTX].store(mobileValue(values, "rx", 0));
        state.axes[SDL_CONTROLLER_AXIS_RIGHTY].store(mobileValue(values, "ry", 0));
        state.axes[SDL_CONTROLLER_AXIS_TRIGGERLEFT].store(mobileValue(values, "lt", SDL_JOYSTICK_AXIS_MIN));
        state.axes[SDL_CONTROLLER_AXIS_TRIGGERRIGHT].store(mobileValue(values, "rt", SDL_JOYSTICK_AXIS_MIN));
        state.buttons.store(static_cast<uint32_t>(mobileValue(values, "buttons", 0)));
        state.lastSeen.store(mobileNow());
        mobileSendResponse(client, "200 OK", "application/json", "{\"ok\":true}");
        return;
    }

    if (method == "POST" && route == "/release") {
        mobileResetInput(state);
        state.claimed.store(false);
        state.token.store(0);
        mobileSendResponse(client, "200 OK", "application/json", "{\"ok\":true}");
        return;
    }

    mobileSendResponse(client, "404 Not Found", "text/plain", "Not found");
}

void mobileServerLoop()
{
    while (gMobileServerRunning.load()) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(gMobileListenSocket, &readSet);
        timeval timeout { 0, 250000 };

#ifdef _WIN32
        int ready = select(0, &readSet, nullptr, nullptr, &timeout);
#else
        int ready = select(gMobileListenSocket + 1, &readSet, nullptr, nullptr, &timeout);
#endif
        if (ready <= 0 || !gMobileServerRunning.load()) {
            continue;
        }

        sockaddr_storage address {};
#ifdef _WIN32
        int addressLength = sizeof(address);
#else
        socklen_t addressLength = sizeof(address);
#endif
        MobileSocket client = accept(gMobileListenSocket, reinterpret_cast<sockaddr*>(&address), &addressLength);
        if (client == kInvalidMobileSocket) {
            continue;
        }

        gMobileClientThreads.emplace_back([client]() {
            mobileHandleClient(client);
            mobileCloseSocket(client);
        });
    }
}

std::string mobileHostAddress()
{
    char hostname[256] {};
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        return "127.0.0.1";
    }

    addrinfo hints {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* result = nullptr;
    if (getaddrinfo(hostname, nullptr, &hints, &result) != 0) {
        return "127.0.0.1";
    }

    std::string address = "127.0.0.1";
    for (addrinfo* current = result; current != nullptr; current = current->ai_next) {
        auto* ipv4 = reinterpret_cast<sockaddr_in*>(current->ai_addr);
        char text[INET_ADDRSTRLEN] {};
        if (inet_ntop(AF_INET, &ipv4->sin_addr, text, sizeof(text)) != nullptr
            && strcmp(text, "127.0.0.1") != 0) {
            address = text;
            break;
        }
    }
    freeaddrinfo(result);
    return address;
}

std::string mobilePairingUrl()
{
    std::ostringstream url;
#ifdef _WIN32
    {
        std::lock_guard<std::mutex> lock(gCloudflareStateMutex);
        if (!gCloudflarePublicUrl.empty()) {
            url << gCloudflarePublicUrl << "/?pin=" << gMobilePin;
            return url.str();
        }
    }
#endif
    url << "http://" << mobileHostAddress() << ":" << kMobilePort << "/?pin=" << gMobilePin;
    return url.str();
}

std::string mobileCloudflareStatus()
{
#ifdef _WIN32
    std::lock_guard<std::mutex> lock(gCloudflareStateMutex);
    return gCloudflareStatus;
#else
    return "WINDOWS BUILD ONLY";
#endif
}

void mobileDrawQrCode(int window, const std::string& text, int left, int top)
{
    qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(
        text.c_str(),
        qrcodegen::QrCode::Ecc::MEDIUM);
    constexpr int scale = 5;
    constexpr int border = 3;
    int side = (qr.getSize() + border * 2) * scale;
    windowFill(window, left, top, side, side, _colorTable[32767]);

    for (int y = 0; y < qr.getSize(); y++) {
        for (int x = 0; x < qr.getSize(); x++) {
            if (qr.getModule(x, y)) {
                windowFill(
                    window,
                    left + (x + border) * scale,
                    top + (y + border) * scale,
                    scale,
                    scale,
                    _colorTable[0]);
            }
        }
    }
}

void mobileDrawHostWindow()
{
    if (gMobileHostWindow == -1) {
        return;
    }

    windowFill(gMobileHostWindow, 0, 0, 620, 420, _colorTable[0]);
    windowDrawBorder(gMobileHostWindow);
    windowDrawText(
        gMobileHostWindow,
        "NOKIA BLACKBERRY PHOBOI - PHONE HOST",
        350,
        20,
        18,
        _colorTable[992]);

    std::string address = mobilePairingUrl();
    char line[256] {};
    snprintf(line, sizeof(line), "OPEN OR SCAN: %s", address.c_str());
    windowDrawText(gMobileHostWindow, line, 360, 20, 54, _colorTable[992]);

    snprintf(line, sizeof(line), "SESSION PIN: %06d", gMobilePin);
    windowDrawText(gMobileHostWindow, line, 340, 20, 82, _colorTable[32767]);
    std::string tunnelStatus = mobileCloudflareStatus();
    snprintf(line, sizeof(line), "C: COPY LINK   T: CLOUDFLARE HTTPS [%s]", tunnelStatus.c_str());
    windowDrawText(gMobileHostWindow, line, 575, 20, 112, _colorTable[992]);

    for (int slot = 1; slot < kLocalCoopMaxPlayers; slot++) {
        const char* status = "OPEN";
        if (gMobileSlots[slot].claimed.load()) {
            status = "PHONE CONNECTED";
        } else if (gLocalCoopPlayers[slot].connected) {
            status = "PHYSICAL CONTROLLER";
        } else if (gLocalCoopPlayers[slot].slotLocked) {
            status = "RESERVED";
        }

        snprintf(line, sizeof(line), "PLAYER %d: %s", slot + 1, status);
        windowDrawText(gMobileHostWindow, line, 340, 20, 150 + (slot - 1) * 34, _colorTable[992]);
    }

    windowDrawText(gMobileHostWindow, "2 / 3 / 4: KICK PHONE SLOT", 340, 20, 278, _colorTable[992]);
    windowDrawText(gMobileHostWindow, "F11 OR ESC: CLOSE", 340, 20, 308, _colorTable[992]);
    windowDrawText(gMobileHostWindow, "HTTPS TUNNEL HIDES YOUR IP AND NEEDS NO VPN", 575, 20, 350, _colorTable[992]);

    mobileDrawQrCode(gMobileHostWindow, address, 395, 145);
    windowRefresh(gMobileHostWindow);
    gMobileHostWindowLastDraw = mobileNow();
}

void mobileOpenHostWindow()
{
    if (gMobileHostWindow != -1) {
        return;
    }

    gMobileHostWindow = windowCreate(
        (screenGetWidth() - 620) / 2,
        (screenGetVisibleHeight() - 420) / 2,
        620,
        420,
        _colorTable[0],
        WINDOW_MOVE_ON_TOP);
    mobileDrawHostWindow();
}

void mobileCloseHostWindow()
{
    if (gMobileHostWindow != -1) {
        windowDestroy(gMobileHostWindow);
        gMobileHostWindow = -1;
    }
}

bool mobileStartServer()
{
    if (gMobileServerStarted.exchange(true)) {
        return gMobileServerRunning.load();
    }

#ifdef _WIN32
    WSADATA data {};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        return false;
    }
#endif

    gMobilePin = 100000 + static_cast<int>((mobileNow() ^ 0xB10B01u) % 900000);
    gMobileListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (gMobileListenSocket == kInvalidMobileSocket) {
        return false;
    }

    int reuse = 1;
#ifdef _WIN32
    setsockopt(gMobileListenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
#else
    setsockopt(gMobileListenSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_port = htons(kMobilePort);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(gMobileListenSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0
        || listen(gMobileListenSocket, 8) != 0) {
        mobileCloseSocket(gMobileListenSocket);
        gMobileListenSocket = kInvalidMobileSocket;
        return false;
    }

    gMobileServerRunning.store(true);
    gMobileServerThread = std::thread(mobileServerLoop);
    std::atexit(localCoopMobileShutdown);
    return true;
}

#ifdef _WIN32
void mobileSetCloudflareStatus(const char* status)
{
    std::lock_guard<std::mutex> lock(gCloudflareStateMutex);
    gCloudflareStatus = status != nullptr ? status : "ERROR";
}

bool mobileFindCloudflareUrl(const std::string& output, std::string* url)
{
    size_t start = output.find("https://");
    while (start != std::string::npos) {
        size_t end = output.find(".trycloudflare.com", start);
        if (end != std::string::npos) {
            end += std::strlen(".trycloudflare.com");
            std::string candidate = output.substr(start, end - start);
            if (candidate.size() < 256) {
                *url = candidate;
                return true;
            }
        }
        start = output.find("https://", start + 8);
    }
    return false;
}

std::string mobileCloudflaredPath()
{
    char modulePath[MAX_PATH] {};
    DWORD length = GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
        return "cloudflared.exe";
    }
    std::string path(modulePath, length);
    size_t separator = path.find_last_of("\\/");
    if (separator == std::string::npos) {
        return "cloudflared.exe";
    }
    path.resize(separator + 1);
    path += "cloudflared.exe";
    return path;
}

void mobileReleaseCloudflareHandles()
{
    if (gCloudflareOutputThread.joinable()) {
        gCloudflareOutputThread.join();
    }
    if (gCloudflareOutput != nullptr) {
        CloseHandle(gCloudflareOutput);
        gCloudflareOutput = nullptr;
    }
    if (gCloudflareProcess != nullptr) {
        CloseHandle(gCloudflareProcess);
        gCloudflareProcess = nullptr;
    }
}

bool mobileStartCloudflareTunnel()
{
    if (gCloudflareProcess != nullptr) {
        DWORD exitCode = STILL_ACTIVE;
        if (GetExitCodeProcess(gCloudflareProcess, &exitCode) && exitCode == STILL_ACTIVE) {
            return true;
        }
        mobileReleaseCloudflareHandles();
    }

    std::string executable = mobileCloudflaredPath();
    if (GetFileAttributesA(executable.c_str()) == INVALID_FILE_ATTRIBUTES) {
        mobileSetCloudflareStatus("MISSING EXE");
        return false;
    }

    SECURITY_ATTRIBUTES security {};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;
    HANDLE outputRead = nullptr;
    HANDLE outputWrite = nullptr;
    if (!CreatePipe(&outputRead, &outputWrite, &security, 0)
        || !SetHandleInformation(outputRead, HANDLE_FLAG_INHERIT, 0)) {
        if (outputRead != nullptr) CloseHandle(outputRead);
        if (outputWrite != nullptr) CloseHandle(outputWrite);
        mobileSetCloudflareStatus("PIPE ERROR");
        return false;
    }

    STARTUPINFOA startup {};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = outputWrite;
    startup.hStdError = outputWrite;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION process {};
    std::string commandText = "\"" + executable + "\" tunnel --no-autoupdate --url http://127.0.0.1:27888";
    std::vector<char> command(commandText.begin(), commandText.end());
    command.push_back('\0');
    BOOL created = CreateProcessA(
        executable.c_str(),
        command.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &startup,
        &process);
    CloseHandle(outputWrite);
    if (!created) {
        CloseHandle(outputRead);
        mobileSetCloudflareStatus("START ERROR");
        return false;
    }

    CloseHandle(process.hThread);
    gCloudflareProcess = process.hProcess;
    gCloudflareOutput = outputRead;
    mobileSetCloudflareStatus("CONNECTING");
    gCloudflareOutputThread = std::thread([outputRead]() {
        std::string output;
        char buffer[2048];
        DWORD count = 0;
        while (ReadFile(outputRead, buffer, sizeof(buffer), &count, nullptr) && count != 0) {
            output.append(buffer, static_cast<size_t>(count));
            if (output.size() > 32768) {
                output.erase(0, output.size() - 16384);
            }
            std::string publicUrl;
            if (mobileFindCloudflareUrl(output, &publicUrl)) {
                std::lock_guard<std::mutex> lock(gCloudflareStateMutex);
                gCloudflarePublicUrl = publicUrl;
                gCloudflareStatus = "ONLINE";
            }
        }
        std::lock_guard<std::mutex> lock(gCloudflareStateMutex);
        if (gCloudflarePublicUrl.empty()) {
            gCloudflareStatus = "FAILED - PRESS T";
        } else {
            gCloudflareStatus = "STOPPED - PRESS T";
            gCloudflarePublicUrl.clear();
        }
    });
    debugPrint("[PHOBOI MOBILE] Cloudflare Quick Tunnel starting\n");
    return true;
}

void mobileStopCloudflareTunnel()
{
    if (gCloudflareProcess != nullptr) {
        TerminateProcess(gCloudflareProcess, 0);
        WaitForSingleObject(gCloudflareProcess, 3000);
    }
    mobileReleaseCloudflareHandles();
    std::lock_guard<std::mutex> lock(gCloudflareStateMutex);
    gCloudflarePublicUrl.clear();
    gCloudflareStatus = "OFF";
}
#endif

bool mobileAttachController(int slot)
{
    if (slot <= 0 || slot >= kLocalCoopMaxPlayers || gLocalCoopPlayers[slot].connected) {
        return false;
    }

    int deviceIndex = SDL_JoystickAttachVirtual(
        SDL_JOYSTICK_TYPE_GAMECONTROLLER,
        SDL_CONTROLLER_AXIS_MAX,
        SDL_CONTROLLER_BUTTON_MAX,
        0);
    if (deviceIndex < 0) {
        debugPrint("[PHOBOI MOBILE] virtual controller attach failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_GameController* controller = SDL_GameControllerOpen(deviceIndex);
    if (controller == nullptr) {
        debugPrint("[PHOBOI MOBILE] controller open failed: %s\n", SDL_GetError());
        SDL_JoystickDetachVirtual(deviceIndex);
        return false;
    }

    SDL_Joystick* joystick = SDL_GameControllerGetJoystick(controller);
    LocalCoopPlayer& player = gLocalCoopPlayers[slot];
    player.slot = slot;
    player.controller = controller;
    player.joystickId = SDL_JoystickInstanceID(joystick);
    player.connected = true;
    snprintf(player.controllerGuid, sizeof(player.controllerGuid), "PHOBOI-MOBILE-%d", slot + 1);

    MobileVirtualDevice& device = gMobileDevices[slot];
    device.deviceIndex = deviceIndex;
    device.controller = controller;
    device.joystick = joystick;

    if (player.slotLocked && player.actor != nullptr) {
        player.actor->flags &= ~(OBJECT_HIDDEN | OBJECT_NO_BLOCK);
        player.humanOwned = true;
    }

    debugPrint("[PHOBOI MOBILE] player %d connected\n", slot + 1);
    return true;
}

void mobileDetachController(int slot)
{
    MobileVirtualDevice& device = gMobileDevices[slot];
    if (device.deviceIndex < 0) {
        return;
    }

    LocalCoopPlayer& player = gLocalCoopPlayers[slot];
    if (player.controller == device.controller) {
        localCoopClearController(player);
    } else if (device.controller != nullptr) {
        SDL_GameControllerClose(device.controller);
    }

    SDL_JoystickDetachVirtual(device.deviceIndex);
    device = MobileVirtualDevice {};
    debugPrint("[PHOBOI MOBILE] player %d disconnected\n", slot + 1);
}

void mobileApplyController(int slot)
{
    MobileSlotState& state = gMobileSlots[slot];
    MobileVirtualDevice& device = gMobileDevices[slot];
    if (device.joystick == nullptr) {
        return;
    }

    int rtt = state.rttMs.load();
    int jitter = state.jitterMs.load();
    int interval = state.packetIntervalMs.load();
    int smoothingDivisor = (rtt > 300 || jitter > 90 || interval > 100) ? 4
        : ((rtt > 160 || jitter > 45 || interval > 55) ? 3 : 2);
    for (int axis = 0; axis < SDL_CONTROLLER_AXIS_MAX; axis++) {
        int target = state.axes[axis].load();
        target = std::max(static_cast<int>(SDL_JOYSTICK_AXIS_MIN), std::min(static_cast<int>(SDL_JOYSTICK_AXIS_MAX), target));
        int current = state.smoothedAxes[axis];
        int value = current + (target - current) / smoothingDivisor;
        if (std::abs(target - current) < 512) value = target;
        state.smoothedAxes[axis] = value;
        SDL_JoystickSetVirtualAxis(device.joystick, axis, static_cast<Sint16>(value));
    }

    uint32_t buttons = state.buttons.load();
    for (int button = 0; button < SDL_CONTROLLER_BUTTON_MAX; button++) {
        SDL_JoystickSetVirtualButton(
            device.joystick,
            button,
            (buttons & (1u << button)) != 0 ? SDL_PRESSED : SDL_RELEASED);
    }
}

} // namespace

void localCoopMobileTick()
{
    if (!gLocalCoopInitialized || !mobileStartServer()) {
        return;
    }

    uint64_t now = mobileNow();
    for (int slot = 1; slot < kLocalCoopMaxPlayers; slot++) {
        MobileSlotState& state = gMobileSlots[slot];

        uint64_t inputAge = now - state.lastSeen.load();
        uint64_t neutralAfter = (state.rttMs.load() > 300 || state.jitterMs.load() > 90 || state.packetIntervalMs.load() > 100) ? kMobileBadLinkNeutralMs : kMobileInputNeutralMs;
        if (state.claimed.load() && inputAge > neutralAfter) {
            // A lost packet must never leave movement, aiming, or an attack held.
            // Keep the player's reservation for sixty seconds. Neutralization is
            // adaptive so high-latency links are not mistaken for disconnects. This lets save/load and other blocking
            // modal work finish without kicking a remote player.
            mobileResetInput(state);
        }
        if (state.claimed.load() && inputAge > kMobileTimeoutMs) {
            state.claimed.store(false);
            state.token.store(0);
        }

        bool mobileAttached = gMobileDevices[slot].deviceIndex >= 0;
        bool occupiedByOtherController =
            gLocalCoopPlayers[slot].connected && !mobileAttached;
        state.available.store(!occupiedByOtherController && !state.claimed.load());

        if (state.claimed.load() && !mobileAttached) {
            mobileAttachController(slot);
        } else if (!state.claimed.load() && mobileAttached) {
            mobileDetachController(slot);
        }

        if (state.claimed.load()) {
            mobileApplyController(slot);
        }
    }

    SDL_JoystickUpdate();
    mobileCaptureStreamFrame(now);

    if (gMobileHostWindow != -1 && now - gMobileHostWindowLastDraw >= 250) {
        mobileDrawHostWindow();
    }

    if (!gMobileNoticeShown && gDude != nullptr) {
        gMobileNoticeShown = true;
        std::ostringstream message;
        message << "PhoBoi phone controllers: http://" << mobileHostAddress()
                << ":" << kMobilePort << "  PIN " << gMobilePin;
        std::string messageText = message.str();
        char mutableMessage[256] {};
        snprintf(mutableMessage, sizeof(mutableMessage), "%s", messageText.c_str());
        displayMonitorAddMessage(mutableMessage);
        debugPrint("[PHOBOI MOBILE] %s\n", mutableMessage);
    }
}

bool localCoopMobileHandleKey(int keyCode)
{
    if (keyCode == KEY_F11) {
        if (gMobileHostWindow == -1) {
            mobileOpenHostWindow();
        } else {
            mobileCloseHostWindow();
        }
        return true;
    }

    if (gMobileHostWindow == -1) {
        return false;
    }

    if (keyCode == KEY_ESCAPE) {
        mobileCloseHostWindow();
        return true;
    }

    if (keyCode == KEY_LOWERCASE_C || keyCode == KEY_UPPERCASE_C) {
        std::string pairingUrl = mobilePairingUrl();
        SDL_SetClipboardText(pairingUrl.c_str());
        mobileDrawHostWindow();
        return true;
    }

    if (keyCode == KEY_LOWERCASE_T || keyCode == KEY_UPPERCASE_T) {
#ifdef _WIN32
        mobileStartCloudflareTunnel();
#endif
        mobileDrawHostWindow();
        return true;
    }

    int slot = -1;
    if (keyCode == KEY_2) {
        slot = 1;
    } else if (keyCode == KEY_3) {
        slot = 2;
    } else if (keyCode == KEY_4) {
        slot = 3;
    }

    if (slot != -1 && gMobileSlots[slot].claimed.load()) {
        mobileResetInput(gMobileSlots[slot]);
        gMobileSlots[slot].claimed.store(false);
        gMobileSlots[slot].token.store(0);
        mobileDrawHostWindow();
        return true;
    }

    return keyCode != -1;
}

void localCoopMobileShutdown()
{
    mobileCloseHostWindow();

#ifdef _WIN32
    mobileStopCloudflareTunnel();
#endif

    if (!gMobileServerStarted.load()) {
        return;
    }

    gMobileServerRunning.store(false);
    if (gMobileListenSocket != kInvalidMobileSocket) {
        mobileCloseSocket(gMobileListenSocket);
        gMobileListenSocket = kInvalidMobileSocket;
    }
    if (gMobileServerThread.joinable()) {
        gMobileServerThread.join();
    }
    for (std::thread& clientThread : gMobileClientThreads) {
        if (clientThread.joinable()) {
            clientThread.join();
        }
    }
    gMobileClientThreads.clear();

    for (int slot = 1; slot < kLocalCoopMaxPlayers; slot++) {
        mobileDetachController(slot);
    }

#ifdef _WIN32
    WSACleanup();
#endif
}

} // namespace fallout
