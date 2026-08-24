#ifndef MAP_H
#define MAP_H

#include "combat_defs.h"
#include "db.h"
#include "geometry.h"
#include "interpreter.h"
#include "local_coop_danger.h"
#include "map_defs.h"
#include "message.h"
#include "platform_compat.h"

namespace fallout {

#define ORIGINAL_ISO_WINDOW_WIDTH 640
#define ORIGINAL_ISO_WINDOW_HEIGHT 380

// TODO: Probably not needed -> replace with array?
typedef struct TileData {
    int field_0[SQUARE_GRID_SIZE];
} TileData;

typedef struct MapHeader {
    int version;
    char name[16];
    int enteringTile;
    int enteringElevation;
    int enteringRotation;
    int localVariablesCount;
    int scriptIndex;
    int flags;
    int darkness;
    int globalVariablesCount;
    int field_34;
    unsigned int lastVisitTime;
    int field_3C[44];
} MapHeader;

typedef struct MapTransition {
    int map;
    int elevation;
    int tile;
    int rotation;
} MapTransition;

typedef void IsoWindowRefreshProc(Rect* rect);

extern int gMapSid;
extern int* gMapLocalVars;
extern int* gMapGlobalVars;
extern int gMapLocalVarsLength;
extern int gMapGlobalVarsLength;
extern int gElevation;

extern MessageList gMapMessageList;
extern MapHeader gMapHeader;
extern TileData* _square[ELEVATION_COUNT];
extern int gIsoWindow;

int isoInit();
void isoReset();
void isoExit();
void _map_init();
void _map_exit();
void isoEnable();
bool isoDisable();
bool isoIsDisabled();

// Selected P1 modal source files are compiled with
// LOCAL_COOP_KEEP_ISO_LIVE. Redirect only their local call sites so opening a
// dialogue/Pip-Boy/character/skill window does not stop object animation or
// critter-script tickers. map.cc itself and all other systems still use the
// stock isoEnable/isoDisable implementation, so Options, Save/Load, movies and
// map transitions retain their normal global pause behavior.
#ifdef LOCAL_COOP_KEEP_ISO_LIVE
inline void localCoopModalIsoEnable()
{
}

inline bool localCoopModalIsoDisable()
{
    return false;
}

#define isoEnable localCoopModalIsoEnable
#define isoDisable localCoopModalIsoDisable
#endif

int mapSetElevation(int elevation);
int mapSetGlobalVar(int var, ProgramValue& value);
int mapGetGlobalVar(int var, ProgramValue& value);
int mapSetLocalVar(int var, ProgramValue& value);
int mapGetLocalVar(int var, ProgramValue& value);
int _map_malloc_local_var(int a1);
void mapSetStart(int a1, int a2, int a3);
char* mapGetName(int map_num, int elev);
bool _is_map_idx_same(int map_num1, int map_num2);
int _get_map_idx_same(int map_num1, int map_num2);
char* mapGetCityName(int map_num);
char* _map_get_description_idx_(int map_index);
int mapGetCurrentMap();
int mapScroll(int dx, int dy);
int mapSetEnteringLocation(int a1, int a2, int a3);
void mapNewMap();
int mapLoadByName(char* fileName);
int mapLoadById(int map_index);
int mapLoadSaved(char* fileName);
int _map_target_load_area();
int mapSetTransition(MapTransition* transition);
int mapHandleTransition();
int _map_save_in_game(bool a1);

// Script opcodes are the normal path for stairs, doors and scripted exits to
// request a different map. While realtime danger is active, consume that request
// without changing maps. The stock mapSetTransition definition in map.cc is not
// renamed because only interpreter_extra.cc defines the translation-unit marker.
inline int localCoopMapSetTransitionDispatch(MapTransition* transition)
{
    if (localCoopDangerBlocksMapExit()) {
        return 0;
    }
    return mapSetTransition(transition);
}

} // namespace fallout

#if defined(LOCAL_COOP_INTERPRETER_EXTRA_TRANSLATION_UNIT)
#define mapSetTransition localCoopMapSetTransitionDispatch
#endif

// Install profile-aware world-map semantics for translation units that enter
// through the ISO map layer. worldmap.cc includes worldmap.h first, so its
// WORLD_MAP_H guard suppresses these call-site remaps and preserves the stock
// Fallout 2 implementation as the fallback backend.
#include "unified_worldmap_profile.h"
#include "unified_worldmap_state_profile.h"
#include "unified_worldmap_grid_profile.h"
#include "unified_loaded_map_profile.h"
#include "unified_worldmap_audio_profile.h"
#include "unified_worldmap_vehicle_profile.h"
#include "unified_fallout1_encounter_runtime.h"
#include "unified_fallout1_encounter_bridge.h"
#include "unified_fallout1_worldmap_events.h"
#include "unified_worldmap_lifecycle_profile.h"

#endif /* MAP_H */
