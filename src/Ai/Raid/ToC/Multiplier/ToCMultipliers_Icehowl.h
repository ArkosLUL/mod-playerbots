#ifndef PLAYERBOTS_RAID_TOCMULTIPLIERS_ICEHOWL_H
#define PLAYERBOTS_RAID_TOCMULTIPLIERS_ICEHOWL_H

#include <vector>

#include "Multiplier.h"

// Make sure clearing Icehowl's charge lane overrides formation/avoidance/chase movement
class IcehowlSuppressMovementDuringChargeMultiplier : public Multiplier
{
public:
    IcehowlSuppressMovementDuringChargeMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "icehowl suppress movement during charge multiplier") {}
    float GetValue(Action* action) override;
};

void AddToCIcehowlMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers);

#endif
