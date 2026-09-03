#ifndef UNIFIED_WORLDMAP_AUDIO_PROFILE_H
#define UNIFIED_WORLDMAP_AUDIO_PROFILE_H

#include <cstring>

#include "unified_campaign.h"
#include "unified_worldmap_profile.h"

namespace fallout {

int wmSfxMaxCount();
int wmSfxRollNextIdx();
int wmSfxIdxName(int sfxIdx, char** namePtr);
int wmMapMusicStart();
int wmSetMapMusic(int mapIdx, const char* name);
int mapGetCurrentMap();
int _gsound_background_play_level_music(const char* name, int mode);
void backgroundSoundDelete();

// Exact fallout1-ce CityMusic table in F1 MAP_* order.
inline constexpr const char* kUnifiedFallout1MapMusic[kUnifiedFallout1MapCount] = {
    "07DESERT", // DESERT1
    "07DESERT", // DESERT2
    "07DESERT", // DESERT3
    "14NECRO", // HALLDED
    "14NECRO", // HOTEL
    "14NECRO", // WATRSHD
    "06VAULT", // VAULT13
    "13CARVRN", // VAULTENT
    "13CARVRN", // VAULTBUR
    "14NECRO", // VAULTNEC
    "12JUNKTN", // JUNKENT
    "12JUNKTN", // JUNKCSNO
    "12JUNKTN", // JUNKKILL
    "04BRTHRH", // BROHDENT
    "04BRTHRH", // BROHD12
    "04BRTHRH", // BROHD34
    "13CARVRN", // CAVES
    "11CHILRN", // CHILDRN1
    "11CHILRN", // CHILDRN2
    "11CHILRN", // CITY1
    "07DESERT", // COAST1
    "07DESERT", // COAST2
    "07DESERT", // COLATRUK
    "07DESERT", // FSAUSER
    "05RAIDER", // RAIDERS
    "15SHADY", // SHADYE
    "15SHADY", // SHADYW
    "09GLOW", // GLOWENT
    "10LABONE", // LAADYTUM
    "16FOLLOW", // LAFOLLWR
    "08VATS", // MBENT
    "08VATS", // MBSTRG12
    "08VATS", // MBVATS12
    "02MSTRLR", // MSTRLR12
    "02MSTRLR", // MSTRLR34
    "06VAULT", // V13ENT
    "01HUB", // HUBENT
    "13CARVRN", // DETHCLAW
    "01HUB", // HUBDWNTN
    "01HUB", // HUBHEIGT
    "01HUB", // HUBOLDTN
    "01HUB", // HUBWATER
    "09GLOW", // GLOW1
    "09GLOW", // GLOW2
    "10LABONE", // LABLADES
    "10LABONE", // LARIPPER
    "10LABONE", // LAGUNRUN
    "08VATS", // CHILDEAD
    "08VATS", // MBDEAD
    "07DESERT", // MOUNTN1
    "07DESERT", // MOUNTN2
    "07DESERT", // FOOT
    "07DESERT", // TARDIS
    "07DESERT", // TALKCOW
    "07DESERT", // USEDCAR
    "08VATS", // BRODEAD
    "07DESERT", // DESCRVN1
    "07DESERT", // DESCRVN2
    "07DESERT", // MNTCRVN1
    "07DESERT", // MNTCRVN2
    "07DESERT", // VIPERS
    "07DESERT", // DESCRVN3
    "07DESERT", // MNTCRVN3
    "07DESERT", // DESCRVN4
    "07DESERT", // MNTCRVN4
    "01HUB", // HUBMIS1
};

inline char gUnifiedFallout1MapMusicOverrides[kUnifiedFallout1MapCount][40] {};

inline const char* unifiedFallout1MapMusicName(int mapIdx)
{
    if (mapIdx < 0 || mapIdx >= kUnifiedFallout1MapCount) {
        return nullptr;
    }

    if (gUnifiedFallout1MapMusicOverrides[mapIdx][0] != '\0') {
        return gUnifiedFallout1MapMusicOverrides[mapIdx];
    }

    return kUnifiedFallout1MapMusic[mapIdx];
}

inline int unifiedWmSfxMaxCount()
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmSfxMaxCount();
    }

    // Fallout 1 has no F2 worldmap.txt per-map ambient-SFX list. Map/script SFX
    // continue to function normally; only the F2 table-driven scheduler is off.
    return 0;
}

inline int unifiedWmSfxRollNextIdx()
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmSfxRollNextIdx();
    }

    return -1;
}

inline int unifiedWmSfxIdxName(int sfxIdx, char** namePtr)
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmSfxIdxName(sfxIdx, namePtr);
    }

    (void)sfxIdx;
    if (namePtr != nullptr) {
        *namePtr = nullptr;
    }
    return -1;
}

inline int unifiedWmMapMusicStart()
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmMapMusicStart();
    }

    const char* music = unifiedFallout1MapMusicName(mapGetCurrentMap());
    if (music == nullptr || music[0] == '\0') {
        return -1;
    }

    return _gsound_background_play_level_music(music, 12);
}

inline int unifiedWmSetMapMusic(int mapIdx, const char* name)
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmSetMapMusic(mapIdx, name);
    }

    if (mapIdx < 0 || mapIdx >= kUnifiedFallout1MapCount || name == nullptr) {
        return -1;
    }

    std::strncpy(gUnifiedFallout1MapMusicOverrides[mapIdx], name, 39);
    gUnifiedFallout1MapMusicOverrides[mapIdx][39] = '\0';

    if (mapGetCurrentMap() == mapIdx) {
        backgroundSoundDelete();
        return unifiedWmMapMusicStart();
    }

    return 0;
}

inline void unifiedFallout1MapMusicResetOverrides()
{
    std::memset(gUnifiedFallout1MapMusicOverrides, 0, sizeof(gUnifiedFallout1MapMusicOverrides));
}

} // namespace fallout

#ifndef WORLD_MAP_H
#define wmSfxMaxCount unifiedWmSfxMaxCount
#define wmSfxRollNextIdx unifiedWmSfxRollNextIdx
#define wmSfxIdxName unifiedWmSfxIdxName
#define wmMapMusicStart unifiedWmMapMusicStart
#define wmSetMapMusic unifiedWmSetMapMusic
#endif

#endif /* UNIFIED_WORLDMAP_AUDIO_PROFILE_H */
