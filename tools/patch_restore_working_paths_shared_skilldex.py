#!/usr/bin/env python3
from pathlib import Path
import re


def read(path):
    return Path(path).read_text(encoding="utf-8")


def write(path, text):
    Path(path).write_text(text, encoding="utf-8")


# ---------------------------------------------------------------------------
# 1. Shared co-op state.
#    Pip-Boy used to work through Fallout's established path. The later direct
#    call from inside the co-op ticker was the regression. Queue only an OPEN
#    request in the ticker, then call the stock backend after the ticker returns.
# ---------------------------------------------------------------------------
path = "src/local_coop.h"
text = read(path)

if "COOP_SHARED_SKILLDEX_MODE_V1" not in text:
    old = '''enum class LocalCoopUiMode {
    World,
    Inventory,
    Character,
'''
    new = '''enum class LocalCoopUiMode {
    World,
    Inventory,
    // COOP_SHARED_SKILLDEX_MODE_V1
    Skilldex,
    Character,
'''
    if old not in text:
        raise SystemExit("LocalCoopUiMode anchor missing")
    text = text.replace(old, new, 1)

if "COOP_PIPBOY_DEFERRED_DIRECT_RESTORE_V1" not in text:
    anchor = 'inline bool gLocalCoopSystemMenuActive = false;\n'
    if anchor not in text:
        raise SystemExit("Pip-Boy request state anchor missing")
    addition = '''
// COOP_PIPBOY_DEFERRED_DIRECT_RESTORE_V1
// The controller ticker must never enter a blocking Fallout modal directly.
// It records the old working Pip-Boy intent; mainLoop consumes it immediately
// after localCoopRuntimeTick returns, bypassing the hidden stock interface-bar
// hotkey gate while still using the real phoboiOpen backend.
inline bool gLocalCoopPipBoyOpenRequested = false;

inline void localCoopRequestPipBoyOpen()
{
    gLocalCoopPipBoyOpenRequested = true;
}

inline bool localCoopTakePipBoyOpenRequest()
{
    bool requested = gLocalCoopPipBoyOpenRequested;
    gLocalCoopPipBoyOpenRequested = false;
    return requested;
}
'''
    text = text.replace(anchor, anchor + addition, 1)

write(path, text)


# ---------------------------------------------------------------------------
# 2. Shared Skilldex UI.
#    This is analogous to the shared bag: one party resource/view rather than a
#    stock gDude-only blocking Skilldex. Any player can open it. It displays all
#    four characters' values and the opener may select which party member uses
#    the selected skill. The chosen operator's own focus target is authoritative.
# ---------------------------------------------------------------------------
path = "src/local_coop_personal_ui.h"
text = read(path)

if "COOP_SHARED_SKILLDEX_V1" not in text:
    include_anchor = '#include <array>\n'
    if include_anchor not in text:
        raise SystemExit("personal UI include anchor missing")
    text = text.replace(
        include_anchor,
        include_anchor
        + '#include "actions.h"\n'
        + '#include "local_coop_focus.h"\n'
        + '#include "skill.h"\n',
        1,
    )

    state_anchor = '''    int inventoryWindow = -1;
    int selectedItem = 0;
'''
    state_new = '''    int inventoryWindow = -1;
    // COOP_SHARED_SKILLDEX_V1
    int skilldexWindow = -1;
    int selectedSkill = 0;
    int skillUserSlot = 0;
    int selectedItem = 0;
'''
    if state_anchor not in text:
        raise SystemExit("personal UI state anchor missing")
    text = text.replace(state_anchor, state_new, 1)

    inventory_rect_anchor = '''inline void localCoopPersonalUiCloseInventory(int slot)
{
'''
    if inventory_rect_anchor not in text:
        raise SystemExit("inventory close anchor missing")

    shared_skilldex = r'''// COOP_SHARED_SKILLDEX_V1
inline constexpr int kLocalCoopSharedSkilldexSkillCount = 8;
inline constexpr int kLocalCoopSharedSkilldexSkills[kLocalCoopSharedSkilldexSkillCount] = {
    SKILL_SNEAK,
    SKILL_LOCKPICK,
    SKILL_STEAL,
    SKILL_TRAPS,
    SKILL_FIRST_AID,
    SKILL_DOCTOR,
    SKILL_SCIENCE,
    SKILL_REPAIR,
};
inline constexpr const char* kLocalCoopSharedSkilldexNames[kLocalCoopSharedSkilldexSkillCount] = {
    "SNEAK",
    "LOCKPICK",
    "STEAL",
    "TRAPS",
    "FIRST AID",
    "DOCTOR",
    "SCIENCE",
    "REPAIR",
};

inline void localCoopPersonalUiSkilldexRect(int& x, int& y, int& width, int& height)
{
    int sw = std::max(640, screenGetWidth());
    int sh = std::max(480, screenGetHeight());
    width = std::min(sw - 16, std::max(560, (sw * 3) / 4));
    height = std::min(sh - 24, 390);
    x = (sw - width) / 2;
    y = (sh - height) / 2;
}

inline void localCoopPersonalUiCloseSkilldex(int slot)
{
    if (slot < 0 || slot >= kLocalCoopMaxPlayers) return;
    auto& ui = gLocalCoopPersonalUi[slot];
    if (ui.skilldexWindow != -1) windowDestroy(ui.skilldexWindow);
    ui.skilldexWindow = -1;
    if (gLocalCoopPlayers[slot].uiMode == LocalCoopUiMode::Skilldex) {
        gLocalCoopPlayers[slot].uiMode = LocalCoopUiMode::World;
    }
}

inline bool localCoopSharedSkilldexSlotUsable(int slot)
{
    return slot >= 0
        && slot < kLocalCoopMaxPlayers
        && gLocalCoopPlayers[slot].slotLocked
        && gLocalCoopPlayers[slot].actor != nullptr;
}

inline int localCoopSharedSkilldexNextUser(int current, int direction)
{
    for (int step = 1; step <= kLocalCoopMaxPlayers; step++) {
        int candidate = (current + direction * step) % kLocalCoopMaxPlayers;
        if (candidate < 0) candidate += kLocalCoopMaxPlayers;
        if (localCoopSharedSkilldexSlotUsable(candidate)) return candidate;
    }
    return current;
}

inline void localCoopPersonalUiDrawSkilldex(int slot)
{
    auto& ui = gLocalCoopPersonalUi[slot];
    if (ui.skilldexWindow == -1) return;

    int w = windowGetWidth(ui.skilldexWindow);
    int h = windowGetHeight(ui.skilldexWindow);
    windowFill(ui.skilldexWindow, 0, 0, w, h, _colorTable[0]);
    windowDrawBorder(ui.skilldexWindow);

    char line[256];
    snprintf(line, sizeof(line), "SHARED PARTY SKILLDEX  |  OPENED BY P%d  |  USE P%d",
        slot + 1, ui.skillUserSlot + 1);
    windowDrawText(ui.skilldexWindow, line, w - 24, 12, 10, _colorTable[992]);
    windowDrawText(ui.skilldexWindow,
        "UP/DOWN SKILL  LEFT/RIGHT PARTY MEMBER  A USE  B/BACK CLOSE",
        w - 24, 12, 31, _colorTable[992]);

    const int nameX = 22;
    int valueStartX = std::max(210, w - 360);
    int valueSpacing = std::max(70, (w - valueStartX - 20) / kLocalCoopMaxPlayers);
    for (int playerSlot = 0; playerSlot < kLocalCoopMaxPlayers; playerSlot++) {
        snprintf(line, sizeof(line), "%cP%d%c",
            playerSlot == ui.skillUserSlot ? '[' : ' ',
            playerSlot + 1,
            playerSlot == ui.skillUserSlot ? ']' : ' ');
        windowDrawText(ui.skilldexWindow, line, valueSpacing - 4,
            valueStartX + playerSlot * valueSpacing, 58,
            _colorTable[playerSlot == ui.skillUserSlot ? 32747 : 992]);
    }

    int y = 86;
    for (int index = 0; index < kLocalCoopSharedSkilldexSkillCount; index++, y += 32) {
        snprintf(line, sizeof(line), "%c %-10s",
            index == ui.selectedSkill ? '>' : ' ',
            kLocalCoopSharedSkilldexNames[index]);
        windowDrawText(ui.skilldexWindow, line, valueStartX - nameX - 8,
            nameX, y, _colorTable[index == ui.selectedSkill ? 32747 : 992]);

        for (int playerSlot = 0; playerSlot < kLocalCoopMaxPlayers; playerSlot++) {
            Object* actor = gLocalCoopPlayers[playerSlot].actor;
            if (!gLocalCoopPlayers[playerSlot].slotLocked || actor == nullptr) {
                snprintf(line, sizeof(line), "--");
            } else {
                snprintf(line, sizeof(line), "%d",
                    skillGetValue(actor, kLocalCoopSharedSkilldexSkills[index]));
            }
            windowDrawText(ui.skilldexWindow, line, valueSpacing - 4,
                valueStartX + playerSlot * valueSpacing, y,
                _colorTable[playerSlot == ui.skillUserSlot ? 32747 : 992]);
        }
    }

    snprintf(line, sizeof(line), "SELECTED: %s / P%d",
        kLocalCoopSharedSkilldexNames[ui.selectedSkill], ui.skillUserSlot + 1);
    windowDrawText(ui.skilldexWindow, line, w - 24, 12, h - 28, _colorTable[992]);
    windowRefresh(ui.skilldexWindow);
}

inline void localCoopPersonalUiOpenSharedSkilldex(int slot)
{
    if (slot < 0 || slot >= kLocalCoopMaxPlayers) return;
    auto& player = gLocalCoopPlayers[slot];
    auto& ui = gLocalCoopPersonalUi[slot];
    if (!player.connected || !player.humanOwned || player.actor == nullptr) return;

    // Only one party Skilldex is visible at a time because it describes the
    // same shared party data regardless of which controller opened it.
    for (int other = 0; other < kLocalCoopMaxPlayers; other++) {
        if (gLocalCoopPersonalUi[other].skilldexWindow != -1) {
            localCoopPersonalUiCloseSkilldex(other);
        }
    }
    if (ui.inventoryWindow != -1) localCoopPersonalUiCloseInventory(slot);

    int x, y, w, h;
    localCoopPersonalUiSkilldexRect(x, y, w, h);
    ui.skilldexWindow = windowCreate(x, y, w, h, _colorTable[0], WINDOW_MOVE_ON_TOP);
    if (ui.skilldexWindow == -1) return;
    ui.selectedSkill = std::clamp(ui.selectedSkill, 0, kLocalCoopSharedSkilldexSkillCount - 1);
    ui.skillUserSlot = localCoopSharedSkilldexSlotUsable(slot)
        ? slot
        : localCoopSharedSkilldexNextUser(0, 1);
    player.uiMode = LocalCoopUiMode::Skilldex;
    localCoopPersonalUiDrawSkilldex(slot);
}

inline void localCoopPersonalUiUseSharedSkill(int ownerSlot)
{
    auto& ui = gLocalCoopPersonalUi[ownerSlot];
    int operatorSlot = ui.skillUserSlot;
    if (!localCoopSharedSkilldexSlotUsable(operatorSlot)) return;

    LocalCoopPlayer& operatorPlayer = gLocalCoopPlayers[operatorSlot];
    Object* actor = operatorPlayer.actor;
    int skill = kLocalCoopSharedSkilldexSkills[ui.selectedSkill];
    Object* target = localCoopFocusFindInteractable(operatorPlayer);
    int rc = -1;

    if (skill == SKILL_SNEAK) {
        if (actor == gDude) {
            rc = _action_skill_use(SKILL_SNEAK);
        } else {
            operatorPlayer.sneaking = !operatorPlayer.sneaking;
            rc = 0;
        }
    } else {
        if ((skill == SKILL_FIRST_AID || skill == SKILL_DOCTOR)
            && (target == nullptr
                || PID_TYPE(target->pid) != OBJ_TYPE_CRITTER
                || (target->data.critter.combat.results & DAM_DEAD) != 0
                || localCoopFocusIsEnemy(actor, target))) {
            target = actor;
        }
        if (target != nullptr) {
            rc = actionUseSkill(actor, target, skill);
        }
    }

    debugPrint("[COOP SHARED SKILLDEX] owner=%d operator=%d skill=%d targetId=%d rc=%d\n",
        ownerSlot,
        operatorSlot,
        skill,
        target != nullptr ? target->id : -1,
        rc);
    localCoopPersonalUiCloseSkilldex(ownerSlot);
}

'''
    text = text.replace(inventory_rect_anchor, shared_skilldex + inventory_rect_anchor, 1)

    # Disconnect cleanup: close either personal overlay belonging to the phone/controller.
    old = '''        if (!player.connected || !player.humanOwned || player.controller == nullptr) {
            if (ui.inventoryWindow != -1) localCoopPersonalUiCloseInventory(slot);
            continue;
        }
'''
    new = '''        if (!player.connected || !player.humanOwned || player.controller == nullptr) {
            if (ui.inventoryWindow != -1) localCoopPersonalUiCloseInventory(slot);
            if (ui.skilldexWindow != -1) localCoopPersonalUiCloseSkilldex(slot);
            continue;
        }
'''
    if old not in text:
        raise SystemExit("personal UI disconnect cleanup anchor missing")
    text = text.replace(old, new, 1)

    old = '''        if (back && !ui.backWasDown) {
            if (ui.inventoryWindow == -1 && player.uiMode == LocalCoopUiMode::World) localCoopPersonalUiOpenInventory(slot);
            else if (ui.inventoryWindow != -1) localCoopPersonalUiCloseInventory(slot);
        }

        if (ui.inventoryWindow != -1) {
'''
    new = '''        if (back && !ui.backWasDown) {
            if (ui.skilldexWindow != -1) {
                localCoopPersonalUiCloseSkilldex(slot);
            } else if (ui.inventoryWindow == -1 && player.uiMode == LocalCoopUiMode::World) {
                localCoopPersonalUiOpenInventory(slot);
            } else if (ui.inventoryWindow != -1) {
                localCoopPersonalUiCloseInventory(slot);
            }
        }

        if (ui.skilldexWindow != -1) {
            if (b && !ui.bWasDown) {
                localCoopPersonalUiCloseSkilldex(slot);
            } else {
                if (up && !ui.upWasDown) {
                    ui.selectedSkill = (ui.selectedSkill + kLocalCoopSharedSkilldexSkillCount - 1)
                        % kLocalCoopSharedSkilldexSkillCount;
                }
                if (down && !ui.downWasDown) {
                    ui.selectedSkill = (ui.selectedSkill + 1) % kLocalCoopSharedSkilldexSkillCount;
                }
                if (left && !ui.leftWasDown) {
                    ui.skillUserSlot = localCoopSharedSkilldexNextUser(ui.skillUserSlot, -1);
                }
                if (right && !ui.rightWasDown) {
                    ui.skillUserSlot = localCoopSharedSkilldexNextUser(ui.skillUserSlot, 1);
                }
                if (a && !ui.aWasDown) {
                    localCoopPersonalUiUseSharedSkill(slot);
                } else {
                    localCoopPersonalUiDrawSkilldex(slot);
                }
            }
        } else if (ui.inventoryWindow != -1) {
'''
    if old not in text:
        raise SystemExit("personal UI Back/inventory branch anchor missing")
    text = text.replace(old, new, 1)

    old = '''        if (ui.hudWindow != -1) windowDestroy(ui.hudWindow);
        if (ui.inventoryWindow != -1) windowDestroy(ui.inventoryWindow);
        ui = LocalCoopPersonalUiState {};
'''
    new = '''        if (ui.hudWindow != -1) windowDestroy(ui.hudWindow);
        if (ui.inventoryWindow != -1) windowDestroy(ui.inventoryWindow);
        if (ui.skilldexWindow != -1) windowDestroy(ui.skilldexWindow);
        ui = LocalCoopPersonalUiState {};
'''
    if old not in text:
        raise SystemExit("personal UI shutdown anchor missing")
    text = text.replace(old, new, 1)

write(path, text)


# ---------------------------------------------------------------------------
# 3. Restore the known-working controller semantics after the regression patch.
#    D-pad-left closes an existing Pip-Boy with Escape just as before. Opening is
#    deferred only long enough to leave the ticker, then mainLoop calls phoboiOpen.
#    Right-stick now opens the shared co-op Skilldex instead of stock gDude UI.
# ---------------------------------------------------------------------------
path = "src/local_coop_runtime.h"
text = read(path)
if "COOP_PIPBOY_WORKING_PATH_RESTORE_V1" not in text:
    pattern = re.compile(
        r'''        // COOP_P1_DIRECT_PIPBOY_HOTKEY_V1\n.*?        \} else if \(canOpen && skilldexDown && !runtime\.skilldexWasDown\) \{\n.*?            debugPrint\("\[COOP SKILLDEX\].*?\n        \} else if \(canOpen && slot == 0 && startDown''',
        re.S,
    )
    match = pattern.search(text)
    if match is None:
        raise SystemExit("generated direct Pip-Boy/Skilldex runtime block not found")
    replacement = r'''        // COOP_P1_DIRECT_PIPBOY_HOTKEY_V1 compatibility marker
        // COOP_PIPBOY_WORKING_PATH_RESTORE_V1
        // Restore the old proven D-pad-left behavior. Closing stays an Escape
        // event inside the active stock modal. Opening is requested here and
        // dispatched from mainLoop only after this ticker has returned.
        if (p1PipboyEdge && pipboyModalActive) {
            gLocalCoopModalControllerSlot = 0;
            enqueueInputEvent(KEY_ESCAPE);
            debugPrint("[PHOBOI INPUT] slot=0 global-ui=pipboy action=close-restored\n");
        } else if (canOwnGlobalUi && p1PipboyEdge) {
            gLocalCoopModalControllerSlot = 0;
            localCoopRequestPipBoyOpen();
            modalActive = true;
            debugPrint("[PHOBOI INPUT] slot=0 global-ui=pipboy action=open-restored\n");
        } else if (canOpen && skilldexDown && !runtime.skilldexWasDown) {
            // COOP_SHARED_SKILLDEX_RUNTIME_V1
            gLocalCoopModalControllerSlot = slot;
            localCoopPersonalUiOpenSharedSkilldex(slot);
            modalActive = gLocalCoopPlayers[slot].uiMode == LocalCoopUiMode::Skilldex;
            debugPrint("[COOP SHARED SKILLDEX] slot=%d source=controller button=right-stick\n", slot);
        } else if (canOpen && slot == 0 && startDown'''
    text = text[:match.start()] + replacement + text[match.end():]
write(path, text)


# ---------------------------------------------------------------------------
# 4. P1 system menu uses the same restored Pip-Boy request and shared Skilldex.
# ---------------------------------------------------------------------------
path = "src/local_coop_system_menu.h"
text = read(path)
if "COOP_SHARED_SKILLDEX_SYSTEM_MENU_V1" not in text:
    old = '''    case LocalCoopSystemMenuAction::PipBoy:
        // COOP_P1_DIRECT_PIPBOY_V1
        // Pip-Boy is a P1-only global device. Call it directly because the
        // controller-owned main loop intentionally discards keyboard letters.
        gLocalCoopModalControllerSlot = 0;
        phoboiOpen(PIPBOY_OPEN_INTENT_WORLD_MAP);
        break;
    case LocalCoopSystemMenuAction::Skilldex:
        gLocalCoopSkilldexInvokerSlot = 0;
        gLocalCoopModalControllerSlot = 0;
        skilldexOpen();
        break;
'''
    new = '''    case LocalCoopSystemMenuAction::PipBoy:
        // COOP_P1_DIRECT_PIPBOY_V1 compatibility marker
        // COOP_PIPBOY_SYSTEM_MENU_RESTORE_V1
        gLocalCoopModalControllerSlot = 0;
        localCoopRequestPipBoyOpen();
        break;
    case LocalCoopSystemMenuAction::Skilldex:
        // COOP_SHARED_SKILLDEX_SYSTEM_MENU_V1
        gLocalCoopModalControllerSlot = 0;
        localCoopPersonalUiOpenSharedSkilldex(0);
        break;
'''
    if old not in text:
        raise SystemExit("generated P1 system-menu Pip-Boy/Skilldex block not found")
    text = text.replace(old, new, 1)
write(path, text)


# ---------------------------------------------------------------------------
# 5. Consume Pip-Boy request in mainLoop after localCoopRuntimeTick returned.
#    This intentionally calls phoboiOpen directly here, not gameHandleKey(P),
#    because the co-op HUD hides the stock interface bar and gameHandleKey's P
#    branch is gated on interfaceBarEnabled().
# ---------------------------------------------------------------------------
path = "src/main.cc"
text = read(path)
if '#include "pipboy.h"' not in text:
    anchor = '#include "preferences.h"\n'
    if anchor not in text:
        raise SystemExit("main.cc Pip-Boy include anchor missing")
    text = text.replace(anchor, anchor + '#include "pipboy.h"\n', 1)

if "COOP_PIPBOY_DEFERRED_DIRECT_CONSUMER_V1" not in text:
    anchor = '''        localCoopRuntimeTick();

        // SFALL: MainLoopHook.'''
    replacement = '''        localCoopRuntimeTick();

        // COOP_PIPBOY_DEFERRED_DIRECT_CONSUMER_V1
        // The old controller intent is now outside the runtime ticker. Use the
        // real backend here so the hidden stock interface bar cannot swallow P.
        if (localCoopTakePipBoyOpenRequest()) {
            gLocalCoopModalControllerSlot = 0;
            phoboiOpen(PIPBOY_OPEN_INTENT_WORLD_MAP);
        }

        // SFALL: MainLoopHook.'''
    if anchor not in text:
        raise SystemExit("mainLoop post-runtime Pip-Boy consumer anchor missing")
    text = text.replace(anchor, replacement, 1)
write(path, text)


# ---------------------------------------------------------------------------
# 6. Restore the exact simple Quick Tunnel launch semantics from the last known
#    working PhoBoi source. Keep R/N reset/new-link controls, but remove all
#    protocol forcing, public self-probe gating and alternate tunnel providers.
# ---------------------------------------------------------------------------
path = "src/local_coop_mobile.cc"
text = read(path)
start = text.find("bool mobileStartCloudflareTunnel()\n{")
stop = text.find("\nvoid mobileStopCloudflareTunnel()\n{", start)
if start == -1 or stop == -1:
    raise SystemExit("Cloudflare start/stop function boundaries not found")

simple_start = r'''// PHOBOI_CLOUDFLARE_WORKING_PATH_RESTORE_V8
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

    // PHOBOI_CLOUDFLARE_SIMPLE_QUICK_TUNNEL_V8
    // This is the same command used by the previously working PhoBoi build.
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
    debugPrint("[PHOBOI MOBILE] Cloudflare original Quick Tunnel path starting\n");
    return true;
}
'''
text = text[:start] + simple_start + text[stop:]

# Never allow the final compiled source to retain the experimental provider.
for forbidden in (
    "localhost.run",
    "--edge-ip-version",
    "--protocol http2",
    "--config",
):
    if forbidden in text:
        raise SystemExit(f"experimental Cloudflare/fallback launch option survived restore: {forbidden}")

write(path, text)


# ---------------------------------------------------------------------------
# Hard regression checks for the final generated source.
# ---------------------------------------------------------------------------
checks = (
    ("src/local_coop.h", "COOP_PIPBOY_DEFERRED_DIRECT_RESTORE_V1"),
    ("src/local_coop.h", "COOP_SHARED_SKILLDEX_MODE_V1"),
    ("src/local_coop_personal_ui.h", "COOP_SHARED_SKILLDEX_V1"),
    ("src/local_coop_runtime.h", "COOP_PIPBOY_WORKING_PATH_RESTORE_V1"),
    ("src/local_coop_runtime.h", "COOP_SHARED_SKILLDEX_RUNTIME_V1"),
    ("src/local_coop_system_menu.h", "COOP_SHARED_SKILLDEX_SYSTEM_MENU_V1"),
    ("src/main.cc", "COOP_PIPBOY_DEFERRED_DIRECT_CONSUMER_V1"),
    ("src/local_coop_mobile.cc", "PHOBOI_CLOUDFLARE_WORKING_PATH_RESTORE_V8"),
    ("src/local_coop_mobile.cc", "PHOBOI_CLOUDFLARE_SIMPLE_QUICK_TUNNEL_V8"),
)
for filename, marker in checks:
    if marker not in read(filename):
        raise SystemExit(f"missing final restore marker {marker} in {filename}")

mobile = read("src/local_coop_mobile.cc")
if 'gCloudflareStatus = "ONLINE";' not in mobile:
    raise SystemExit("restored Cloudflare path does not publish parsed URL immediately")
if 'tunnel --no-autoupdate --url http://127.0.0.1:27888' not in mobile:
    raise SystemExit("restored Cloudflare command differs from known-working command")

print("Restored working Cloudflare/Pip-Boy paths and installed shared party Skilldex")
