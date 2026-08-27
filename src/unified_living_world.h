#ifndef UNIFIED_LIVING_WORLD_H
#define UNIFIED_LIVING_WORLD_H

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdint>
#include <cstring>

#include "debug.h"
#include "unified_campaign.h"
#include "unified_world_system.h"

namespace fallout {

// COOP_UNIFIED_LIVING_WORLD_V1
// Inspired by the uploaded FOnline living-world/faction/event scripts, but this
// is a native offline simulation for the unified Fallout 1/2 game. It does not
// require a FOnline server. Cells evolve off-screen and feed the encounter
// director when the party enters them.
inline constexpr uint32_t kUnifiedLivingWorldMagic = 0x31574C55; // ULW1
inline constexpr uint32_t kUnifiedLivingWorldVersion = 1;
inline constexpr int kUnifiedLivingFactionCount = 8;
inline constexpr Uint32 kUnifiedLivingStepMs = 15000;
inline constexpr Uint32 kUnifiedLivingAutosaveMs = 60000;
inline constexpr int kUnifiedLivingCellsPerStep = 16;

enum class UnifiedLivingFaction : uint8_t {
    Neutral = 0,
    Settlers = 1,
    Raiders = 2,
    Traders = 3,
    Law = 4,
    Mutants = 5,
    Brotherhood = 6,
    Enclave = 7,
};

enum class UnifiedLivingEvent : uint8_t {
    None = 0,
    Caravan = 1,
    Raid = 2,
    Patrol = 3,
    Migration = 4,
    Scavenge = 5,
    Rescue = 6,
    Siege = 7,
    Trade = 8,
    Revenge = 9,
};

struct UnifiedLivingFactionState {
    uint8_t strength;
    uint8_t supplies;
    uint8_t morale;
    uint8_t aggression;
};

struct UnifiedLivingCellState {
    uint8_t owner;
    uint8_t stability;
    uint8_t danger;
    uint8_t prosperity;
    uint8_t supplies;
    uint8_t conflict;
    uint8_t event;
    uint8_t factionA;
    uint8_t factionB;
    uint8_t eventStrengthA;
    uint8_t eventStrengthB;
    uint8_t eventAge;
};

struct UnifiedLivingWorldProfile {
    std::array<UnifiedLivingFactionState, kUnifiedLivingFactionCount> factions {};
    std::array<UnifiedLivingCellState, kUnifiedWorldSystemCellCount> cells {};
    uint32_t simulationTurn = 0;
    uint16_t simulationCursor = 0;
    uint16_t reserved = 0;
};

struct UnifiedLivingWorldState {
    uint32_t magic = kUnifiedLivingWorldMagic;
    uint32_t version = kUnifiedLivingWorldVersion;
    std::array<UnifiedLivingWorldProfile, kUnifiedWorldSystemGameCount> worlds {};
};

inline UnifiedLivingWorldState gUnifiedLivingWorld;
inline bool gUnifiedLivingWorldInitialized = false;
inline Uint32 gUnifiedLivingWorldNextStep = 0;
inline Uint32 gUnifiedLivingWorldNextSave = 0;
inline uint32_t gUnifiedLivingWorldRng = 0x6C697669; // "livi"

inline uint32_t unifiedLivingMix(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

inline uint32_t unifiedLivingRandom()
{
    gUnifiedLivingWorldRng = unifiedLivingMix(gUnifiedLivingWorldRng + 0x9E3779B9u);
    return gUnifiedLivingWorldRng;
}

inline int unifiedLivingRand(int minimum, int maximum)
{
    if (maximum <= minimum) return minimum;
    return minimum + static_cast<int>(unifiedLivingRandom() % static_cast<uint32_t>(maximum - minimum + 1));
}

inline const char* unifiedLivingFactionName(int faction)
{
    static const char* names[kUnifiedLivingFactionCount] = {
        "neutral", "settlers", "raiders", "traders", "law", "mutants", "brotherhood", "enclave"
    };
    return faction >= 0 && faction < kUnifiedLivingFactionCount ? names[faction] : "unknown";
}

inline const char* unifiedLivingEventName(UnifiedLivingEvent event)
{
    switch (event) {
    case UnifiedLivingEvent::Caravan: return "caravan";
    case UnifiedLivingEvent::Raid: return "raid";
    case UnifiedLivingEvent::Patrol: return "patrol";
    case UnifiedLivingEvent::Migration: return "migration";
    case UnifiedLivingEvent::Scavenge: return "scavenge";
    case UnifiedLivingEvent::Rescue: return "rescue";
    case UnifiedLivingEvent::Siege: return "siege";
    case UnifiedLivingEvent::Trade: return "trade";
    case UnifiedLivingEvent::Revenge: return "revenge";
    default: return "none";
    }
}

inline void unifiedLivingSeedProfile(int gameIndex)
{
    UnifiedWorldSystemState& base = unifiedWorldSystemGetState();
    UnifiedLivingWorldProfile& world = gUnifiedLivingWorld.worlds[gameIndex];
    uint32_t worldSeed = base.worlds[gameIndex].seed ^ static_cast<uint32_t>(0xA17E0000u + gameIndex);

    for (int faction = 0; faction < kUnifiedLivingFactionCount; faction++) {
        uint32_t seed = unifiedLivingMix(worldSeed ^ static_cast<uint32_t>(faction * 0x45D9F3Bu));
        world.factions[faction].strength = static_cast<uint8_t>(45 + seed % 41);
        world.factions[faction].supplies = static_cast<uint8_t>(40 + (seed >> 8) % 51);
        world.factions[faction].morale = static_cast<uint8_t>(45 + (seed >> 16) % 41);
        world.factions[faction].aggression = static_cast<uint8_t>(20 + (seed >> 24) % 61);
    }
    world.factions[static_cast<int>(UnifiedLivingFaction::Neutral)].aggression = 0;

    for (int index = 0; index < kUnifiedWorldSystemCellCount; index++) {
        uint32_t seed = unifiedLivingMix(worldSeed ^ base.worlds[gameIndex].cells[index].seed ^ static_cast<uint32_t>(index));
        UnifiedLivingCellState& cell = world.cells[index];
        int ownerRoll = static_cast<int>(seed % 100);
        if (ownerRoll < 36) cell.owner = static_cast<uint8_t>(UnifiedLivingFaction::Neutral);
        else cell.owner = static_cast<uint8_t>(1 + ((seed >> 7) % (kUnifiedLivingFactionCount - 1)));
        cell.stability = static_cast<uint8_t>(30 + (seed >> 5) % 61);
        cell.danger = static_cast<uint8_t>(15 + (seed >> 12) % 71);
        cell.prosperity = static_cast<uint8_t>(20 + (seed >> 18) % 66);
        cell.supplies = static_cast<uint8_t>(25 + (seed >> 24) % 66);
        cell.conflict = static_cast<uint8_t>((seed >> 3) % 31);
        cell.event = static_cast<uint8_t>(UnifiedLivingEvent::None);
    }
}

inline const char* unifiedLivingSavePath()
{
    return "SAVEGAME/LIVINGWORLD.SAV";
}

inline bool unifiedLivingLoad()
{
    FILE* file = std::fopen(unifiedLivingSavePath(), "rb");
    if (file == nullptr) return false;
    UnifiedLivingWorldState loaded {};
    size_t count = std::fread(&loaded, 1, sizeof(loaded), file);
    std::fclose(file);
    if (count != sizeof(loaded)
        || loaded.magic != kUnifiedLivingWorldMagic
        || loaded.version != kUnifiedLivingWorldVersion) {
        return false;
    }
    gUnifiedLivingWorld = loaded;
    return true;
}

inline void unifiedLivingSave()
{
    FILE* file = std::fopen(unifiedLivingSavePath(), "wb");
    if (file == nullptr) return;
    std::fwrite(&gUnifiedLivingWorld, 1, sizeof(gUnifiedLivingWorld), file);
    std::fclose(file);
}

inline void unifiedLivingEnsureInitialized()
{
    if (gUnifiedLivingWorldInitialized) return;
    gUnifiedLivingWorldInitialized = true;
    if (!unifiedLivingLoad()) {
        gUnifiedLivingWorld = UnifiedLivingWorldState {};
        for (int gameIndex = 0; gameIndex < kUnifiedWorldSystemGameCount; gameIndex++) {
            unifiedLivingSeedProfile(gameIndex);
        }
        unifiedLivingSave();
    }
    gUnifiedLivingWorldRng ^= gUnifiedLivingWorld.worlds[0].simulationTurn * 0x9E3779B9u;
    gUnifiedLivingWorldNextStep = SDL_GetTicks() + kUnifiedLivingStepMs;
    gUnifiedLivingWorldNextSave = SDL_GetTicks() + kUnifiedLivingAutosaveMs;
}

inline int unifiedLivingNeighborIndex(int index, int direction)
{
    int x = index % kUnifiedWorldSystemColumns;
    int y = index / kUnifiedWorldSystemColumns;
    if (direction == 0) y--;
    else if (direction == 1) x++;
    else if (direction == 2) y++;
    else x--;
    return unifiedWorldSystemCellIndex(x, y);
}

inline void unifiedLivingStartEvent(UnifiedLivingCellState& cell, UnifiedLivingEvent event, int a, int b, int sa, int sb)
{
    cell.event = static_cast<uint8_t>(event);
    cell.factionA = static_cast<uint8_t>(std::clamp(a, 0, kUnifiedLivingFactionCount - 1));
    cell.factionB = static_cast<uint8_t>(std::clamp(b, 0, kUnifiedLivingFactionCount - 1));
    cell.eventStrengthA = static_cast<uint8_t>(std::clamp(sa, 0, 100));
    cell.eventStrengthB = static_cast<uint8_t>(std::clamp(sb, 0, 100));
    cell.eventAge = 0;
}

inline void unifiedLivingSimulateCell(int gameIndex, int index)
{
    UnifiedLivingWorldProfile& world = gUnifiedLivingWorld.worlds[gameIndex];
    UnifiedLivingCellState& cell = world.cells[index];
    int owner = std::clamp(static_cast<int>(cell.owner), 0, kUnifiedLivingFactionCount - 1);
    UnifiedLivingFactionState& ownerState = world.factions[owner];

    if (cell.event != static_cast<uint8_t>(UnifiedLivingEvent::None)) {
        cell.eventAge = static_cast<uint8_t>(std::min(255, static_cast<int>(cell.eventAge) + 1));
        if (cell.eventAge > 7) {
            cell.event = static_cast<uint8_t>(UnifiedLivingEvent::None);
            cell.eventAge = 0;
        }
    }

    // Peaceful regions slowly recover; dangerous regions decay until someone
    // patrols, trades or wins control of them.
    if (owner != static_cast<int>(UnifiedLivingFaction::Neutral)) {
        cell.stability = static_cast<uint8_t>(std::min(100, static_cast<int>(cell.stability) + (ownerState.morale > 55 ? 1 : 0)));
        cell.supplies = static_cast<uint8_t>(std::min(100, static_cast<int>(cell.supplies) + (ownerState.supplies > 50 ? 1 : 0)));
    }
    if (cell.danger > 0 && cell.stability > 65) cell.danger--;

    int neighborIndex = unifiedLivingNeighborIndex(index, unifiedLivingRand(0, 3));
    if (neighborIndex == -1) return;
    UnifiedLivingCellState& neighbor = world.cells[neighborIndex];
    int other = std::clamp(static_cast<int>(neighbor.owner), 0, kUnifiedLivingFactionCount - 1);

    if (owner != 0 && owner == other) {
        // FOnline-style caravans/trade move resources between friendly cells.
        int transfer = std::max(0, (static_cast<int>(cell.supplies) - static_cast<int>(neighbor.supplies)) / 5);
        if (transfer > 0) {
            cell.supplies = static_cast<uint8_t>(std::max(0, static_cast<int>(cell.supplies) - transfer));
            neighbor.supplies = static_cast<uint8_t>(std::min(100, static_cast<int>(neighbor.supplies) + transfer));
        }
        if (unifiedLivingRand(0, 99) < 14 && cell.event == 0) {
            unifiedLivingStartEvent(cell, transfer > 0 ? UnifiedLivingEvent::Caravan : UnifiedLivingEvent::Trade,
                owner, owner, 55 + transfer, 0);
            cell.prosperity = static_cast<uint8_t>(std::min(100, static_cast<int>(cell.prosperity) + 2));
        }
        return;
    }

    if (other == 0 && owner != 0 && unifiedLivingRand(0, 99) < 12) {
        neighbor.owner = static_cast<uint8_t>(owner);
        neighbor.stability = static_cast<uint8_t>(std::max(25, static_cast<int>(neighbor.stability)));
        unifiedLivingStartEvent(neighbor, UnifiedLivingEvent::Migration, owner, 0, 45, 15);
        return;
    }

    if (owner == 0 || other == 0 || owner == other) {
        if (cell.event == 0 && unifiedLivingRand(0, 99) < 8) {
            UnifiedLivingEvent event = cell.danger > 60 ? UnifiedLivingEvent::Rescue : UnifiedLivingEvent::Scavenge;
            unifiedLivingStartEvent(cell, event, owner, 0, 35 + cell.prosperity / 2, 25 + cell.danger / 2);
        }
        return;
    }

    UnifiedLivingFactionState& otherState = world.factions[other];
    int attack = ownerState.strength + ownerState.morale / 2 + ownerState.aggression / 3 + cell.supplies / 4 + unifiedLivingRand(0, 30);
    int defense = otherState.strength + otherState.morale / 2 + neighbor.stability / 2 + neighbor.supplies / 4 + unifiedLivingRand(0, 30);
    int lead = attack - defense;

    cell.conflict = static_cast<uint8_t>(std::min(100, static_cast<int>(cell.conflict) + 3));
    neighbor.conflict = static_cast<uint8_t>(std::min(100, static_cast<int>(neighbor.conflict) + 3));
    cell.danger = static_cast<uint8_t>(std::min(100, static_cast<int>(cell.danger) + 2));
    neighbor.danger = static_cast<uint8_t>(std::min(100, static_cast<int>(neighbor.danger) + 3));

    UnifiedLivingEvent event = std::abs(lead) < 12 ? UnifiedLivingEvent::Siege : UnifiedLivingEvent::Raid;
    if (cell.event == 0) unifiedLivingStartEvent(cell, event, owner, other, std::clamp(attack / 2, 1, 100), std::clamp(defense / 2, 1, 100));
    if (neighbor.event == 0) unifiedLivingStartEvent(neighbor, event, other, owner, std::clamp(defense / 2, 1, 100), std::clamp(attack / 2, 1, 100));

    // Territory only flips after a decisive off-screen win, so borders move but
    // do not flicker every simulation pass.
    if (lead > 28 && neighbor.conflict > 24) {
        neighbor.owner = static_cast<uint8_t>(owner);
        neighbor.stability = 30;
        neighbor.conflict = 8;
        unifiedLivingStartEvent(neighbor, UnifiedLivingEvent::Revenge, other, owner, 45, 60);
    } else if (lead < -28 && cell.conflict > 24) {
        cell.owner = static_cast<uint8_t>(other);
        cell.stability = 30;
        cell.conflict = 8;
        unifiedLivingStartEvent(cell, UnifiedLivingEvent::Revenge, owner, other, 45, 60);
    }
}

inline void unifiedLivingRuntimeTick()
{
    unifiedLivingEnsureInitialized();
    Uint32 now = SDL_GetTicks();
    if (static_cast<Sint32>(now - gUnifiedLivingWorldNextStep) >= 0) {
        int activeGameIndex = unifiedWorldSystemGameIndex(unifiedCampaignGetActiveGame());
        UnifiedLivingWorldProfile& world = gUnifiedLivingWorld.worlds[activeGameIndex];
        for (int n = 0; n < kUnifiedLivingCellsPerStep; n++) {
            int index = world.simulationCursor % kUnifiedWorldSystemCellCount;
            unifiedLivingSimulateCell(activeGameIndex, index);
            world.simulationCursor = static_cast<uint16_t>((world.simulationCursor + 1) % kUnifiedWorldSystemCellCount);
        }
        world.simulationTurn++;
        gUnifiedLivingWorldNextStep = now + kUnifiedLivingStepMs;
    }
    if (static_cast<Sint32>(now - gUnifiedLivingWorldNextSave) >= 0) {
        unifiedLivingSave();
        gUnifiedLivingWorldNextSave = now + kUnifiedLivingAutosaveMs;
    }
}

inline UnifiedLivingCellState* unifiedLivingCurrentCell()
{
    unifiedLivingEnsureInitialized();
    UnifiedGameId game = unifiedCampaignGetActiveGame();
    int gameIndex = unifiedWorldSystemGameIndex(game);
    UnifiedWorldSystemState& base = unifiedWorldSystemGetState();
    int x = base.travel.currentCellX[gameIndex];
    int y = base.travel.currentCellY[gameIndex];
    int index = unifiedWorldSystemCellIndex(x, y);
    if (index < 0) return nullptr;
    return &gUnifiedLivingWorld.worlds[gameIndex].cells[index];
}

// Returns the encounter-director objective index:
// 0 ambush, 1 hold, 2 scavenge, 3 rescue, 4 hunt, 5 escape, 6 crossfire.
inline int unifiedLivingEncounterObjectiveBias(int fallback)
{
    UnifiedLivingCellState* cell = unifiedLivingCurrentCell();
    if (cell == nullptr) return fallback;
    switch (static_cast<UnifiedLivingEvent>(cell->event)) {
    case UnifiedLivingEvent::Caravan: return cell->danger > 55 ? 0 : 5;
    case UnifiedLivingEvent::Raid: return 0;
    case UnifiedLivingEvent::Patrol: return 4;
    case UnifiedLivingEvent::Migration: return 5;
    case UnifiedLivingEvent::Scavenge: return 2;
    case UnifiedLivingEvent::Rescue: return 3;
    case UnifiedLivingEvent::Siege: return 1;
    case UnifiedLivingEvent::Trade: return 2;
    case UnifiedLivingEvent::Revenge: return 4;
    default: break;
    }
    if (cell->danger > 78) return 0;
    if (cell->conflict > 62) return 6;
    if (cell->prosperity > 74 && cell->danger < 38) return 2;
    return fallback;
}

inline void unifiedLivingRecordEncounterSetup(int hiddenLead)
{
    UnifiedLivingCellState* cell = unifiedLivingCurrentCell();
    if (cell == nullptr) return;
    int intensity = std::min(8, std::max(1, std::abs(hiddenLead) / 8));
    cell->conflict = static_cast<uint8_t>(std::min(100, static_cast<int>(cell->conflict) + intensity));
    cell->danger = static_cast<uint8_t>(std::min(100, static_cast<int>(cell->danger) + std::max(1, intensity / 2)));
    if (cell->event == 0 && intensity >= 5) {
        unifiedLivingStartEvent(*cell, UnifiedLivingEvent::Revenge, cell->owner, 0, 50, 40);
    }
}

inline void unifiedLivingDebugCurrentCell()
{
    UnifiedLivingCellState* cell = unifiedLivingCurrentCell();
    if (cell == nullptr) return;
    debugPrint("[LIVING WORLD] owner=%s event=%s stability=%d danger=%d prosperity=%d supplies=%d conflict=%d\n",
        unifiedLivingFactionName(cell->owner),
        unifiedLivingEventName(static_cast<UnifiedLivingEvent>(cell->event)),
        cell->stability, cell->danger, cell->prosperity, cell->supplies, cell->conflict);
}

} // namespace fallout

#endif
