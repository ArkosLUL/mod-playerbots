#ifndef PLAYERBOTS_RAID_TOCMULTIPLIERS_ICEHOWL_H
#define PLAYERBOTS_RAID_TOCMULTIPLIERS_ICEHOWL_H

#include <vector>

#include "Multiplier.h"

// From the gaze to the end of the charge only attacks and the dodge move anyone. Anything else can walk
// a bot into the line, like melee chasing him to where the jump back lands.
class IcehowlChargeGuardMultiplier : public Multiplier
{
public:
    IcehowlChargeGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icehowl charge guard") {}
    float GetValue(Action* action) override;
};

void AddToCIcehowlMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers);

#endif
