#ifndef UNIFIED_VEHICLE_SYSTEM_H
#define UNIFIED_VEHICLE_SYSTEM_H

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>

#include "display_monitor.h"
#include "object.h"
#include "proto.h"
#include "proto_types.h"
#include "skill.h"
#include "stat.h"
#include "unified_world_system.h"

namespace fallout {

enum class UnifiedVehicleType : int {
    Motorbike = 0,
    Scout = 1,
    Buggy = 2,
    Highwayman = 3,
    Vertibird = 4,
    Count = 5,
    None = -1,
};

inline constexpr int kUnifiedVehiclePartsRequired = 3;
inline constexpr int kUnifiedVehicleRepairSkillRequired = 50;
inline constexpr int kUnifiedVehicleScienceSkillRequired = 40;
inline constexpr int kUnifiedVehicleIntelligenceRequired = 7;
inline constexpr int kUnifiedVehicleLuckRequired = 6;
inline constexpr int kUnifiedVehicleSalvageLogCapacity = 32;

inline int gUnifiedVehicleSalvagedObjectIds[kUnifiedVehicleSalvageLogCapacity] {};
inline int gUnifiedVehicleSalvagedObjectCount = 0;
inline int gUnifiedVehicleLoadedPrototypePids[static_cast<int>(UnifiedVehicleType::Count)] {
    -1, -1, -1, -1, -1
};

inline const char* unifiedVehicleName(UnifiedVehicleType type)
{
    switch (type) {
    case UnifiedVehicleType::Motorbike: return "motorbike";
    case UnifiedVehicleType::Scout: return "scout";
    case UnifiedVehicleType::Buggy: return "buggy";
    case UnifiedVehicleType::Highwayman: return "Highwayman";
    case UnifiedVehicleType::Vertibird: return "Vertibird";
    default: return "vehicle";
    }
}

inline bool unifiedVehicleContainsInsensitive(const char* text, const char* token)
{
    if (text == nullptr || token == nullptr || token[0] == '\0') {
        return false;
    }
    size_t tokenLength = strlen(token);
    for (const char* cursor = text; *cursor != '\0'; cursor++) {
        size_t index = 0;
        while (index < tokenLength && cursor[index] != '\0'
            && tolower(static_cast<unsigned char>(cursor[index]))
                == tolower(static_cast<unsigned char>(token[index]))) {
            index++;
        }
        if (index == tokenLength) return true;
    }
    return false;
}

inline UnifiedVehicleType unifiedVehicleTypeFromName(const char* name)
{
    if (unifiedVehicleContainsInsensitive(name, "vertibird")) return UnifiedVehicleType::Vertibird;
    if (unifiedVehicleContainsInsensitive(name, "highwayman")) return UnifiedVehicleType::Highwayman;
    if (unifiedVehicleContainsInsensitive(name, "motorbike")
        || unifiedVehicleContainsInsensitive(name, "motorcycle")
        || unifiedVehicleContainsInsensitive(name, "bike")) return UnifiedVehicleType::Motorbike;
    if (unifiedVehicleContainsInsensitive(name, "buggy")) return UnifiedVehicleType::Buggy;
    if (unifiedVehicleContainsInsensitive(name, "scout")) return UnifiedVehicleType::Scout;
    if (unifiedVehicleContainsInsensitive(name, "car")
        || unifiedVehicleContainsInsensitive(name, "truck")
        || unifiedVehicleContainsInsensitive(name, "vehicle")
        || unifiedVehicleContainsInsensitive(name, "wreck")) return UnifiedVehicleType::Scout;
    return UnifiedVehicleType::None;
}

inline UnifiedVehicleType unifiedVehicleTypeForObject(const Object* object)
{
    if (object == nullptr || PID_TYPE(object->pid) != OBJ_TYPE_SCENERY) return UnifiedVehicleType::None;
    if (object->pid == PROTO_ID_CAR) return UnifiedVehicleType::Highwayman;
    return unifiedVehicleTypeFromName(protoGetName(object->pid));
}

inline uint8_t& unifiedVehicleOwnedMask() { return unifiedWorldSystemGetState().travel.reserved; }
inline uint8_t& unifiedVehiclePackedState() { return unifiedWorldSystemGetState().travel.roadReserved; }
inline int unifiedVehicleParts() { return unifiedVehiclePackedState() & 0x07; }
inline void unifiedVehicleSetParts(int parts)
{
    parts = std::max(0, std::min(parts, 7));
    unifiedVehiclePackedState() = static_cast<uint8_t>((unifiedVehiclePackedState() & 0xF8) | parts);
}
inline UnifiedVehicleType unifiedVehicleActiveType()
{
    int encoded = (unifiedVehiclePackedState() >> 3) & 0x07;
    return encoded == 0 ? UnifiedVehicleType::None : static_cast<UnifiedVehicleType>(encoded - 1);
}
inline void unifiedVehicleSetActiveType(UnifiedVehicleType type)
{
    int encoded = type == UnifiedVehicleType::None ? 0 : static_cast<int>(type) + 1;
    unifiedVehiclePackedState() = static_cast<uint8_t>((unifiedVehiclePackedState() & 0xC7) | ((encoded & 0x07) << 3));
}
inline bool unifiedVehicleOwns(UnifiedVehicleType type)
{
    int index = static_cast<int>(type);
    return index >= 0 && index < static_cast<int>(UnifiedVehicleType::Count)
        && (unifiedVehicleOwnedMask() & (1 << index)) != 0;
}
inline bool unifiedVehicleCanFastTravel() { return (unifiedVehicleOwnedMask() & 0x1F) != 0; }
inline int unifiedVehicleTravelTimePercent()
{
    switch (unifiedVehicleActiveType()) {
    case UnifiedVehicleType::Motorbike: return 70;
    case UnifiedVehicleType::Scout: return 62;
    case UnifiedVehicleType::Buggy: return 55;
    case UnifiedVehicleType::Highwayman: return 45;
    case UnifiedVehicleType::Vertibird: return 25;
    default: return 100;
    }
}
inline void unifiedVehicleResetEncounterSalvage() { gUnifiedVehicleSalvagedObjectCount = 0; }
inline bool unifiedVehicleObjectWasSalvaged(int id)
{
    for (int i = 0; i < gUnifiedVehicleSalvagedObjectCount; i++)
        if (gUnifiedVehicleSalvagedObjectIds[i] == id) return true;
    return false;
}
inline void unifiedVehicleMarkObjectSalvaged(int id)
{
    if (!unifiedVehicleObjectWasSalvaged(id) && gUnifiedVehicleSalvagedObjectCount < kUnifiedVehicleSalvageLogCapacity)
        gUnifiedVehicleSalvagedObjectIds[gUnifiedVehicleSalvagedObjectCount++] = id;
}

inline bool unifiedVehicleTryUseWreck(Object* object)
{
    UnifiedVehicleType type = unifiedVehicleTypeForObject(object);
    if (type == UnifiedVehicleType::None || gDude == nullptr) return false;
    if (unifiedVehicleOwns(type)) {
        unifiedVehicleSetActiveType(type);
        char message[96];
        snprintf(message, sizeof(message), "%s selected for world-map travel.", unifiedVehicleName(type));
        displayMonitorAddMessage(message);
        return true;
    }

    int repair = skillGetValue(gDude, SKILL_REPAIR);
    int science = skillGetValue(gDude, SKILL_SCIENCE);
    int intelligence = critterGetStat(gDude, STAT_INTELLIGENCE);
    int luck = critterGetStat(gDude, STAT_LUCK);
    bool directRepair = repair >= kUnifiedVehicleRepairSkillRequired
        && intelligence >= kUnifiedVehicleIntelligenceRequired
        && luck >= kUnifiedVehicleLuckRequired;
    bool partsRepair = unifiedVehicleParts() >= kUnifiedVehiclePartsRequired;
    if (directRepair || partsRepair) {
        if (partsRepair && !directRepair)
            unifiedVehicleSetParts(unifiedVehicleParts() - kUnifiedVehiclePartsRequired);
        unifiedVehicleOwnedMask() |= static_cast<uint8_t>(1 << static_cast<int>(type));
        unifiedVehicleSetActiveType(type);
        char message[128];
        snprintf(message, sizeof(message), "%s restored. World-map fast travel is now available.", unifiedVehicleName(type));
        displayMonitorAddMessage(message);
        return true;
    }

    if (science >= kUnifiedVehicleScienceSkillRequired && !unifiedVehicleObjectWasSalvaged(object->id)) {
        unifiedVehicleMarkObjectSalvaged(object->id);
        unifiedVehicleSetParts(unifiedVehicleParts() + 1);
        char message[128];
        snprintf(message, sizeof(message), "Salvaged a vehicle part (%d/%d). Parts bypass repair stat requirements.",
            unifiedVehicleParts(), kUnifiedVehiclePartsRequired);
        displayMonitorAddMessage(message);
        return true;
    }
    displayMonitorAddMessage(unifiedVehicleObjectWasSalvaged(object->id)
        ? "This wreck has already been stripped for usable parts."
        : "Repair needs Repair 50, INT 7 and Luck 6; Science 40 can salvage a part.");
    return true;
}

inline void unifiedVehicleRememberLoadedPrototype(UnifiedVehicleType type, int pid)
{
    int index = static_cast<int>(type);
    if (index < 0 || index >= static_cast<int>(UnifiedVehicleType::Count)) return;
    if (gUnifiedVehicleLoadedPrototypePids[index] == -1)
        gUnifiedVehicleLoadedPrototypePids[index] = pid;
}

inline void unifiedVehicleIndexLoadedMapPrototypes()
{
    int indexed = 0;
    for (Object* object = objectFindFirst(); object != nullptr; object = objectFindNext()) {
        UnifiedVehicleType type = unifiedVehicleTypeForObject(object);
        if (type == UnifiedVehicleType::None) continue;
        int index = static_cast<int>(type);
        if (gUnifiedVehicleLoadedPrototypePids[index] == -1) {
            gUnifiedVehicleLoadedPrototypePids[index] = object->pid;
            indexed++;
        }
    }
    if (indexed != 0)
        debugPrint("[VEHICLE] indexed=%d loaded-map wreck prototypes without global proto scan\n", indexed);
}

inline int unifiedVehicleFindPrototype(UnifiedVehicleType requested)
{
    int index = static_cast<int>(requested);
    if (index < 0 || index >= static_cast<int>(UnifiedVehicleType::Count)) return -1;

    int loadedPid = gUnifiedVehicleLoadedPrototypePids[index];
    if (loadedPid != -1) return loadedPid;

    // The Highwayman is the only stock vehicle PID guaranteed by the engine.
    // Never sweep proto_max_id here: protoGetProto retains every scenery proto
    // it touches and eventually fragments the legacy movable heap.
    if (requested == UnifiedVehicleType::Highwayman) {
        Proto* proto = nullptr;
        if (protoGetProto(PROTO_ID_CAR, &proto) == 0) {
            gUnifiedVehicleLoadedPrototypePids[index] = PROTO_ID_CAR;
            return PROTO_ID_CAR;
        }
    }
    return -1;
}

} // namespace fallout
#endif
