from pathlib import Path

MARKER = '// COOP_DOWNED_MEDICAL_V1'
CLOSEST_MARKER = '// COOP_CLOSEST_DOCTOR_RESCUE_V2'

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
            # Current source no longer necessarily includes loadsave.h here.
            anchor = '#include "item.h"\n'
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

# 3) Runtime: timer, teammate Doctor revive, persistent debt and evacuation to
# the nearest medical settlement instead of stock game-over/world-map limbo.
p = Path('src/local_coop_runtime.h')
s = p.read_text(encoding='utf-8')
if MARKER not in s:
    # Medical rescue directly uses the safe map loader and world-map area
    # coordinates. These are intentionally kept in the runtime header so the
    # rescue is resolved in the same frame that bleedout completes.
    include_anchor = '#include "mainmenu.h"\n'
    if include_anchor not in s:
        raise SystemExit('local_coop_runtime.h include anchor not found')
    extra_includes = ''
    if '#include "map.h"\n' not in s:
        extra_includes += '#include "map.h"\n'
    if '#include "worldmap.h"\n' not in s:
        extra_includes += '#include "worldmap.h"\n'
    if extra_includes:
        s = s.replace(include_anchor, include_anchor + extra_includes, 1)

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

inline int localCoopMedicalDebtCaps(const LocalCoopPlayer& player)
{
    return static_cast<int>(localCoopCharacterStateGetConst().slots[player.slot].reserved)
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
    debugPrint("[COOP MEDICAL] slot=%d fee=%d paid=%d debtAdded=%d totalDebt=%d\n",
        player.slot,
        fee,
        paid,
        owed,
        localCoopMedicalDebtCaps(player));
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

        // Repay at most one 100-cap unit per player per second. This keeps debt
        // meaningful without instantly deleting every cap the party receives.
        itemCapsAdjust(sharedOwner, -kLocalCoopMedicalDebtUnitCaps);
        available -= kLocalCoopMedicalDebtUnitCaps;
        saved.reserved = static_cast<uint8_t>(static_cast<int>(saved.reserved) - 1);
        gLocalCoopCharacterStateRevision++;
        debugPrint("[COOP MEDICAL] slot=%d debt-payment=%d remainingDebt=%d\n",
            player.slot,
            kLocalCoopMedicalDebtUnitCaps,
            localCoopMedicalDebtCaps(player));
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

struct LocalCoopMedicalDestination {
    int area;
    int map;
    const char* name;
};

// COOP_CLOSEST_DOCTOR_RESCUE_V2
inline LocalCoopMedicalDestination localCoopFindClosestMedicalDestination()
{
    // Each anchor is a safe, ordinary settlement map with nearby medical care.
    // The world-map coordinates decide which one is geographically closest.
    static const LocalCoopMedicalDestination destinations[] = {
        { CITY_ARROYO, MAP_ARROYO_VILLAGE, "Arroyo healer" },
        { CITY_KLAMATH, MAP_KLAMATH_1, "Klamath clinic" },
        { CITY_DEN, MAP_DEN_BUSINESS, "Den medical stop" },
        { CITY_MODOC, MAP_MODOC_MAINSTREET, "Modoc medical stop" },
        { CITY_VAULT_CITY, MAP_VAULTCITY_COURTYARD, "Vault City medical center" },
        { CITY_BROKEN_HILLS, MAP_BROKEN_HILLS1, "Broken Hills medical stop" },
        { CITY_NEW_RENO, MAP_NEW_RENO_2, "New Reno medical stop" },
        { CITY_REDDING, MAP_REDDING_DOWNTOWN, "Redding doctor" },
        { CITY_NEW_CALIFORNIA_REPUBLIC, MAP_NCR_DOWNTOWN, "NCR medical center" },
        { CITY_SAN_FRANCISCO, MAP_SAN_FRAN_CHINATOWN, "San Francisco clinic" },
    };

    int worldX = 0;
    int worldY = 0;
    bool haveWorldPosition = wmGetPartyWorldPos(&worldX, &worldY) == 0;

    int currentArea = -1;
    if (wmMatchAreaContainingMapIdx(mapGetCurrentMap(), &currentArea) == 0) {
        if (!haveWorldPosition) {
            haveWorldPosition = wmGetAreaWorldPos(currentArea, &worldX, &worldY) == 0;
        }
    }

    // If world coordinates are unavailable, prefer the medical anchor in the
    // current settlement before falling back to Klamath.
    if (!haveWorldPosition) {
        for (const LocalCoopMedicalDestination& destination : destinations) {
            if (destination.area == currentArea) {
                return destination;
            }
        }
        return destinations[1];
    }

    const LocalCoopMedicalDestination* best = &destinations[1];
    long long bestDistance = 0x7FFFFFFFFFFFFFFFLL;
    for (const LocalCoopMedicalDestination& destination : destinations) {
        int x = 0;
        int y = 0;
        if (wmGetAreaWorldPos(destination.area, &x, &y) != 0) {
            continue;
        }
        long long dx = static_cast<long long>(x) - worldX;
        long long dy = static_cast<long long>(y) - worldY;
        long long distance = dx * dx + dy * dy;
        if (distance < bestDistance) {
            bestDistance = distance;
            best = &destination;
        }
    }
    return *best;
}

inline void localCoopMedicalEvacuate(LocalCoopPlayer& patient, const char* reason)
{
    if (patient.actor == nullptr) {
        return;
    }

    localCoopChargeMedicalTreatment(patient);
    LocalCoopMedicalDestination destination = localCoopFindClosestMedicalDestination();

    // One loaded map is shared by all players, so rescue the entire party from
    // the lethal scene. Everyone is stabilized to 50% HP, while only the player
    // whose bleedout triggered the ambulance gets this treatment bill.
    for (LocalCoopPlayer& player : gLocalCoopPlayers) {
        Object* actor = player.actor;
        if (!player.humanOwned || actor == nullptr) {
            continue;
        }
        int maximumHp = std::max(1, critterGetStat(actor, STAT_MAXIMUM_HIT_POINTS));
        actor->data.critter.hp = std::max(1, maximumHp / 2);
        actor->data.critter.combat.results &= ~(DAM_DEAD | DAM_KNOCKED_OUT | DAM_KNOCKED_DOWN | DAM_LOSE_TURN);
        LocalCoopRuntimeSlot& runtime = gLocalCoopRuntimeSlots[player.slot];
        runtime.downed = false;
        runtime.downedUntil = 0;
        localCoopClearQueuedAttack(runtime);
    }

    localCoopDangerEnd();
    localCoopRealtimeAiReset();
    animationStop();

    // Keep the physical world position consistent with the hospital/doctor
    // destination, then load its safe settlement map immediately. If a custom
    // campaign profile does not expose that map, fall back to the world map
    // rather than ever allowing stock game-over.
    int teleportRc = wmTeleportToArea(destination.area);
    int loadRc = mapLoadById(destination.map);
    if (loadRc != 0) {
        scriptsRequestWorldMap();
    }

    debugPrint("[COOP MEDICAL] rescue slot=%d reason=%s destination=%s area=%d map=%d teleportRc=%d loadRc=%d debt=%d\n",
        patient.slot,
        reason != nullptr ? reason : "unknown",
        destination.name,
        destination.area,
        destination.map,
        teleportRc,
        loadRc,
        localCoopMedicalDebtCaps(patient));
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

    # Older runtime used a dedicated Doctor button. Current controller builds
    # intentionally combine First Aid + Doctor on the medical bind. Support both
    # source shapes so this patch remains idempotent across build branches.
    current_medical = '''        if (medicalDown
            && !runtime.doctorWasDown
            && localCoopTickReached(now, runtime.nextHealingSkillTick)) {
            runtime.nextHealingSkillTick = now + 1000;
            localCoopUseHealingSkill(player, SKILL_FIRST_AID);
            localCoopUseHealingSkill(player, SKILL_DOCTOR);
        }
'''
    current_medical_new = '''        if (medicalDown
            && !runtime.doctorWasDown
            && localCoopTickReached(now, runtime.nextHealingSkillTick)) {
            runtime.nextHealingSkillTick = now + 1000;
            Object* downedTarget = localCoopFindDownedDoctorTarget(player);
            if (downedTarget != nullptr) {
                localCoopReviveDownedPlayer(player, downedTarget);
            } else {
                localCoopUseHealingSkill(player, SKILL_FIRST_AID);
                localCoopUseHealingSkill(player, SKILL_DOCTOR);
            }
        }
'''
    old_doctor = '''        if (doctorDown
            && !runtime.doctorWasDown
            && localCoopTickReached(now, runtime.nextHealingSkillTick)) {
            runtime.nextHealingSkillTick = now + 1000;
            localCoopUseHealingSkill(player, SKILL_DOCTOR);
        }
'''
    old_doctor_new = '''        if (doctorDown
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
    if current_medical in s:
        s = s.replace(current_medical, current_medical_new, 1)
    elif old_doctor in s:
        s = s.replace(old_doctor, old_doctor_new, 1)
    else:
        raise SystemExit('local_coop_runtime.h current medical/Doctor input block not found')

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

    # Surface persistent debt directly in the four-player HUD.
    hud_old = '''        char header[64];
        snprintf(header, sizeof(header), "P%d  %s", slot + 1,
            player.connected ? "CONNECTED" : (player.slotLocked ? "RESERVED" : "EMPTY"));
        windowDrawText(gLocalCoopHudWindow, header, textWidth, textX, 8, _colorTable[992]);
'''
    hud_new = '''        char header[96];
        int medicalDebt = static_cast<int>(localCoopCharacterStateGetConst().slots[slot].reserved)
            * kLocalCoopMedicalDebtUnitCaps;
        if (medicalDebt > 0) {
            snprintf(header, sizeof(header), "P%d  %s  DEBT %d", slot + 1,
                player.connected ? "CONNECTED" : (player.slotLocked ? "RESERVED" : "EMPTY"),
                medicalDebt);
        } else {
            snprintf(header, sizeof(header), "P%d  %s", slot + 1,
                player.connected ? "CONNECTED" : (player.slotLocked ? "RESERVED" : "EMPTY"));
        }
        windowDrawText(gLocalCoopHudWindow, header, textWidth, textX, 8, _colorTable[992]);
'''
    if hud_old not in s:
        raise SystemExit('local_coop_runtime.h HUD header anchor not found')
    s = s.replace(hud_old, hud_new, 1)

    p.write_text(s, encoding='utf-8')
    print('Patched co-op downed, Doctor revive, persistent debt and closest-doctor rescue runtime')
else:
    print('local_coop_runtime.h downed medical runtime already patched')
    if CLOSEST_MARKER not in s:
        raise SystemExit('Existing downed runtime is older than closest-doctor rescue V2; materialize/update source before compiling')
