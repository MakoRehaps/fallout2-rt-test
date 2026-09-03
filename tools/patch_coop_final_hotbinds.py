from pathlib import Path

p = Path('src/local_coop_runtime.h')
s = p.read_text(encoding='utf-8')
marker = '// COOP_FINAL_HOTBINDS_V1'
if marker in s:
    print('final hotbinds already applied')
    raise SystemExit(0)

s = s.replace('''    bool pipboyWasDown = false;\n    bool skilldexWasDown = false;\n    bool postgameSwitchWasDown = false;''', '''    bool pipboyWasDown = false;\n    bool inventoryWasDown = false;\n    bool startWasDown = false;\n    bool skilldexWasDown = false;\n    bool postgameSwitchWasDown = false;''', 1)

old_switch = '''inline void localCoopProcessPostgameCampaignSwitch()\n{\n    if (!unifiedCampaignIsPostgameFreeRoam()) {\n        return;\n    }'''
new_switch = '''inline void localCoopProcessPostgameCampaignSwitch()\n{\n    // COOP_FINAL_HOTBINDS_V1\n    // Back/Select is now Inventory and Start is the menu. The old Back+Start\n    // postgame campaign switch conflicts with those controls and lineage now\n    // handles the F1 -> F2 transition, so this legacy shortcut stays disabled.\n    return;\n\n    if (!unifiedCampaignIsPostgameFreeRoam()) {\n        return;\n    }'''
if old_switch not in s:
    raise SystemExit('postgame switch function anchor not found')
s = s.replace(old_switch, new_switch, 1)

old_modal = '''        bool backDown = player.controller != nullptr\n            && SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_BACK) != 0;\n        bool startDown = player.controller != nullptr\n            && SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_START) != 0;\n        bool skilldexDown = player.controller != nullptr\n            && SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_RIGHTSTICK) != 0;\n        bool pipboyDown = backDown && !startDown;\n        bool canOpen = !modalActive\n            && player.connected\n            && player.humanOwned\n            && player.controller != nullptr\n            && player.uiMode == LocalCoopUiMode::World\n            && !localCoopDangerBlocksMapExit();\n\n        if (canOpen && pipboyDown && !runtime.pipboyWasDown) {\n            gLocalCoopModalControllerSlot = slot;\n            enqueueInputEvent(KEY_LOWERCASE_P);\n            modalActive = true;\n            debugPrint("[PHOBOI INPUT] slot=%d source=controller button=back\\n", slot);\n        } else if (canOpen && skilldexDown && !runtime.skilldexWasDown) {\n            gLocalCoopModalControllerSlot = slot;\n            gLocalCoopSkilldexInvokerSlot = slot;\n            enqueueInputEvent(KEY_LOWERCASE_S);\n            modalActive = true;\n            debugPrint("[COOP SKILLDEX] slot=%d source=controller button=right-stick\\n", slot);\n        }\n\n        runtime.pipboyWasDown = pipboyDown;\n        runtime.skilldexWasDown = skilldexDown;'''
new_modal = '''        bool backDown = player.controller != nullptr\n            && SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_BACK) != 0;\n        bool startDown = player.controller != nullptr\n            && SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_START) != 0;\n        bool skilldexDown = player.controller != nullptr\n            && SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_RIGHTSTICK) != 0;\n        bool pipboyDown = player.controller != nullptr\n            && SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT) != 0;\n        bool inventoryDown = backDown;\n        bool canOpen = !modalActive\n            && player.connected\n            && player.humanOwned\n            && player.controller != nullptr\n            && player.uiMode == LocalCoopUiMode::World\n            && !localCoopDangerBlocksMapExit();\n\n        if (canOpen && pipboyDown && !runtime.pipboyWasDown) {\n            gLocalCoopModalControllerSlot = slot;\n            enqueueInputEvent(KEY_LOWERCASE_P);\n            modalActive = true;\n            debugPrint("[PHOBOI INPUT] slot=%d source=controller button=dpad-left\\n", slot);\n        } else if (canOpen && inventoryDown && !runtime.inventoryWasDown) {\n            gLocalCoopModalControllerSlot = slot;\n            enqueueInputEvent(KEY_LOWERCASE_I);\n            modalActive = true;\n            debugPrint("[COOP INVENTORY] slot=%d source=controller button=back\\n", slot);\n        } else if (canOpen && skilldexDown && !runtime.skilldexWasDown) {\n            gLocalCoopModalControllerSlot = slot;\n            gLocalCoopSkilldexInvokerSlot = slot;\n            enqueueInputEvent(KEY_LOWERCASE_S);\n            modalActive = true;\n            debugPrint("[COOP SKILLDEX] slot=%d source=controller button=right-stick\\n", slot);\n        } else if (canOpen && startDown && !runtime.startWasDown) {\n            gLocalCoopModalControllerSlot = slot;\n            enqueueInputEvent(KEY_ESCAPE);\n            modalActive = true;\n            debugPrint("[COOP MENU] slot=%d source=controller button=start\\n", slot);\n        }\n\n        runtime.pipboyWasDown = pipboyDown;\n        runtime.inventoryWasDown = inventoryDown;\n        runtime.startWasDown = startDown;\n        runtime.skilldexWasDown = skilldexDown;'''
if old_modal not in s:
    raise SystemExit('modal hotbind block not found')
s = s.replace(old_modal, new_modal, 1)

old_heal_target = '''    Object* target = localCoopHealingTarget(player);\n    if (target == nullptr) {\n        return false;\n    }'''
new_heal_target = '''    // World quick-medical is self-only. Treating another player/NPC remains\n    // available through the normal Skilldex targeting flow.\n    Object* target = player.actor;\n    if (target == nullptr) {\n        return false;\n    }'''
if old_heal_target not in s:
    raise SystemExit('healing target block not found')
s = s.replace(old_heal_target, new_heal_target, 1)

old_scan = '''        bool firstAidDown = false;\n        bool doctorDown = false;\n\n        if (hasController) {\n            int rightTrigger = SDL_GameControllerGetAxis(player.controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);\n            primaryDown = rightTrigger > 12000;\n            secondaryDown = SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) != 0;\n            reloadDown = SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_X) != 0;\n            swapDown = SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_Y) != 0;\n            firstAidDown = SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT) != 0;\n            doctorDown = SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) != 0;\n        }'''
new_scan = '''        bool medicalDown = false;\n\n        if (hasController) {\n            int rightTrigger = SDL_GameControllerGetAxis(player.controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);\n            primaryDown = rightTrigger > 12000;\n            secondaryDown = SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) != 0;\n            reloadDown = SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_X) != 0;\n            swapDown = SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_Y) != 0;\n            medicalDown = SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) != 0;\n        }'''
if old_scan not in s:
    raise SystemExit('combat D-pad scan block not found')
s = s.replace(old_scan, new_scan, 1)

old_heals = '''        // Phone packets can briefly cross neutral while a touch remains held,\n        // producing a second rising edge. Share a short cooldown between both\n        // healing skills so modal Fallout skill work cannot be re-entered.\n        if (firstAidDown\n            && !runtime.firstAidWasDown\n            && localCoopTickReached(now, runtime.nextHealingSkillTick)) {\n            runtime.nextHealingSkillTick = now + 1000;\n            localCoopUseHealingSkill(player, SKILL_FIRST_AID);\n        }\n        if (doctorDown\n            && !runtime.doctorWasDown\n            && localCoopTickReached(now, runtime.nextHealingSkillTick)) {\n            runtime.nextHealingSkillTick = now + 1000;\n            localCoopUseHealingSkill(player, SKILL_DOCTOR);\n        }'''
new_heals = '''        // D-pad Right is one self-medical quick action. Attempt First Aid and\n        // Doctor on the invoking character only; targeted treatment of someone\n        // else is deliberately left to Skilldex.\n        if (medicalDown\n            && !runtime.doctorWasDown\n            && localCoopTickReached(now, runtime.nextHealingSkillTick)) {\n            runtime.nextHealingSkillTick = now + 1000;\n            localCoopUseHealingSkill(player, SKILL_FIRST_AID);\n            localCoopUseHealingSkill(player, SKILL_DOCTOR);\n        }'''
if old_heals not in s:
    raise SystemExit('healing edge block not found')
s = s.replace(old_heals, new_heals, 1)

old_edges = '''        runtime.swapWasDown = swapDown;\n        runtime.firstAidWasDown = firstAidDown;\n        runtime.doctorWasDown = doctorDown;'''
new_edges = '''        runtime.swapWasDown = swapDown;\n        runtime.firstAidWasDown = false;\n        runtime.doctorWasDown = medicalDown;'''
if old_edges not in s:
    raise SystemExit('healing edge-state block not found')
s = s.replace(old_edges, new_edges, 1)

p.write_text(s, encoding='utf-8')
print('Applied final co-op controller hotbinds')
