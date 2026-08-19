#ifndef GAME_H
#define GAME_H

#ifdef LOCAL_COOP_LOADSAVE_META
#include "unified_campaign.h"
#include "unified_fallout1_worldmap_state.h"
#endif

#include "game_vars.h"
#include "message.h"

namespace fallout {

typedef enum GameState {
    GAME_STATE_0,
    GAME_STATE_1,
    GAME_STATE_2,
    GAME_STATE_3,
    GAME_STATE_4,
    GAME_STATE_5,
} GameState;

extern int* gGameGlobalVars;
extern int gGameGlobalVarsLength;
extern const char* asc_5186C8;
extern int _game_user_wants_to_quit;

extern MessageList gMiscMessageList;

int gameInitWithOptions(const char* windowTitle, bool isMapper, int a3, int a4, int argc, char** argv);
void gameReset();

#ifdef LOCAL_COOP_LOADSAVE_META
inline void localCoopLoadSaveGameReset()
{
    unifiedCampaignRunBeforeGameResetHook();

    bool appliedCampaignMeta = unifiedCampaignApplyPendingSaveHeader();
    if (appliedCampaignMeta
        && unifiedCampaignGetActiveGame() == UnifiedGameId::Fallout1) {
        // New sidecars restore Fallout 1's complete original world-map payload.
        // Header-only sidecars from earlier co-op builds remain readable; they
        // receive Fallout 1's stock Vault 13 starting world-map state instead of
        // inheriting stale exploration data from a previous loaded slot.
        if (!unifiedFallout1WorldMapApplyPending()) {
            unifiedFallout1WorldMapResetCurrent();
        }

        // Stock gameReset calls wmWorldMap_reset after this wrapper. The F1
        // lifecycle adapter consumes this one-shot flag so the just-restored
        // sidecar survives that reset. Ordinary new-game resets do not set it.
        unifiedFallout1WorldMapPreserveNextReset();
    } else {
        unifiedFallout1WorldMapClearPending();
    }

    gameReset();
}

inline bool localCoopLoadSaveShouldAbortForContentReload()
{
    if (!unifiedCampaignShouldAbortLoadForContentReload()) {
        return false;
    }

    // Cross-profile loading intentionally restarts through the main menu. Do not
    // leave a payload staged from the old content profile; opening the slot again
    // after rebootstrap will stage it from the same COOPMETA.SAV file.
    unifiedFallout1WorldMapClearPending();

    // If this load was requested while playing, leave the gameplay loop through
    // its normal teardown path. The main-menu initializer then performs the
    // full engine rebootstrap into the requested Fallout content profile.
    _game_user_wants_to_quit = 2;
    return true;
}

#define gameReset() \
    do { \
        if (localCoopLoadSaveShouldAbortForContentReload()) { \
            return -1; \
        } \
        localCoopLoadSaveGameReset(); \
    } while (0)
#endif

void gameExit();
int gameHandleKey(int eventCode, bool isInCombatMode);
void gameUiDisable(int a1);
void gameUiEnable();
bool gameUiIsDisabled();
int gameGetGlobalVar(int var);
int gameSetGlobalVar(int var, int value);
int globalVarsRead(const char* path, const char* section, int* variablesListLengthPtr, int** variablesListPtr);
int gameGetState();
int gameRequestState(int newGameState);
void gameUpdateState();
int showQuitConfirmationDialog();

int gameShowDeathDialog(const char* message);
void* gameGetGlobalPointer(int var);
int gameSetGlobalPointer(int var, void* value);

class GameMode {
public:
    enum Flags {
        kWorldmap = 0x1,
        kDialog = 0x4,
        kOptions = 0x8,
        kSaveGame = 0x10,
        kLoadGame = 0x20,
        kCombat = 0x40,
        kPreferences = 0x80,
        kHelp = 0x100,
        kEditor = 0x200,
        kPipboy = 0x400,
        kPlayerTurn = 0x800,
        kInventory = 0x1000,
        kAutomap = 0x2000,
        kSkilldex = 0x4000,
        kUseOn = 0x8000,
        kLoot = 0x10000,
        kBarter = 0x20000,
        kHero = 0x40000,
        kDialogReview = 0x80000,
        kCounter = 0x100000,
        kSpecial = 0x80000000,
    };

    static void enterGameMode(int gameMode);
    static void exitGameMode(int gameMode);
    static bool isInGameMode(int gameMode);
    static int getCurrentGameMode() { return currentGameMode; }

private:
    static int currentGameMode;
};

class ScopedGameMode {
public:
    ScopedGameMode(int gameMode);
    ~ScopedGameMode();

private:
    int gameMode;
};

} // namespace fallout

#endif /* GAME_H */
