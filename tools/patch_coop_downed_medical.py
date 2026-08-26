from pathlib import Path

MARKER = '// COOP_DOWNED_MEDICAL_V1'

# 1) Stop stock combat from marking human-owned co-op actors dead at 0 HP.
p = Path('src/combat.cc')
s = p.read_text(encoding='utf-8')
if MARKER not in s:
    old = '''static void _check_for_death(Object* object, int damage, int* flags)
{
    if (object == nullptr || !_critter_flag_check(object->pid, CRITTER_INVULNERABLE)) {
        if (object == nullptr || PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
            if (damage > 0) {
                if (critterGetHitPoints(object) - damage <= 0) {
                    *flags |= DAM_DEAD;
                }
            }
        }
    }
}
'''
    new = '''static void _check_for_death(Object* object, int damage, int* flags)
{
    // COOP_DOWNED_MEDICAL_V1
    // Human-controlled co-op actors do not enter Fallout's stock death path.
    // At 0 HP they become downed; the co-op runtime owns the 60 second bleedout,
    // -100 HP hard floor, Doctor revival and medical evacuation.
    if (object != nullptr
        && gLocalCoopInitialized
        && localCoopActorIsHumanOwned(object)
        && damage > 0
        && critterGetHitPoints(object) - damage <= 0) {
        *flags &= ~DAM_DEAD;
        *flags |= DAM_KNOCKED_OUT;
        return;
    }

    if (object == nullptr || !_critter_flag_check(object->pid, CRITTER_INVULNERABLE)) {
        if (object == nullptr || PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
            if (damage > 0) {
                if (critterGetHitPoints(object) - damage <= 0) {
                    *flags |= DAM_DEAD;
                }
            }
        }
    }
}
'''
    if old not in s:
        raise SystemExit('combat.cc _check_for_death block not found')
    s = s.replace(old, new, 1)
    p.write_text(s, encoding='utf-8')
    print('Patched combat death flag for co-op downed state')
else:
    print('combat.cc downed death flag already patched')

# 2) Stop generic HP adjustment (poison, scripts, etc.) from stock-killing co-op
# actors and clamp them at -100 HP.
p = Path('src/critter.cc')
s = p.read_text(encoding='utf-8')
if MARKER not in s:
    if '#include "local_coop.h"\n' not in s:
        anchor = '#include "loadsave.h"\n'
        if anchor not in s:
            raise SystemExit('critter.cc include anchor not found')
        s = s.replace(anchor, anchor + '#include "local_coop.h"\n', 1)

    old = '''int critterAdjustHitPoints(Object* critter, int hp)
{
    if (PID_TYPE(critter->pid) != OBJ_TYPE_CRITTER) {
        return 0;
    }

    int maximumHp = critterGetStat(critter, STAT_MAXIMUM_HIT_POINTS);
    int newHp = critter->data.critter.hp + hp;

    critter->data.critter.hp = newHp;
    if (maximumHp >= newHp) {
        if (newHp <= 0 && (critter->data.critter.combat.results & DAM_DEAD) == 0) {
            critterKill(critter, -1, true);
        }
    } else {
        critter->data.critter.hp = maximumHp;
    }

    return 0;
}
'''
    new = '''int critterAdjustHitPoints(Object* critter, int hp)
{
    if (PID_TYPE(critter->pid) != OBJ_TYPE_CRITTER) {
        return 0;
    }

    int maximumHp = critterGetStat(critter, STAT_MAXIMUM_HIT_POINTS);
    int newHp = critter->data.critter.hp + hp;

    // COOP_DOWNED_MEDICAL_V1
    if (gLocalCoopInitialized && localCoopActorIsHumanOwned(critter)) {
        // Co-op players can survive below zero while teammates attempt Doctor.
        // -100 is the absolute damage floor; the runtime converts that state to
        // a medical evacuation instead of allowing Fallout's stock game-over.
        newHp = std::max(-100, std::min(newHp, maximumHp));
        critter->data.critter.hp = newHp;
        return 0;
    }

    critter->data.critter.hp = newHp;
    if (maximumHp >= newHp) {
        if (newHp <= 0 && (critter->data.critter.combat.results & DAM_DEAD) == 0) {
            critterKill(critter, -1, true);
        }
    } else {
        critter->data.critter.hp = maximumHp;
    }

    return 0;
}
'''
    if old not in s:
        raise SystemExit('critter.cc critterAdjustHitPoints block not found')
    s = s.replace(old, new, 1)
    p.write_text(s, encoding='utf-8')
    print('Patched generic HP adjustment with -100 co-op floor')
else:
    print('critter.cc downed HP floor already patched')

# 3) Runtime: timer, Doctor revive, treatment cost/debt and evacuation.
p = Path('src/local_coop_runtime.h')
s = p.read_text(encoding='utf-8')
if MARKER not in s:
    old_fields = '''    bool queuedAttackPending = false;
    bool queuedAttackSecondary = false;
    int queuedAttackTargetId = -1;
    Object* aimTarget = nullptr;
};
'''
    new_fields = '''    bool queuedAttackPending = false;
    bool queuedAttackSecondary = false;
    int queuedAttackTargetId = -1;
    Object* aimTarget = nullptr;

    // COOP_DOWNED_MEDICAL_V1
    bool downed = false;
    Uint32 downedUntil = 0;
};
'''
    if old_fields not in s:
        raise SystemExit('local_coop_runtime.h runtime slot field anchor not found')
    s = s.replace(old_fields, new_fields, 1)

    global_anchor = 'inline int gLocalCoopCameraTargetTile = -1;\n'
    if global_anchor not in s:
        raise SystemExit('local_coop_runtime.h global anchor not found')
    s = s.replace(global_anchor, global_anchor + '''inline Uint32 gLocalCoopNextMedicalDebtPaymentTick = 0;
inline constexpr Uint32 kLocalCoopDownedDurationMs = 60000;
inline constexpr int kLocalCoopDownedHardFloor = -100;
inline constexpr int kLocalCoopMedicalDebtUnitCaps = 100;

''', 1)

    healing_anchor = '''inline Object* localCoopHealingTarget(LocalCoopPlayer& player)
{
'''
    if healing_anchor not in s:
        raise SystemExit('local_coop_runtime.h healing target anchor not found')

    helpers = r'''// COOP_DOWNED_MEDICAL_V1
inline LocalCoopPlayer* localCoopPlayerForActor(Object* actor)
{
    if (actor == nullptr) {
        return nullptr;
    }
    for (LocalCoopPlayer& player : gLocalCoopPlayers) {
        if (player.humanOwned && player.actor == actor) {
            return &player;
        }
    }
    return nullptr;
}

inline bool localCoopActorIsDowned(Object* actor)
{
    LocalCoopPlayer* player = localCoopPlayerForActor(actor);
    if (player == nullptr) {
        return false;
    }
    return gLocalCoopRuntimeSlots[player->slot].downed;
}

inline int localCoopMedicalTreatmentFee(Object* actor)
{
    int level = actor != nullptr ? std::max(1, critterGetStat(actor, STAT_LEVEL)) : 1;
    int rawFee = 100 + 25 * level;
    // Debt is stored in the roster's existing reserved byte as 100-cap units,
    // preserving the current save chunk layout and old-save compatibility.
    return ((rawFee + kLocalCoopMedicalDebtUnitCaps - 1) / kLocalCoopMedicalDebtUnitCaps)
        * kLocalCoopMedicalDebtUnitCaps;
}

inline void localCoopAddMedicalDebt(LocalCoopPlayer& player, int caps)
{
    if (caps <= 0) {
        return;
    }
    LocalCoopCharacterSlotState& saved = localCoopCharacterStateGet().slots[player.slot];
    int units = (caps + kLocalCoopMedicalDebtUnitCaps - 1) / kLocalCoopMedicalDebtUnitCaps;
    int combined = std::min(255, static_cast<int>(saved.reserved) + units);
    saved.reserved = static_cast<uint8_t>(combined);
    gLocalCoopCharacterStateRevision++;
}

inline void localCoopChargeMedicalTreatment(LocalCoopPlayer& player)
{
    int fee = localCoopMedicalTreatmentFee(player.actor);
    Object* sharedOwner = localCoopGetSharedInventoryOwner();
    int available = sharedOwner != nullptr ? std::max(0, itemGetTotalCaps(sharedOwner)) : 0;
    int paid = std::min(fee, available);
    if (paid > 0 && sharedOwner != nullptr) {
        itemCapsAdjust(sharedOwner, -paid);
    }
    int owed = fee - paid;
    localCoopAddMedicalDebt(player, owed);
    debugPrint("[COOP MEDICAL] slot=%d fee=%d paid=%d debtAdded=%d debtUnits=%d\n",
        player.slot,
        fee,
        paid,
        owed,
        static_cast<int>(localCoopCharacterStateGetConst().slots[player.slot].reserved));
}

inline void localCoopProcessMedicalDebtPayments(Uint32 now)
{
    if (!localCoopTickReached(now, gLocalCoopNextMedicalDebtPaymentTick)) {
        return;
    }
    gLocalCoopNextMedicalDebtPaymentTick = now + 1000;

    Object* sharedOwner = localCoopGetSharedInventoryOwner();
    if (sharedOwner == nullptr) {
        return;
    }

    int available = std::max(0, itemGetTotalCaps(sharedOwner));
    if (available < kLocalCoopMedicalDebtUnitCaps) {
        return;
    }

    for (LocalCoopPlayer& player : gLocalCoopPlayers) {
        LocalCoopCharacterSlotState& saved = localCoopCharacterStateGet().slots[player.slot];
        if (saved.reserved == 0 || available < kLocalCoopMedicalDebtUnitCaps) {
            continue;
        }

        int affordableUnits = available / kLocalCoopMedicalDebtUnitCaps;
        int units = std::min(static_cast<int>(saved.reserved), affordableUnits);
        int payment = units * kLocalCoopMedicalDebtUnitCaps;
        if (payment <= 0) {
            continue;
        }

        itemCapsAdjust(sharedOwner, -payment);
        available -= payment;
        saved.reserved = static_cast<uint8_t>(static_cast<int>(saved.reserved) - units);
        gLocalCoopCharacterStateRevision++;
        debugPrint("[COOP MEDICAL] slot=%d debt-payment=%d remainingUnits=%d\n",
            player.slot,
            payment,
            static_cast<int>(saved.reserved));
    }
}

inline Object* localCoopFindDownedDoctorTarget(LocalCoopPlayer& healer)
{
    Object* focused = localCoopFocusFindInteractable(healer);
    if (focused != nullptr
        && focused != healer.actor
        && localCoopActorIsDowned(focused)
        && !localCoopFocusIsEnemy(healer.actor, focused)) {
        return focused;
    }

    Object* best = nullptr;
    int bestDistance = 0x7FFFFFFF;
    for (LocalCoopPlayer& candidate : gLocalCoopPlayers) {
        if (!candidate.humanOwned
            || candidate.actor == nullptr
            || candidate.actor == healer.actor
            || !gLocalCoopRuntimeSlots[candidate.slot].downed
            || candidate.actor->elevation != healer.actor->elevation) {
            continue;
        }
        int distance = objectGetDistanceBetween(healer.actor, candidate.actor);
        if (distance <= 3 && distance < bestDistance) {
            bestDistance = distance;
            best = candidate.actor;
        }
    }
    return best;
}

inline bool localCoopReviveDownedPlayer(LocalCoopPlayer& healer, Object* target)
{
    LocalCoopPlayer* patient = localCoopPlayerForActor(target);
    if (patient == nullptr || !gLocalCoopRuntimeSlots[patient->slot].downed) {
        return false;
    }

    LocalCoopRuntimeSlot& patientRuntime = gLocalCoopRuntimeSlots[patient->slot];
    int maximumHp = std::max(1, critterGetStat(target, STAT_MAXIMUM_HIT_POINTS));
    int reviveHp = std::max(1, maximumHp / 4);
    reg_anim_clear(target);
    target->data.critter.hp = reviveHp;
    target->data.critter.combat.results &= ~(DAM_DEAD | DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN | DAM_LOSE_TURN);
    patientRuntime.downed = false;
    patientRuntime.downedUntil = 0;
    if (target == gDude && gInterfaceBarWindow != -1) {
        interfaceRenderHitPoints(true);
    }
    debugPrint("[COOP REVIVE] healerSlot=%d patientSlot=%d hp=%d\n",
        healer.slot,
        patient->slot,
        reviveHp);
    return true;
}

inline void localCoopMedicalEvacuate(LocalCoopPlayer& player, const char* reason)
{
    Object* actor = player.actor;
    if (actor == nullptr) {
        return;
    }

    localCoopChargeMedicalTreatment(player);

    int maximumHp = std::max(1, critterGetStat(actor, STAT_MAXIMUM_HIT_POINTS));
    actor->data.critter.hp = std::max(1, maximumHp / 2);
    actor->data.critter.combat.results &= ~(DAM_DEAD | DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN | DAM_LOSE_TURN);
    LocalCoopRuntimeSlot& runtime = gLocalCoopRuntimeSlots[player.slot];
    runtime.downed = false;
    runtime.downedUntil = 0;
    localCoopClearQueuedAttack(runtime);

    // The party shares one loaded map, so a medical rescue evacuates the whole
    // co-op group out of the lethal encounter. The world/town layer can then
    // place the party at its safe medical destination without splitting players
    // across maps.
    localCoopDangerEnd();
    localCoopRealtimeAiReset();
    scriptsRequestWorldMap();

    if (actor == gDude && gInterfaceBarWindow != -1) {
        interfaceRenderHitPoints(true);
    }
    debugPrint("[COOP MEDICAL] evacuation slot=%d reason=%s hp=%d\n",
        player.slot,
        reason != nullptr ? reason : "unknown",
        actor->data.critter.hp);
}

inline void localCoopProcessDownedPlayers(Uint32 now)
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
            debugPrint("[COOP DOWNED] slot=%d hp=%d bleedoutMs=%u\n",
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
    s = s.replace(healing_anchor, helpers + healing_anchor, 1)

    old_doctor = '''        if (doctorDown
            && !runtime.doctorWasDown
            && localCoopTickReached(now, runtime.nextHealingSkillTick)) {
            runtime.nextHealingSkillTick = now + 1000;
            localCoopUseHealingSkill(player, SKILL_DOCTOR);
        }
'''
    new_doctor = '''        if (doctorDown
            && !runtime.doctorWasDown
            && localCoopTickReached(now, runtime.nextHealingSkillTick)) {
            runtime.nextHealingSkillTick = now + 1000;
            Object* downedTarget = localCoopFindDownedDoctorTarget(player);
            if (downedTarget != nullptr) {
                localCoopReviveDownedPlayer(player, downedTarget);
            } else {
                localCoopUseHealingSkill(player, SKILL_DOCTOR);
            }
        }
'''
    if old_doctor not in s:
        raise SystemExit('local_coop_runtime.h Doctor input block not found')
    s = s.replace(old_doctor, new_doctor, 1)

    tick_anchor = '''    localCoopProcessPostgameWorldSwitch();
    localCoopProcessModalMenuInput();
    localCoopProcessCombatInput();
    localCoopRealtimeAiTick();
'''
    tick_new = '''    Uint32 runtimeNow = SDL_GetTicks();
    localCoopProcessDownedPlayers(runtimeNow);
    localCoopProcessMedicalDebtPayments(runtimeNow);

    localCoopProcessPostgameWorldSwitch();
    localCoopProcessModalMenuInput();
    localCoopProcessCombatInput();
    localCoopRealtimeAiTick();
'''
    if tick_anchor not in s:
        raise SystemExit('local_coop_runtime.h runtime tick anchor not found')
    s = s.replace(tick_anchor, tick_new, 1)

    reset_anchor = '''            runtime.nextApRegenTick = 0;
            runtime.actionPointsHundredths = -1;
            runtime.actionPointsActorId = -1;
            runtime.apRegenDelayTicks = 0;
            localCoopClearQueuedAttack(runtime);
'''
    reset_new = '''            runtime.nextApRegenTick = 0;
            runtime.actionPointsHundredths = -1;
            runtime.actionPointsActorId = -1;
            runtime.apRegenDelayTicks = 0;
            runtime.downed = false;
            runtime.downedUntil = 0;
            localCoopClearQueuedAttack(runtime);
'''
    if reset_anchor not in s:
        raise SystemExit('local_coop_runtime.h runtime reset anchor not found')
    s = s.replace(reset_anchor, reset_new, 1)

    p.write_text(s, encoding='utf-8')
    print('Patched co-op 60s downed, Doctor revive, medical debt and evacuation runtime')
else:
    print('local_coop_runtime.h downed medical runtime already patched')
