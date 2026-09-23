#ifndef PLAYERBOTS_RAID_TOCMULTIPLIERS_NORTHRENDBEASTS_H
#define PLAYERBOTS_RAID_TOCMULTIPLIERS_NORTHRENDBEASTS_H

#include <vector>

#include "Multiplier.h"
#include "ToCMultipliers_Shared.h"

// Keep beast tanks anchored instead of drifting with the combat formation
class NorthrendBeastsControlTankMovementMultiplier : public Multiplier
{
public:
    NorthrendBeastsControlTankMovementMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "northrend beasts control tank movement multiplier") {}
    float GetValue(Action* action) override;
};

void AddToCNorthrendBeastsMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers);

ToCBurstWindow ToCNorthrendBeastsBurstWindow(PlayerbotAI* botAI);

#endif
