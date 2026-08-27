from pathlib import Path

# Repair the already-expanded 18-choice menu marker when the menu content is
# present but an older helper stopped before adding its marker.
p = Path('src/local_coop.h')
s = p.read_text(encoding='utf-8')
if '// COOP_18_ARCHETYPES_MENU_V1' not in s:
    needle = '    windowDrawText(player.joinWindow, choice, 380, 20, 82, _colorTable[992]);\n    windowDrawText(player.joinWindow, kLocalCoopArchetypeRoles[player.archetype], 380, 20, 106, _colorTable[992]);'
    if needle not in s:
        raise SystemExit('18-choice menu content missing')
    s = s.replace(needle, '    // COOP_18_ARCHETYPES_MENU_V1\n' + needle, 1)
    p.write_text(s, encoding='utf-8')

# Repair the final controller mapping against the current runtime source.
p = Path('src/local_coop_runtime.h')
s = p.read_text(encoding='utf-8')
if '// COOP_FINAL_HOTBINDS_V1' not in s:
    # State edges.
    state_old = '    bool pipboyWasDown = false;\n    bool skilldexWasDown = false;'
    state_new = '    bool pipboyWasDown = false;\n    bool inventoryWasDown = false;\n    bool startWasDown = false;\n    bool skilldexWasDown = false;'
    if state_old in s:
        s = s.replace(state_old, state_new, 1)

    # Disable obsolete Back+Start postgame switching regardless of body drift.
    fn = 'inline void localCoopProcessPostgameCampaignSwitch()\n{\n'
    if fn in s:
        s = s.replace(fn, fn + '    // COOP_FINAL_HOTBINDS_V1\n    // Final controller layout owns Back and Start independently.\n    return;\n\n', 1)
    else:
        raise SystemExit('postgame switch function missing')

    # World modal buttons: D-left PipBoy, Back inventory, RS Skilldex, Start menu.
    old = '''        bool pipboyDown = backDown && !startDown;
        bool canOpen = !modalActive'''
    new = '''        bool pipboyDown = player.controller != nullptr
            && SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT) != 0;
        bool inventoryDown = backDown;
        bool canOpen = !modalActive'''
    if old not in s:
        raise SystemExit('modal pipboy mapping anchor missing')
    s = s.replace(old, new, 1)

    old = '''        if (canOpen && pipboyDown && !runtime.pipboyWasDown) {
            gLocalCoopModalControllerSlot = slot;
            enqueueInputEvent(KEY_LOWERCASE_P);
            modalActive = true;
            debugPrint("[PHOBOI INPUT] slot=%d source=controller button=back\\n", slot);
        } else if (canOpen && skilldexDown && !runtime.skilldexWasDown) {'''
    new = '''        if (canOpen && pipboyDown && !runtime.pipboyWasDown) {
            gLocalCoopModalControllerSlot = slot;
            enqueueInputEvent(KEY_LOWERCASE_P);
            modalActive = true;
            debugPrint("[PHOBOI INPUT] slot=%d source=controller button=dpad-left\\n", slot);
        } else if (canOpen && inventoryDown && !runtime.inventoryWasDown) {
            gLocalCoopModalControllerSlot = slot;
            enqueueInputEvent(KEY_LOWERCASE_I);
            modalActive = true;
            debugPrint("[COOP INVENTORY] slot=%d source=controller button=back\\n", slot);
        } else if (canOpen && skilldexDown && !runtime.skilldexWasDown) {'''
    if old not in s:
        raise SystemExit('modal action block missing')
    s = s.replace(old, new, 1)

    old = '''            debugPrint("[COOP SKILLDEX] slot=%d source=controller button=right-stick\\n", slot);
        }

        runtime.pipboyWasDown = pipboyDown;
        runtime.skilldexWasDown = skilldexDown;'''
    new = '''            debugPrint("[COOP SKILLDEX] slot=%d source=controller button=right-stick\\n", slot);
        } else if (canOpen && startDown && !runtime.startWasDown) {
            gLocalCoopModalControllerSlot = slot;
            enqueueInputEvent(KEY_ESCAPE);
            modalActive = true;
            debugPrint("[COOP MENU] slot=%d source=controller button=start\\n", slot);
        }

        runtime.pipboyWasDown = pipboyDown;
        runtime.inventoryWasDown = inventoryDown;
        runtime.startWasDown = startDown;
        runtime.skilldexWasDown = skilldexDown;'''
    if old not in s:
        raise SystemExit('modal edge block missing')
    s = s.replace(old, new, 1)

    # D-pad Right is the one self-medical action.
    old = '''        bool firstAidDown = false;
        bool doctorDown = false;'''
    if old in s:
        s = s.replace(old, '        bool medicalDown = false;', 1)
    old = '''            firstAidDown = SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT) != 0;
            doctorDown = SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) != 0;'''
    if old not in s:
        raise SystemExit('medical button scan missing')
    s = s.replace(old, '            medicalDown = SDL_GameControllerGetButton(player.controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) != 0;', 1)

    old = '''        if (firstAidDown
            && !runtime.firstAidWasDown
            && localCoopTickReached(now, runtime.nextHealingSkillTick)) {
            runtime.nextHealingSkillTick = now + 1000;
            localCoopUseHealingSkill(player, SKILL_FIRST_AID);
        }
        if (doctorDown
            && !runtime.doctorWasDown
            && localCoopTickReached(now, runtime.nextHealingSkillTick)) {
            runtime.nextHealingSkillTick = now + 1000;
            localCoopUseHealingSkill(player, SKILL_DOCTOR);
        }'''
    new = '''        if (medicalDown
            && !runtime.doctorWasDown
            && localCoopTickReached(now, runtime.nextHealingSkillTick)) {
            runtime.nextHealingSkillTick = now + 1000;
            Object* savedTarget = localCoopHealingTarget(player);
            (void)savedTarget;
            localCoopUseHealingSkill(player, SKILL_FIRST_AID);
            localCoopUseHealingSkill(player, SKILL_DOCTOR);
        }'''
    if old not in s:
        raise SystemExit('medical action block missing')
    s = s.replace(old, new, 1)

    old = '        runtime.firstAidWasDown = firstAidDown;\n        runtime.doctorWasDown = doctorDown;'
    if old not in s:
        raise SystemExit('medical edge state missing')
    s = s.replace(old, '        runtime.firstAidWasDown = false;\n        runtime.doctorWasDown = medicalDown;', 1)

    # Force quick-medical target to self; Skilldex still uses its own targeting.
    old = '''    Object* target = localCoopHealingTarget(player);
    if (target == nullptr) {
        return false;
    }'''
    if old in s:
        s = s.replace(old, '''    Object* target = player.actor;
    if (target == nullptr) {
        return false;
    }''', 1)

    p.write_text(s, encoding='utf-8')

print('Repaired 18-choice menu marker and final hotbinds')
