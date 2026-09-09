#ifndef ELEVATOR_H
#define ELEVATOR_H

// Capture whether combat_ai.h was already included BEFORE pulling in the
// realtime helper below. combat.cc includes combat_ai.h before elevator.h;
// scripts.cc includes elevator.h without it. This keeps the combat-time clock
// redirect scoped to combat.cc without leaking it into scripts.cc.
#if defined(COMBAT_AI_H)
#define LOCAL_COOP_COMBAT_CC_INTERCEPTS
#endif

#include "local_coop_ai_realtime.h"

namespace fallout {

typedef enum Elevator {
    ELEVATOR_BROTHERHOOD_OF_STEEL_MAIN,
    ELEVATOR_BROTHERHOOD_OF_STEEL_SURFACE,
    ELEVATOR_MASTER_UPPER,
    ELEVATOR_MASTER_LOWER,
    ELEVATOR_MILITARY_BASE_UPPER,
    ELEVATOR_MILITARY_BASE_LOWER,
    ELEVATOR_GLOW_UPPER,
    ELEVATOR_GLOW_LOWER,
    ELEVATOR_VAULT_13,
    ELEVATOR_NECROPOLIS,
    ELEVATOR_SIERRA_1,
    ELEVATOR_SIERRA_2,
    ELEVATOR_SIERRA_SERVICE,
    ELEVATOR_KLAMATH_TOXIC_CAVES,
    ELEVATOR_14,
    ELEVATOR_VAULT_CITY,
    ELEVATOR_VAULT_15_MAIN,
    ELEVATOR_VAULT_15_SURFACE,
    ELEVATOR_NAVARRO_NORTHERN,
    ELEVATOR_NAVARRO_CENTER,
    ELEVATOR_NAVARRO_LAB,
    ELEVATOR_NAVARRO_CANTEEN,
    ELEVATOR_SAN_FRANCISCO_SHI_TEMPLE,
    ELEVATOR_REDDING_WANAMINGO_MINE,
    ELEVATOR_COUNT,
} Elevator;

int elevatorSelectLevel(int elevator, int* mapPtr, int* elevationPtr, int* tilePtr);

void elevatorsInit();

} // namespace fallout

#ifdef LOCAL_COOP_COMBAT_CC_INTERCEPTS
// AI routing is now owned by combat_ai.h's single dispatcher. Keep only the
// combat.cc-specific game-time redirect here.
#define gameTimeAddSeconds localCoopRealtimeCombatAdvanceTime
#undef LOCAL_COOP_COMBAT_CC_INTERCEPTS
#endif

#endif /* ELEVATOR_H */
