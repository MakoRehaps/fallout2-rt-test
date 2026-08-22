#ifndef LOCAL_COOP_MODE_SYNC_H
#define LOCAL_COOP_MODE_SYNC_H

#include "game.h"
#include "input.h"
#include "local_coop.h"

namespace fallout {

inline bool gLocalCoopModeSyncOwnsPlayerOne = false;
inline bool gLocalCoopModeSyncTickerInstalled = false;

inline LocalCoopUiMode localCoopLegacyModeForPlayerOne()
{
    if (GameMode::isInGameMode(GameMode::kDialog)) {
        return LocalCoopUiMode::Dialogue;
    }

    if (GameMode::isInGameMode(GameMode::kBarter)) {
        return LocalCoopUiMode::Barter;
    }

    if (GameMode::isInGameMode(GameMode::kLoot)) {
        return LocalCoopUiMode::Loot;
    }

    if (GameMode::isInGameMode(GameMode::kPipboy)) {
        return LocalCoopUiMode::PipBoy;
    }

    if (GameMode::isInGameMode(GameMode::kEditor)
        || GameMode::isInGameMode(GameMode::kHero)
        || GameMode::isInGameMode(GameMode::kSkilldex)) {
        return LocalCoopUiMode::Character;
    }

    // Do not claim kInventory here. The co-op inventory is intentionally its
    // own live overlay and can be independently open for any player.
    return LocalCoopUiMode::World;
}

inline void localCoopSyncLegacyModes()
{
    if (!gLocalCoopInitialized) {
        return;
    }

    LocalCoopPlayer& playerOne = gLocalCoopPlayers[0];
    LocalCoopUiMode legacyMode = localCoopLegacyModeForPlayerOne();

    if (legacyMode != LocalCoopUiMode::World) {
        // Only P1 owns global story/UI actions. P2-P4 keep whatever local UI
        // state they selected (normally World) and their controller tickers stay
        // active while P1 is in dialogue/barter/Pip-Boy/etc.
        playerOne.uiMode = legacyMode;
        gLocalCoopModeSyncOwnsPlayerOne = true;
        return;
    }

    if (gLocalCoopModeSyncOwnsPlayerOne) {
        playerOne.uiMode = LocalCoopUiMode::World;
        gLocalCoopModeSyncOwnsPlayerOne = false;
    }
}

inline void localCoopModeSyncTicker()
{
    localCoopSyncLegacyModes();
}

inline void localCoopModeSyncEnsureTicker()
{
    if (!gLocalCoopModeSyncTickerInstalled) {
        tickersAdd(localCoopModeSyncTicker);
        gLocalCoopModeSyncTickerInstalled = true;
    }
}

} // namespace fallout

#endif /* LOCAL_COOP_MODE_SYNC_H */
