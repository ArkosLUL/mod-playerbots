#ifndef PLAYERBOTS_RAID_TOCMULTIPLIERS_JARAXXUS_H
#define PLAYERBOTS_RAID_TOCMULTIPLIERS_JARAXXUS_H

#include <vector>

#include "Multiplier.h"
#include "ToCMultipliers_Shared.h"

// Keep tanks anchored on Jaraxxus and his adds instead of drifting with the combat formation
class JaraxxusControlTankMovementMultiplier : public Multiplier
{
public:
    JaraxxusControlTankMovementMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "jaraxxus control tank movement multiplier") {}
    float GetValue(Action* action) override;
};

void AddToCJaraxxusMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers);

ToCBurstWindow ToCJaraxxusBurstWindow(PlayerbotAI* botAI);

#endif
