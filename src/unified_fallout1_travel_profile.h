#ifndef UNIFIED_FALLOUT1_TRAVEL_PROFILE_H
#define UNIFIED_FALLOUT1_TRAVEL_PROFILE_H

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

#include "color.h"
#include "game.h"
#include "input.h"
#include "kb.h"
#include "local_coop.h"
#include "map.h"
#include "object.h"
#include "party_member.h"
#include "perk.h"
#include "queue.h"
#include "random.h"
#include "skill.h"
#include "stat.h"
#include "svga.h"
#include "unified_campaign.h"
#include "unified_fallout1_encounter_runtime.h"
#include "unified_fallout1_worldmap_globals.h"
#include "unified_worldmap_grid_profile.h"
#include "unified_worldmap_profile.h"
#include "unified_worldmap_state_profile.h"
#include "window_manager.h"

namespace fallout {

// True stock Fallout 2 entry points. These declarations are intentionally made
// before the call-site remaps at the bottom of this file.
void wmWorldMap();
void wmTownMap();

inline constexpr int kUnifiedFallout1LoadMapIndexGvar = 32;
inline constexpr int kUnifiedFallout1TravelMaxX = 1399;
inline constexpr int kUnifiedFallout1TravelMaxY = 1499;

struct UnifiedFallout1TownEntrance {
    const char* mapName;
    int loadMapIndex;
};

// Original Fallout 1 TownHotSpots map/index contract. Coordinates from the
// original town-map art are deliberately omitted here: this first backend is a
// controller-native list, not a virtual-mouse recreation of the old hotspots.
inline constexpr UnifiedFallout1TownEntrance kUnifiedFallout1TownEntrances[12][6] = {
    { { "V13ENT.MAP", 0 }, { "VAULT13.MAP", 1 }, { "VAULT13.MAP", 2 }, { "VAULT13.MAP", 3 }, { nullptr, 0 }, { nullptr, 0 } },
    { { "VAULTENT.MAP", 0 }, { "VAULTBUR.MAP", 1 }, { "VAULTBUR.MAP", 2 }, { "VAULTBUR.MAP", 3 }, { nullptr, 0 }, { nullptr, 0 } },
    { { "SHADYW.MAP", 1 }, { "SHADYE.MAP", 2 }, { "SHADYE.MAP", 3 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } },
    { { "JUNKENT.MAP", 3 }, { "JUNKKILL.MAP", 2 }, { "JUNKCSNO.MAP", 1 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } },
    { { "RAIDERS.MAP", 1 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } },
    { { "HOTEL.MAP", 3 }, { "HALLDED.MAP", 2 }, { "WATRSHD.MAP", 1 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } },
    { { "HUBENT.MAP", 1 }, { "HUBDWNTN.MAP", 3 }, { "HUBHEIGT.MAP", 5 }, { "HUBOLDTN.MAP", 2 }, { "HUBWATER.MAP", 4 }, { "DETHCLAW.MAP", 0 } },
    { { "BROHDENT.MAP", 1 }, { "BROHD12.MAP", 2 }, { "BROHD12.MAP", 3 }, { "BROHD34.MAP", 4 }, { "BROHD34.MAP", 5 }, { nullptr, 0 } },
    { { "MBENT.MAP", 0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } },
    { { "GLOWENT.MAP", 0 }, { "GLOW1.MAP", 1 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } },
    { { "LAADYTUM.MAP", 3 }, { "LABLADES.MAP", 4 }, { "LAFOLLWR.MAP", 5 }, { "LAGUNRUN.MAP", 1 }, { "LARIPPER.MAP", 2 }, { nullptr, 0 } },
    { { "CHILDRN1.MAP", 0 }, { "CHILDRN1.MAP", 1 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } },
};

struct UnifiedFallout1PadEdges {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    bool accept = false;
    bool cancel = false;
};

inline UnifiedFallout1PadEdges unifiedFallout1TravelReadPadEdges(UnifiedFallout1PadEdges& previous)
{
    UnifiedFallout1PadEdges current {};
    SDL_GameController* controller = nullptr;
    if (gLocalCoopInitialized
        && gLocalCoopPlayers[0].connected
        && gLocalCoopPlayers[0].controller != nullptr) {
        controller = gLocalCoopPlayers[0].controller;
    }

    if (controller != nullptr) {
        current.up = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP) != 0;
        current.down = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN) != 0;
        current.left = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT) != 0;
        current.right = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) != 0;
        current.accept = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A) != 0;
        current.cancel = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_B) != 0;
    }

    UnifiedFallout1PadEdges edges {};
    edges.up = current.up && !previous.up;
    edges.down = current.down && !previous.down;
    edges.left = current.left && !previous.left;
    edges.right = current.right && !previous.right;
    edges.accept = current.accept && !previous.accept;
    edges.cancel = current.cancel && !previous.cancel;
    previous = current;
    return edges;
}

inline void unifiedFallout1TravelSetGlobal(int index, int value)
{
    if (gGameGlobalVars != nullptr && index >= 0 && index < gGameGlobalVarsLength) {
        gGameGlobalVars[index] = value;
    }
}

inline int unifiedFallout1TravelGetGlobal(int index)
{
    if (gGameGlobalVars == nullptr || index < 0 || index >= gGameGlobalVarsLength) {
        return 0;
    }
    return gGameGlobalVars[index];
}

inline int unifiedFallout1TravelOpenWindow(const char* title)
{
    if (!gWindowSystemInitialized) {
        return -1;
    }

    int width = std::min(screenGetWidth(), 520);
    int height = std::min(screenGetVisibleHeight(), 390);
    int x = std::max(0, (screenGetWidth() - width) / 2);
    int y = std::max(0, (screenGetVisibleHeight() - height) / 2);
    int win = windowCreate(x, y, width, height, _colorTable[0], WINDOW_MOVE_ON_TOP);
    if (win == -1) {
        return -1;
    }

    windowDrawBorder(win);
    windowDrawText(win, title, width - 24, 12, 10, _colorTable[992]);
    windowShow(win);
    return win;
}

inline int unifiedFallout1SelectKnownTown()
{
    unifiedFallout1WorldMapSyncFromGlobals();

    std::array<int, 12> towns {};
    int count = 0;
    for (int town = 0; town < 12; town++) {
        if (unifiedWmAreaIsKnown(town)) {
            towns[count++] = town;
        }
    }

    if (count == 0) {
        return -1;
    }

    int selected = 0;
    int currentTown = unifiedFallout1WorldMapGetStateConst().currentTown;
    for (int index = 0; index < count; index++) {
        if (towns[index] == currentTown) {
            selected = index;
            break;
        }
    }

    int win = unifiedFallout1TravelOpenWindow("FALLOUT WORLD MAP - SELECT DESTINATION");
    UnifiedFallout1PadEdges previous {};
    bool dirty = true;

    while (true) {
        if (dirty && win != -1) {
            int width = windowGetWidth(win);
            int height = windowGetHeight(win);
            windowFill(win, 4, 32, width - 8, height - 36, _colorTable[0]);
            windowDrawText(win, "D-PAD / ARROWS: SELECT     A / ENTER: TRAVEL     B / ESC: BACK", width - 24, 12, 36, _colorTable[992]);

            const UnifiedFallout1WorldMapState& state = unifiedFallout1WorldMapGetStateConst();
            char position[96];
            std::snprintf(position, sizeof(position), "PARTY POSITION: %d, %d", state.worldX, state.worldY);
            windowDrawText(win, position, width - 24, 12, 58, _colorTable[992]);

            for (int index = 0; index < count; index++) {
                char name[40] = {};
                unifiedWmGetAreaIdxName(towns[index], name);
                char line[80];
                std::snprintf(line, sizeof(line), "%c %s", index == selected ? '>' : ' ', name[0] != '\0' ? name : "UNKNOWN");
                windowDrawText(win, line, width - 40, 24, 86 + index * 21, _colorTable[992]);
            }
            windowRefresh(win);
            dirty = false;
        }

        int key = inputGetInput();
        UnifiedFallout1PadEdges edges = unifiedFallout1TravelReadPadEdges(previous);
        if (key == KEY_ARROW_UP || key == KEY_ARROW_LEFT || edges.up || edges.left) {
            selected = (selected + count - 1) % count;
            dirty = true;
        } else if (key == KEY_ARROW_DOWN || key == KEY_ARROW_RIGHT || edges.down || edges.right) {
            selected = (selected + 1) % count;
            dirty = true;
        } else if (key == KEY_RETURN || edges.accept) {
            if (win != -1) {
                windowDestroy(win);
            }
            return towns[selected];
        } else if (key == KEY_ESCAPE || edges.cancel) {
            if (win != -1) {
                windowDestroy(win);
            }
            return -1;
        }

        SDL_Delay(8);
    }
}

inline bool unifiedFallout1TownEntranceKnown(int town, int entrance)
{
    if (!unifiedFallout1TownIndexIsValid(town) || entrance < 0 || entrance >= 6) {
        return false;
    }

    unifiedFallout1WorldMapSyncFromGlobals();
    const UnifiedFallout1WorldMapState& state = unifiedFallout1WorldMapGetStateConst();
    return kUnifiedFallout1TownEntrances[town][entrance].mapName != nullptr
        && state.townSelectionKnowledge[town][entrance] != 0;
}

inline int unifiedFallout1LoadMapName(const char* mapName, int loadMapIndex)
{
    if (mapName == nullptr) {
        return -1;
    }

    unifiedFallout1TravelSetGlobal(kUnifiedFallout1LoadMapIndexGvar, loadMapIndex);

    char mutableName[32];
    std::strncpy(mutableName, mapName, sizeof(mutableName) - 1);
    mutableName[sizeof(mutableName) - 1] = '\0';
    return mapLoadByName(mutableName);
}

inline int unifiedFallout1SelectAndLoadTownEntrance(int town)
{
    if (!unifiedFallout1TownIndexIsValid(town)) {
        return -1;
    }

    // Preserve Fallout 1's post-destruction crater-map substitutions.
    if (town == 11 && unifiedFallout1TravelGetGlobal(kUnifiedFallout1MasterBlownGvar) != 0) {
        return unifiedFallout1LoadMapName("CHILDEAD.MAP", 0);
    }
    if (town == 8 && unifiedFallout1TravelGetGlobal(kUnifiedFallout1VatsBlownGvar) != 0) {
        return unifiedFallout1LoadMapName("MBDEAD.MAP", 0);
    }

    std::array<int, 6> entrances {};
    int count = 0;
    for (int entrance = 0; entrance < 6; entrance++) {
        if (unifiedFallout1TownEntranceKnown(town, entrance)) {
            entrances[count++] = entrance;
        }
    }

    // Original towns always have a first entrance once they become known. Be
    // defensive for older sidecars or partially synchronized quest state.
    if (count == 0 && kUnifiedFallout1TownEntrances[town][0].mapName != nullptr) {
        entrances[count++] = 0;
    }
    if (count == 0) {
        return -1;
    }

    int selected = 0;
    int win = unifiedFallout1TravelOpenWindow("FALLOUT TOWN MAP - SELECT ENTRANCE");
    UnifiedFallout1PadEdges previous {};
    bool dirty = true;

    while (true) {
        if (dirty && win != -1) {
            int width = windowGetWidth(win);
            int height = windowGetHeight(win);
            windowFill(win, 4, 32, width - 8, height - 36, _colorTable[0]);
            windowDrawText(win, "D-PAD / ARROWS: SELECT     A / ENTER: ENTER     B / ESC: WORLD MAP", width - 24, 12, 36, _colorTable[992]);

            char townName[40] = {};
            unifiedWmGetAreaIdxName(town, townName);
            windowDrawText(win, townName, width - 24, 12, 60, _colorTable[992]);

            for (int index = 0; index < count; index++) {
                int entrance = entrances[index];
                const UnifiedFallout1TownEntrance& entry = kUnifiedFallout1TownEntrances[town][entrance];
                char line[96];
                std::snprintf(line, sizeof(line), "%c ENTRANCE %d   %s", index == selected ? '>' : ' ', entrance + 1, entry.mapName);
                windowDrawText(win, line, width - 40, 24, 92 + index * 24, _colorTable[992]);
            }
            windowRefresh(win);
            dirty = false;
        }

        int key = inputGetInput();
        UnifiedFallout1PadEdges edges = unifiedFallout1TravelReadPadEdges(previous);
        if (key == KEY_ARROW_UP || key == KEY_ARROW_LEFT || edges.up || edges.left) {
            selected = (selected + count - 1) % count;
            dirty = true;
        } else if (key == KEY_ARROW_DOWN || key == KEY_ARROW_RIGHT || edges.down || edges.right) {
            selected = (selected + 1) % count;
            dirty = true;
        } else if (key == KEY_RETURN || edges.accept) {
            int entrance = entrances[selected];
            const UnifiedFallout1TownEntrance entry = kUnifiedFallout1TownEntrances[town][entrance];
            UnifiedFallout1WorldMapState& state = unifiedFallout1WorldMapGetState();
            state.currentTown = town;
            state.currentSection = entrance;
            unifiedFallout1MarkTownKnown(town, true);
            if (win != -1) {
                windowDestroy(win);
            }
            return unifiedFallout1LoadMapName(entry.mapName, entry.loadMapIndex);
        } else if (key == KEY_ESCAPE || edges.cancel) {
            if (win != -1) {
                windowDestroy(win);
            }
            return 0;
        }

        SDL_Delay(8);
    }
}

struct UnifiedFallout1TravelLine {
    int targetX;
    int targetY;
    int deltaX;
    int deltaY;
    int stepX;
    int stepY;
    int error;
};

inline UnifiedFallout1TravelLine unifiedFallout1TravelLineCreate(int x, int y, int targetX, int targetY)
{
    UnifiedFallout1TravelLine line {};
    line.targetX = targetX;
    line.targetY = targetY;
    line.deltaX = std::abs(targetX - x);
    line.deltaY = -std::abs(targetY - y);
    line.stepX = x < targetX ? 1 : -1;
    line.stepY = y < targetY ? 1 : -1;
    line.error = line.deltaX + line.deltaY;
    return line;
}

inline bool unifiedFallout1TravelLineStep(UnifiedFallout1TravelLine& line, int& x, int& y)
{
    if (x == line.targetX && y == line.targetY) {
        return false;
    }

    int doubled = line.error * 2;
    if (doubled >= line.deltaY) {
        line.error += line.deltaY;
        x += line.stepX;
    }
    if (doubled <= line.deltaX) {
        line.error += line.deltaX;
        y += line.stepY;
    }

    x = std::max(0, std::min(x, kUnifiedFallout1TravelMaxX));
    y = std::max(0, std::min(y, kUnifiedFallout1TravelMaxY));
    return true;
}

inline void unifiedFallout1TravelTiming(int& milesPerDay, int& timeAdder)
{
    int outdoorsman = gDude != nullptr ? skillGetValue(gDude, SKILL_OUTDOORSMAN) : 0;
    outdoorsman = std::max(0, std::min(outdoorsman, 100));
    milesPerDay = static_cast<int>((static_cast<float>(outdoorsman) / 100.0f) * 60.0f + 60.0f);
    milesPerDay = std::max(1, milesPerDay);

    float ticks = 864000.0f / static_cast<float>(milesPerDay);
    int pathfinder = gDude != nullptr ? perkGetRank(gDude, PERK_PATHFINDER) : 0;
    ticks *= 1.0f - static_cast<float>(pathfinder) * 0.25f;
    timeAdder = std::max(1, static_cast<int>(ticks));
}

inline int unifiedFallout1LoadEncounterMap(const UnifiedFallout1EncounterSelection& encounter)
{
    if (!encounter.triggered || encounter.mapIdx < 0) {
        return 0;
    }

    char name[24] = {};
    if (unifiedWmMapIdxToName(encounter.mapIdx, name, sizeof(name)) != 0) {
        return -1;
    }

    char fileName[32];
    std::snprintf(fileName, sizeof(fileName), "%s", name);
    UnifiedFallout1WorldMapState& state = unifiedFallout1WorldMapGetState();
    state.currentTown = -1;
    state.currentSection = 0;
    return unifiedFallout1LoadMapName(fileName, 0);
}

inline bool unifiedFallout1TravelAdvanceTime(int ticks)
{
    gameTimeAddTicks(ticks);
    return queueProcessEvents() == 0;
}

inline int unifiedFallout1TravelToTown(int town)
{
    if (!unifiedFallout1TownIndexIsValid(town)) {
        return -1;
    }

    UnifiedFallout1WorldMapState& state = unifiedFallout1WorldMapGetState();
    int targetX = unifiedFallout1TownWorldX(town);
    int targetY = unifiedFallout1TownWorldY(town);

    // Fallout 1 deliberately perturbs town destinations by +/-0..16 pixels.
    int offset = randomBetween(0, 16);
    targetX += randomBetween(0, 1) != 0 ? offset : -offset;
    offset = randomBetween(0, 16);
    targetY += randomBetween(0, 1) != 0 ? offset : -offset;
    targetX = std::max(0, std::min(targetX, kUnifiedFallout1TravelMaxX));
    targetY = std::max(0, std::min(targetY, kUnifiedFallout1TravelMaxY));

    UnifiedFallout1TravelLine line = unifiedFallout1TravelLineCreate(state.worldX, state.worldY, targetX, targetY);
    int milesPerDay;
    int timeAdder;
    unifiedFallout1TravelTiming(milesPerDay, timeAdder);

    int travelMile = 0;
    int mountainCounter = 0;
    int cityCounter = 0;
    int revealCounter = 0;
    int win = unifiedFallout1TravelOpenWindow("FALLOUT WORLD MAP - TRAVELLING");
    UnifiedFallout1PadEdges previous {};

    while (state.worldX != targetX || state.worldY != targetY) {
        int terrain = unifiedFallout1TerrainAt(state.worldX, state.worldY);
        bool shouldStep = true;
        bool extraStep = false;

        if (terrain == static_cast<int>(UnifiedFallout1Terrain::Mountain)) {
            mountainCounter++;
            shouldStep = (mountainCounter % 2) == 0;
        } else if (terrain == static_cast<int>(UnifiedFallout1Terrain::City)) {
            cityCounter++;
            extraStep = (cityCounter % 4) == 0;
        }

        if (shouldStep) {
            unifiedFallout1TravelLineStep(line, state.worldX, state.worldY);
            if (extraStep && (state.worldX != targetX || state.worldY != targetY)) {
                unifiedFallout1TravelLineStep(line, state.worldX, state.worldY);
            }
        }

        if (!unifiedFallout1TravelAdvanceTime(timeAdder)) {
            if (win != -1) {
                windowDestroy(win);
            }
            return 0;
        }

        if (++revealCounter >= 3) {
            revealCounter = 0;
            unifiedFallout1WorldGridRevealAround(state.worldX, state.worldY, 1);
        }

        travelMile++;
        if (travelMile >= milesPerDay) {
            travelMile = 0;
            _partyMemberRestingHeal(24);

            int luck = gDude != nullptr ? critterGetStat(gDude, STAT_LUCK) : 5;
            int explorer = gDude != nullptr ? perkGetRank(gDude, PERK_EXPLORER) : 0;
            UnifiedFallout1EncounterSelection encounter = unifiedFallout1SelectTravelEncounter(
                state.worldX,
                state.worldY,
                luck,
                explorer);
            if (encounter.triggered) {
                if (win != -1) {
                    windowDestroy(win);
                }
                return unifiedFallout1LoadEncounterMap(encounter);
            }
        }

        if (win != -1 && (travelMile % 8) == 0) {
            int width = windowGetWidth(win);
            int height = windowGetHeight(win);
            windowFill(win, 4, 32, width - 8, height - 36, _colorTable[0]);
            char name[40] = {};
            unifiedWmGetAreaIdxName(town, name);
            char lineText[128];
            std::snprintf(lineText, sizeof(lineText), "DESTINATION: %s", name);
            windowDrawText(win, lineText, width - 24, 12, 42, _colorTable[992]);
            std::snprintf(lineText, sizeof(lineText), "POSITION: %d, %d", state.worldX, state.worldY);
            windowDrawText(win, lineText, width - 24, 12, 68, _colorTable[992]);
            windowDrawText(win, "B / ESC: STOP TRAVELLING", width - 24, 12, 96, _colorTable[992]);
            windowRefresh(win);
        }

        int key = inputGetInput();
        UnifiedFallout1PadEdges edges = unifiedFallout1TravelReadPadEdges(previous);
        if (key == KEY_ESCAPE || edges.cancel) {
            if (win != -1) {
                windowDestroy(win);
            }
            state.currentTown = unifiedFallout1TownAtWorldPos(state.worldX, state.worldY);
            return 0;
        }
    }

    if (win != -1) {
        windowDestroy(win);
    }

    // The random +/-16 target remains inside the destination's 50px town cell.
    state.currentTown = town;
    state.currentSection = 0;
    unifiedFallout1MarkTownKnown(town, true);
    unifiedFallout1WorldGridRevealAround(state.worldX, state.worldY, 1);
    return unifiedFallout1SelectAndLoadTownEntrance(town);
}

inline void unifiedWmTownMap()
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        wmTownMap();
        return;
    }

    UnifiedFallout1WorldMapState& state = unifiedFallout1WorldMapGetState();
    int town = state.currentTown;
    if (!unifiedFallout1TownIndexIsValid(town)) {
        town = unifiedFallout1TownAtWorldPos(state.worldX, state.worldY);
    }

    if (unifiedFallout1TownIndexIsValid(town)) {
        unifiedFallout1SelectAndLoadTownEntrance(town);
    }
}

inline void unifiedWmWorldMap()
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        wmWorldMap();
        return;
    }

    unifiedFallout1WorldMapSyncFromGlobals();
    int destination = unifiedFallout1SelectKnownTown();
    if (destination == -1) {
        return;
    }

    UnifiedFallout1WorldMapState& state = unifiedFallout1WorldMapGetState();
    if (unifiedFallout1TownAtWorldPos(state.worldX, state.worldY) == destination) {
        state.currentTown = destination;
        unifiedFallout1SelectAndLoadTownEntrance(destination);
        return;
    }

    unifiedFallout1TravelToTown(destination);
}

} // namespace fallout

// scripts.cc includes scripts.h before worldmap.h, so its world/town-map request
// dispatch is redirected here. worldmap.cc includes worldmap.h first; its stock
// Fallout 2 function definitions therefore remain completely untouched.
#ifndef WORLD_MAP_H
#define wmWorldMap unifiedWmWorldMap
#define wmTownMap unifiedWmTownMap
#endif

#endif /* UNIFIED_FALLOUT1_TRAVEL_PROFILE_H */
