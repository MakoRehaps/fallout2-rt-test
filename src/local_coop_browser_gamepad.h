#pragma once

// Browser standard Gamepad API -> existing PhoBoi SDL gamepad transport.
// No native XInput DLL is required inside a browser. Xbox-compatible pads are
// normally exposed with mapping="standard" on supported browsers.
namespace fallout {
inline const char* localCoopBrowserGamepadScript()
{
    return R"PHOBOIPAD(
(function() {
  'use strict';
  // PHOBOI_BROWSER_STANDARD_GAMEPAD_V1
  const bitMap = [
    [0,0], [1,1], [2,2], [3,3],
    [4,9], [5,10],
    [8,4], [9,6], [10,7], [11,8],
    [12,11], [13,12], [14,13], [15,14], [16,5]
  ];
  let selectedIndex = -1, active = false;
  let previousLeft = false, previousRight = false, previousA = false;

  function connectedPads() {
    if (typeof navigator.getGamepads !== 'function') return [];
    try {
      return Array.from(navigator.getGamepads() || []).filter(
        p => p && p.connected && p.mapping === 'standard');
    } catch (_) { return []; }
  }

  function choosePad() {
    const pads = connectedPads();
    return pads.find(p => p.index === selectedIndex) || pads[0] || null;
  }

  function updateMode(isActive) {
    if (active === isActive) return;
    active = isActive;
    window.phoboiBrowserGamepadActive = isActive;
    document.body.classList.toggle('phoboi-browser-gamepad', isActive);
    if (typeof updateStatus === 'function' && typeof slot === 'number' && slot >= 0) {
      updateStatus();
    }
  }

  function stickValue(value) {
    const n = Number.isFinite(value) ? Math.max(-1, Math.min(1, value)) : 0;
    return Math.abs(n) < 0.12 ? 0 : Math.round(n < 0 ? n * 32768 : n * 32767);
  }

  function triggerValue(button) {
    const n = button && Number.isFinite(button.value) ? button.value : 0;
    return Math.round(Math.max(0, Math.min(1, n)) * 65535) - 32768;
  }

  window.phoboiBrowserGamepadRead = function() {
    const pad = choosePad();
    updateMode(!!pad);
    if (!pad) return null; // Keep existing on-screen touch controls available.
    const pressed = n => !!(pad.buttons[n] && pad.buttons[n].pressed);
    let mappedButtons = 0;
    for (const [web, sdl] of bitMap) {
      if (pressed(web)) mappedButtons |= (1 << sdl);
    }
    const axes = [
      stickValue(pad.axes[0]), stickValue(pad.axes[1]),
      stickValue(pad.axes[2]), stickValue(pad.axes[3]),
      triggerValue(pad.buttons[6]), triggerValue(pad.buttons[7])
    ];
    return { axes, buttons: mappedButtons >>> 0 };
  };

  // A + D-pad left/right can join P2-P4 without a keyboard or mouse.
  // Existing PIN auto-fill still works when opening a host-generated URL.
  function joinWithController() {
    if (typeof slot !== 'number' || slot >= 0) return;
    const pad = choosePad();
    const pressed = i => !!(pad && pad.buttons[i] && pad.buttons[i].pressed);
    const left = pressed(14), right = pressed(15), a = pressed(0);
    if (!pad) {
      previousLeft = previousRight = previousA = false;
      return;
    }
    const slotPicker = document.getElementById('slot');
    if (slotPicker && !slotPicker.disabled) {
      const selected = Math.max(0, Number(slotPicker.value) - 1);
      if (left && !previousLeft) slotPicker.value = String(Math.max(0, selected - 1) + 1);
      if (right && !previousRight) slotPicker.value = String(Math.min(2, selected + 1) + 1);
    }
    const connect = document.getElementById('connect');
    if (a && !previousA && connect && !connect.disabled) connect.click();
    previousLeft = left;
    previousRight = right;
    previousA = a;
  }

  window.addEventListener('gamepaddisconnected', function(event) {
    if (event.gamepad && event.gamepad.index === selectedIndex) selectedIndex = -1;
    if (!choosePad()) updateMode(false);
  });
  window.addEventListener('gamepadconnected', function(event) {
    if (selectedIndex < 0) selectedIndex = event.gamepad.index;
  });
  setInterval(joinWithController, 80);
})();
)PHOBOIPAD";
}
} // namespace fallout
