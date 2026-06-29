#ifndef PLAYERBOTS_TOCMULTIPLIERS_H
#define PLAYERBOTS_TOCMULTIPLIERS_H

#include "Multiplier.h"

// Make sure clearing Icehowl's charge lane overrides formation/avoidance/chase movement
class IcehowlSuppressMovementDuringChargeMultiplier : public Multiplier
{
public:
    IcehowlSuppressMovementDuringChargeMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "icehowl suppress movement during charge multiplier") {}
    float GetValue(Action* action) override;
};

// Keep beast tanks anchored instead of drifting with the combat formation
class NorthrendBeastsControlTankMovementMultiplier : public Multiplier
{
public:
    NorthrendBeastsControlTankMovementMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "northrend beasts control tank movement multiplier") {}
    float GetValue(Action* action) override;
};

#endif
