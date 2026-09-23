#ifndef PLAYERBOTS_RAID_TOCMULTIPLIERS_TWINVALKYR_H
#define PLAYERBOTS_RAID_TOCMULTIPLIERS_TWINVALKYR_H

#include <vector>

#include "Multiplier.h"
#include "ToCMultipliers_Shared.h"

// Keep the twin tanks anchored on Fjola/Eydis instead of drifting with the combat formation
class TwinValkyrControlTankMovementMultiplier : public Multiplier
{
public:
    TwinValkyrControlTankMovementMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "twin valkyr control tank movement multiplier") {}
    float GetValue(Action* action) override;
};

// While a non-tank bot must swap essence (vortex/touch colour mismatch), suppress every other movement
// so nothing competes with the run to the portal
class TwinValkyrPrioritizeEssenceSwapMultiplier : public Multiplier
{
public:
    TwinValkyrPrioritizeEssenceSwapMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "twin valkyr prioritize essence swap multiplier") {}
    float GetValue(Action* action) override;
};

void AddToCTwinValkyrMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers);

ToCBurstWindow ToCTwinValkyrBurstWindow(PlayerbotAI* botAI);

#endif
