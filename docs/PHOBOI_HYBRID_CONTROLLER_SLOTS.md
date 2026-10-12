# PhoBoi Hybrid Controller Slots (P1–P4)

This branch implements physical SDL/XInput-style controllers and browser gamepads as interchangeable input sources for the **same four character slots**, with the existing single-host game and Cloudflare transport.

## Join
1. P1 uses a controller connected to the host PC; only P2–P4 can join through PhoBoi.
2. Host opens PhoBoi with **F11**, then starts Cloudflare with **T** and shares the HTTPS link / session PIN. The local pairing URL works on the same LAN.
3. Browser player selects P2, P3, or P4, enters the PIN, and selects CONNECT (or uses the browser gamepad: D-pad left/right to select P2–P4 and **A** to connect).
4. With an Xbox-style standard-mapped controller connected to the browser device, the **Gamepad API** supplies inputs. Otherwise PhoBoi retains its original on-screen touch controls. Remote video and controls reuse PhoBoi's WebSocket and fallback HTTP endpoints. Browsers generally require a secure context for gamepads, so use the Cloudflare HTTPS link or localhost.

## Handoff
- **Local -> Browser**: disconnect the local P2–P4 controller; their character slot stays locked. Connect through PhoBoi and select that same P number.
- **Browser -> Local**: click **LEAVE / FREE SLOT** in PhoBoi (or have the host kick that phone slot). Reconnect the local controller. A saved controller with a unique serial can reclaim its prior character, while a different replacement controller reclaims the only disconnected reserved P2–P4 character when unambiguous.
- There are **4 players total**, not 4 physical plus remote. P1 remains local-only. Only one active input device owns each P2–P4 slot.
- Handoff never spawns an extra player, recreates a character, erases stats/equipment, or forcibly teleports an existing actor. Saves retain their original slot identities.
- A browser cannot steal a slot still controlled by a physical controller. Freeing an existing slot is an explicit user/host action.
- If multiple identical controllers have no distinct hardware serials and multiple reserved slots are simultaneously empty, order may be ambiguous. Reconnect one player at a time to preserve assignments.

## Implementation
- `src/local_coop_browser_gamepad.h`: standard browser gamepad mapping to PhoBoi packet format.
- `src/local_coop_mobile.cc`: browser script endpoint, touch fallback, claim/release, SDL virtual controller, Cloudflare transport.
- `src/local_coop.h`: physical device affinity and reconnect scanning with character slots retained.
- `.github/workflows/phoboi-hybrid-controller-build.yml`: branch-specific Windows build plus source invariants.

## Validation caveat
A successful CI build proves compilation, not physical controller, Cloudflare edge routing, frame stream smoothness, or session security on a real network. Test a real P2->browser->P2 handoff and simultaneous P1–P4 input before distributing a release.
