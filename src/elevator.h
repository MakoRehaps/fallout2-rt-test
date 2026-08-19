#ifndef ELEVATOR_H
#define ELEVATOR_H

// combat.cc includes elevator.h after combat_ai.h, while combat_ai.cc does not.
// This gives the co-op engine a narrow interception point for NPC turns without
// rewriting the large legacy combat implementation or renaming the original AI
// function itself.
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

// Redirect only translation units that include elevator.h after combat_ai.h
// (notably combat.cc). The original _combat_ai implementation remains intact
// inside combat_ai.cc and is called by localCoopRealtimeAiTurn.
#define _combat_ai localCoopRealtimeAiTurn

#endif /* ELEVATOR_H */
