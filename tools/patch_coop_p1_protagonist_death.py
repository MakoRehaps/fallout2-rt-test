from pathlib import Path

MARKER = '// COOP_P1_PROTAGONIST_DEATH_V2'


def replace_once(path, old, new, label):
    p = Path(path)
    s = p.read_text(encoding='utf-8')
    if MARKER in s:
        print(f'{label} already patched')
        return
    if old not in s:
        raise SystemExit(f'{label} anchor not found')
    p.write_text(s.replace(old, new, 1), encoding='utf-8')
    print(f'Patched {label}')


# P1 is the story protagonist. All human-controlled co-op actors still bypass
# Fallout's stock instant-death path, but only P1 owns the negative-HP / rescue
# rule. P2-P4 are support actors and can only be downed.
combat_old = '''    // COOP_DOWNED_MEDICAL_V1
    // Human-controlled co-op actors do not enter Fallout's stock death path.
    // At 0 HP they become downed; the co-op runtime owns the 60 second bleedout,
    // -100 HP hard floor, Doctor revival and medical evacuation.
'''
combat_new = '''    // COOP_DOWNED_MEDICAL_V1
    // COOP_P1_PROTAGONIST_DEATH_V2
    // Human-controlled co-op actors do not enter Fallout's stock death path.
    // P1 is the story protagonist and owns the negative-HP / medical-rescue
    // rule. P2-P4 stop at 0 HP and remain downed until revived.
'''
replace_once('src/combat.cc', combat_old, combat_new, 'combat protagonist death comment')


# Generic HP damage can arrive from poison/scripts as well as combat. P1 may
# descend to -100 HP. P2-P4 are clamped at 0 HP so they can never enter the
# protagonist death/rescue state.
critter_old = '''    // COOP_DOWNED_MEDICAL_V1
    if (gLocalCoopInitialized && localCoopActorIsHumanOwned(critter)) {
        // Co-op players can survive below zero while teammates attempt Doctor.
        // -100 is the absolute damage floor; the runtime converts that state to
        // a medical rescue instead of allowing Fallout's stock game-over.
        if (newHp < -100) {
            newHp = -100;
        }
        if (newHp > maximumHp) {
            newHp = maximumHp;
        }
        critter->data.critter.hp = newHp;
        return 0;
    }
'''
critter_new = '''    // COOP_DOWNED_MEDICAL_V1
    // COOP_P1_PROTAGONIST_DEATH_V2
    if (gLocalCoopInitialized && localCoopActorIsHumanOwned(critter)) {
        // P1/gDude is the story protagonist and is the only co-op actor allowed
        // below zero. P2-P4 are support actors: reaching 0 HP means downed and
        // further damage cannot push them into protagonist death/rescue.
        int minimumHp = critter == gLocalCoopPlayers[0].actor ? -100 : 0;
        if (newHp < minimumHp) {
            newHp = minimumHp;
        }
        if (newHp > maximumHp) {
            newHp = maximumHp;
        }
        critter->data.critter.hp = newHp;
        return 0;
    }
'''
replace_once('src/critter.cc', critter_old, critter_new, 'critter P1/P2-P4 HP floors')


runtime_path = Path('src/local_coop_runtime.h')
runtime = runtime_path.read_text(encoding='utf-8')
if MARKER in runtime:
    print('runtime protagonist death split already patched')
else:
    evac_old = '''inline void localCoopMedicalEvacuate(LocalCoopPlayer& patient, const char* reason)
{
    if (patient.actor == nullptr) {
        return;
    }

    localCoopChargeMedicalTreatment(patient);
'''
    evac_new = '''inline void localCoopMedicalEvacuate(LocalCoopPlayer& patient, const char* reason)
{
    // COOP_P1_PROTAGONIST_DEATH_V2
    // Only P1 is allowed to trigger the story medical-rescue transition.
    // P2-P4 are support actors and remain downed until another player revives
    // them; they never teleport the party or create a treatment bill.
    if (patient.actor == nullptr || patient.slot != 0) {
        return;
    }

    localCoopChargeMedicalTreatment(patient);
'''
    if evac_old not in runtime:
        raise SystemExit('localCoopMedicalEvacuate anchor not found')
    runtime = runtime.replace(evac_old, evac_new, 1)

    downed_old = '''inline void localCoopProcessDownedPlayers(Uint32 now)
{
    for (LocalCoopPlayer& player : gLocalCoopPlayers) {
        Object* actor = player.actor;
        if (!player.humanOwned || actor == nullptr) {
            continue;
        }

        LocalCoopRuntimeSlot& runtime = gLocalCoopRuntimeSlots[player.slot];
        int hp = critterGetHitPoints(actor);
        if (hp > 0) {
            if (runtime.downed) {
                runtime.downed = false;
                runtime.downedUntil = 0;
                actor->data.critter.combat.results &= ~(DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN | DAM_LOSE_TURN);
            }
            continue;
        }

        if (!runtime.downed) {
            runtime.downed = true;
            runtime.downedUntil = now + kLocalCoopDownedDurationMs;
            localCoopClearQueuedAttack(runtime);
            debugPrint("[COOP DOWNED] slot=%d hp=%d bleedoutMs=%u\\n",
                player.slot,
                hp,
                kLocalCoopDownedDurationMs);
        }

        // Keep stock knockout recovery from waking a downed co-op player before
        // Doctor is used or the medical rescue fires.
        actor->data.critter.combat.results &= ~DAM_DEAD;
        actor->data.critter.combat.results |= DAM_KNOCKED_OUT;

        if (hp <= kLocalCoopDownedHardFloor) {
            localCoopMedicalEvacuate(player, "hp-floor");
            return;
        }
        if (localCoopTickReached(now, runtime.downedUntil)) {
            localCoopMedicalEvacuate(player, "bleedout");
            return;
        }
    }
}
'''
    downed_new = '''inline void localCoopProcessDownedPlayers(Uint32 now)
{
    for (LocalCoopPlayer& player : gLocalCoopPlayers) {
        Object* actor = player.actor;
        if (!player.humanOwned || actor == nullptr) {
            continue;
        }

        LocalCoopRuntimeSlot& runtime = gLocalCoopRuntimeSlots[player.slot];
        int hp = critterGetHitPoints(actor);
        if (hp > 0) {
            if (runtime.downed) {
                runtime.downed = false;
                runtime.downedUntil = 0;
                actor->data.critter.combat.results &= ~(DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN | DAM_LOSE_TURN);
            }
            continue;
        }

        const bool protagonist = player.slot == 0;

        // P2-P4 can only be downed. Keep them at exactly 0 HP even if some
        // damage path bypassed critterAdjustHitPoints, and give them no bleedout
        // timer or medical-evacuation trigger.
        if (!protagonist && hp < 0) {
            actor->data.critter.hp = 0;
            hp = 0;
        }

        if (!runtime.downed) {
            runtime.downed = true;
            runtime.downedUntil = protagonist ? now + kLocalCoopDownedDurationMs : 0;
            localCoopClearQueuedAttack(runtime);
            if (protagonist) {
                debugPrint("[COOP DOWNED] P1 hp=%d bleedoutMs=%u hardFloor=%d\\n",
                    hp,
                    kLocalCoopDownedDurationMs,
                    kLocalCoopDownedHardFloor);
            } else {
                debugPrint("[COOP DOWNED] slot=%d hp=0 persistent-support-downed\\n", player.slot);
            }
        }

        // Keep stock knockout recovery from waking a downed co-op actor. Doctor
        // revival is the only way P2-P4 return; P1 can additionally trigger the
        // protagonist medical rescue.
        actor->data.critter.combat.results &= ~DAM_DEAD;
        actor->data.critter.combat.results |= DAM_KNOCKED_OUT;

        if (!protagonist) {
            continue;
        }

        if (hp <= kLocalCoopDownedHardFloor) {
            localCoopMedicalEvacuate(player, "hp-floor");
            return;
        }
        if (localCoopTickReached(now, runtime.downedUntil)) {
            localCoopMedicalEvacuate(player, "bleedout");
            return;
        }
    }
}
'''
    if downed_old not in runtime:
        raise SystemExit('localCoopProcessDownedPlayers anchor not found')
    runtime = runtime.replace(downed_old, downed_new, 1)
    runtime_path.write_text(runtime, encoding='utf-8')
    print('Patched runtime: P1 protagonist death/rescue, P2-P4 downed-only')
