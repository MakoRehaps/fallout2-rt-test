#include "local_coop_mobile.h"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
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

namespace fallout {
namespace {

constexpr int kMobilePort = 27888;
constexpr uint64_t kMobileTimeoutMs = 10000;

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
    std::atomic<uint32_t> buttons { 0 };

    MobileSlotState()
    {
        for (auto& axis : axes) {
            axis.store(0);
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
MobileSocket gMobileListenSocket = kInvalidMobileSocket;
int gMobilePin = 0;
bool gMobileNoticeShown = false;

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
#pad{display:none;position:fixed;inset:0;background:radial-gradient(circle at center,#183321,#07100b)}
.top{position:absolute;left:0;right:0;top:7px;text-align:center;font-size:13px}
.stick{position:absolute;width:31vw;height:31vw;max-width:220px;max-height:220px;border:3px solid #4dbd68;border-radius:50%;background:#0b1c11aa}
#ls{left:5vw;bottom:8vh}#rs{right:31vw;bottom:8vh}.nub{position:absolute;width:38%;height:38%;left:31%;top:31%;border-radius:50%;background:#68dd82}
.btn{position:absolute;border-radius:50%;width:14vw;height:14vw;max-width:88px;max-height:88px;padding:0;background:#173923cc}
#ba{right:5vw;bottom:18vh}#bb{right:17vw;bottom:7vh}#bx{right:17vw;bottom:29vh}#by{right:29vw;bottom:18vh}
.small{width:11vw;height:9vw;max-width:74px;max-height:54px;border-radius:10px}
#lb{left:4vw;top:7vh}#lt{left:17vw;top:7vh}#rb{right:17vw;top:7vh}#rt{right:4vw;top:7vh}
#back{left:42vw;top:12vh}#start{right:42vw;top:12vh}
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
<div id="pad"><div class="top" id="top">PHOBOI CONTROLLER</div>
<button class="small" id="lb">LB</button><button class="small" id="lt">LT</button>
<button class="small" id="rb">RB</button><button class="small" id="rt">RT</button>
<button class="small" id="back">BACK</button><button class="small" id="start">START</button>
<div class="stick" id="ls"><div class="nub"></div></div><div class="stick" id="rs"><div class="nub"></div></div>
<button class="btn" id="ba">A</button><button class="btn" id="bb">B</button><button class="btn" id="bx">X</button><button class="btn" id="by">Y</button>
<div class="dpad"><button class="du" id="du">▲</button><button class="dl" id="dl">◀</button><button class="dr" id="dr">▶</button><button class="dd" id="dd">▼</button></div>
</div>
<script>
let slot=-1,token=0,buttons=0,axes=[0,0,0,0,-32768,-32768],sending=false;
const $=id=>document.getElementById(id);
function bindButton(id,bit,axis){
 const e=$(id); const down=ev=>{ev.preventDefault();e.setPointerCapture?.(ev.pointerId);if(axis!==undefined)axes[axis]=32767;else buttons|=(1<<bit)};
 const up=ev=>{ev.preventDefault();if(axis!==undefined)axes[axis]=-32768;else buttons&=~(1<<bit)};
 e.addEventListener('pointerdown',down);e.addEventListener('pointerup',up);e.addEventListener('pointercancel',up);e.addEventListener('pointerleave',up);
}
[['ba',0],['bb',1],['bx',2],['by',3],['back',4],['start',6],['lb',9],['rb',10],['du',11],['dd',12],['dl',13],['dr',14]].forEach(x=>bindButton(x[0],x[1]));
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
 slot=j.slotIndex;token=j.token;$('join').style.display='none';$('pad').style.display='block';$('top').textContent=`PLAYER ${j.player} — CONNECTED`;
 if(document.documentElement.requestFullscreen)document.documentElement.requestFullscreen().catch(()=>{});
 if(screen.orientation?.lock)screen.orientation.lock('landscape').catch(()=>{});
}catch(e){$('msg').textContent=e.message}};
async function send(){if(slot<0||sending)return;sending=true;try{const body=new URLSearchParams({slot,token,lx:axes[0],ly:axes[1],rx:axes[2],ry:axes[3],lt:axes[4],rt:axes[5],buttons});await fetch('/input',{method:'POST',body,keepalive:true})}catch(e){}sending=false}
setInterval(send,33);addEventListener('pagehide',()=>{if(slot>=0)navigator.sendBeacon('/release',new URLSearchParams({slot,token}))});
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

        mobileHandleClient(client);
        mobileCloseSocket(client);
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

    for (int axis = 0; axis < SDL_CONTROLLER_AXIS_MAX; axis++) {
        int value = state.axes[axis].load();
        value = std::max(static_cast<int>(SDL_JOYSTICK_AXIS_MIN), std::min(static_cast<int>(SDL_JOYSTICK_AXIS_MAX), value));
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
        if (state.claimed.load() && inputAge > 250) {
            // A lost packet must never leave movement, aiming, or an attack held.
            // Keep the player's reservation for ten seconds, but neutralize the
            // virtual pad almost immediately.
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

void localCoopMobileShutdown()
{
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

    for (int slot = 1; slot < kLocalCoopMaxPlayers; slot++) {
        mobileDetachController(slot);
    }

#ifdef _WIN32
    WSACleanup();
#endif
}

} // namespace fallout
