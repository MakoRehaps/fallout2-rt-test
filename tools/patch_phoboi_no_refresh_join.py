from pathlib import Path

path = Path("src/local_coop_mobile.cc")
text = path.read_text(encoding="utf-8")

marker = "PHOBOI_NO_REFRESH_JOIN_V1"
if marker in text:
    print("PhoBoi no-refresh join already applied")
    raise SystemExit(0)

old_connect = r'''bindStick('ls',0,1);bindStick('rs',2,3);
$('connect').onclick=async()=>{try{const body=new URLSearchParams({slot:$('slot').value,pin:$('pin').value});
 const r=await fetch('/claim',{method:'POST',body});const j=await r.json();if(!j.ok)throw new Error(j.error||'Unable to connect');
 slot=j.slotIndex;token=j.token;sessionStorage.setItem('phoboiSession',JSON.stringify({slot,token}));$('join').style.display='none';$('pad').style.display='block';controlMode='STARTING';videoMode='STARTING';updateStatus();openSocket();openStream();
 if(document.documentElement.requestFullscreen)document.documentElement.requestFullscreen().catch(()=>{});
 if(screen.orientation?.lock)screen.orientation.lock('landscape').catch(()=>{});
}catch(e){$('msg').textContent=e.message}};
let ws=null,controlTimer=null,controlAttempt=0,controlMode='STARTING';
'''

new_connect = r'''bindStick('ls',0,1);bindStick('rs',2,3);
// PHOBOI_NO_REFRESH_JOIN_V1
// Claiming a shared browser/phone link is two-phase: reserve the slot, then wait
// until the game thread has actually attached the SDL virtual controller. The old
// page jumped straight to WS/video and often only worked after a manual refresh.
const sleep=ms=>new Promise(resolve=>setTimeout(resolve,ms));
async function waitForControllerReady(){
 while(slot>=0&&token){
  try{
   const r=await fetch(`/ready?slot=${slot}&token=${token}&nonce=${Date.now()}`,{cache:'no-store'});
   if(r.status===403)throw new Error('Session expired');
   if(r.ok){const j=await r.json();if(j.ok&&j.ready)return true}
  }catch(e){if(String(e&&e.message||e).includes('expired'))throw e}
  $('msg').textContent=`PLAYER ${slot+1} CONNECTED - JOINING CURRENT GAME...`;
  await sleep(125);
 }
 return false;
}
function enterControllerPage(){
 $('join').style.display='none';$('pad').style.display='block';
 controlMode='STARTING';videoMode='STARTING';updateStatus();openSocket();openStream();
}
$('connect').onclick=async()=>{const button=$('connect');button.disabled=true;try{
 $('msg').textContent='CONNECTING TO CURRENT GAME...';
 const body=new URLSearchParams({slot:$('slot').value,pin:$('pin').value});
 const r=await fetch('/claim',{method:'POST',body,cache:'no-store'});const j=await r.json();if(!j.ok)throw new Error(j.error||'Unable to connect');
 slot=j.slotIndex;token=j.token;sessionStorage.setItem('phoboiSession',JSON.stringify({slot,token}));
 await waitForControllerReady();enterControllerPage();
 if(document.documentElement.requestFullscreen)document.documentElement.requestFullscreen().catch(()=>{});
 if(screen.orientation?.lock)screen.orientation.lock('landscape').catch(()=>{});
}catch(e){$('msg').textContent=e.message||String(e);button.disabled=false}};
let ws=null,controlTimer=null,controlAttempt=0,controlMode='STARTING';
'''

if old_connect not in text:
    raise SystemExit("PhoBoi client join anchor not found")
text = text.replace(old_connect, new_connect, 1)

old_resume = r'''(async()=>{try{
 const saved=JSON.parse(sessionStorage.getItem('phoboiSession')||'null');if(!saved)return;
 const r=await fetch(`/resume?slot=${saved.slot}&token=${saved.token}`,{cache:'no-store'});const j=await r.json();if(!j.ok)return;
 slot=saved.slot;token=saved.token;$('join').style.display='none';$('pad').style.display='block';controlMode='STARTING';videoMode='STARTING';updateStatus();openSocket();openStream();
}catch(e){}})();
'''
new_resume = r'''(async()=>{try{
 const saved=JSON.parse(sessionStorage.getItem('phoboiSession')||'null');if(!saved)return;
 const r=await fetch(`/resume?slot=${saved.slot}&token=${saved.token}`,{cache:'no-store'});const j=await r.json();if(!j.ok)return;
 slot=saved.slot;token=saved.token;$('msg').textContent=`PLAYER ${slot+1} - RESTORING CONTROLLER...`;
 await waitForControllerReady();enterControllerPage();
}catch(e){sessionStorage.removeItem('phoboiSession');slot=-1;token=0;$('join').style.display='flex';$('pad').style.display='none';$('msg').textContent=e.message||String(e)}})();
'''
if old_resume not in text:
    raise SystemExit("PhoBoi resume anchor not found")
text = text.replace(old_resume, new_resume, 1)

old_server = r'''    MobileSlotState& state = gMobileSlots[slot];

    if (method == "GET" && route == "/resume") {
'''
new_server = r'''    MobileSlotState& state = gMobileSlots[slot];

    // PHOBOI_NO_REFRESH_JOIN_V1
    // The browser must not guess when the game thread has finished creating its
    // SDL virtual controller. Polling this endpoint also keeps the reserved slot
    // alive while a modal/slow frame delays the attach by a few ticks.
    if (method == "GET" && route == "/ready") {
        uint32_t token = mobileUnsignedValue(values, "token", 0);
        if (!state.claimed.load() || token == 0 || token != state.token.load()) {
            mobileSendResponse(client, "403 Forbidden", "application/json", "{\\\"ok\\\":false,\\\"error\\\":\\\"Session expired\\\"}");
            return;
        }
        state.lastSeen.store(mobileNow());
        const MobileVirtualDevice& device = gMobileDevices[slot];
        const LocalCoopPlayer& player = gLocalCoopPlayers[slot];
        bool ready = device.deviceIndex >= 0
            && device.controller != nullptr
            && player.connected
            && player.controller == device.controller;
        std::ostringstream body;
        body << "{\\\"ok\\\":true,\\\"ready\\\":" << (ready ? "true" : "false")
             << ",\\\"player\\\":" << slot + 1 << "}";
        mobileSendResponse(client, "200 OK", "application/json", body.str());
        return;
    }

    if (method == "GET" && route == "/resume") {
'''
if old_server not in text:
    raise SystemExit("PhoBoi server ready anchor not found")
text = text.replace(old_server, new_server, 1)

path.write_text(text, encoding="utf-8")
print("Applied PhoBoi no-refresh join + controller-ready handshake")
