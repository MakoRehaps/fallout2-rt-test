#ifndef UNIFIED_WORLDMAP_VEHICLE_PROFILE_H
#define UNIFIED_WORLDMAP_VEHICLE_PROFILE_H

#include "unified_campaign.h"

namespace fallout {

int wmCarUseGas(int amount);
int wmCarFillGas(int amount);
int wmCarGasAmount();
bool wmCarIsOutOfGas();
int wmCarCurrentArea();
int wmCarGiveToParty();
void wmCarSetCurrentArea(int area);

inline int unifiedWmCarUseGas(int amount)
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmCarUseGas(amount);
    }

    (void)amount;
    return -1;
}

inline int unifiedWmCarFillGas(int amount)
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmCarFillGas(amount);
    }

    (void)amount;
    return -1;
}

inline int unifiedWmCarGasAmount()
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmCarGasAmount();
    }

    return 0;
}

inline bool unifiedWmCarIsOutOfGas()
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmCarIsOutOfGas();
    }

    return true;
}

inline int unifiedWmCarCurrentArea()
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmCarCurrentArea();
    }

    return -1;
}

inline int unifiedWmCarGiveToParty()
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        return wmCarGiveToParty();
    }

    return -1;
}

inline void unifiedWmCarSetCurrentArea(int area)
{
    if (unifiedCampaignGetActiveGame() != UnifiedGameId::Fallout1) {
        wmCarSetCurrentArea(area);
        return;
    }

    (void)area;
}

} // namespace fallout

#ifndef WORLD_MAP_H
#define wmCarUseGas unifiedWmCarUseGas
#define wmCarFillGas unifiedWmCarFillGas
#define wmCarGasAmount unifiedWmCarGasAmount
#define wmCarIsOutOfGas unifiedWmCarIsOutOfGas
#define wmCarCurrentArea unifiedWmCarCurrentArea
#define wmCarGiveToParty unifiedWmCarGiveToParty
#define wmCarSetCurrentArea unifiedWmCarSetCurrentArea
#endif

#endif /* UNIFIED_WORLDMAP_VEHICLE_PROFILE_H */
