#ifndef PLAYERBOTS_RAID_TOCMULTIPLIERS_NORTHRENDBEASTS_H
#define PLAYERBOTS_RAID_TOCMULTIPLIERS_NORTHRENDBEASTS_H

#include "Multiplier.h"
#include "RaidBurstWindow.h"

class NorthrendBeastsControlTankMovementMultiplier : public Multiplier
{
public:
    NorthrendBeastsControlTankMovementMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "northrend beasts control tank movement multiplier") {}
    float GetValue(Action* action) override;
};

class NorthrendBeastsTauntGuardMultiplier : public Multiplier
{
public:
    NorthrendBeastsTauntGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "northrend beasts taunt guard") {}
    float GetValue(Action* action) override;
};

RaidBurstWindow ToCNorthrendBeastsBurstWindow(PlayerbotAI* botAI);

#endif
