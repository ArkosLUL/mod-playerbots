#ifndef PLAYERBOTS_RAID_TOCMULTIPLIERS_ICEHOWL_H
#define PLAYERBOTS_RAID_TOCMULTIPLIERS_ICEHOWL_H

#include "Multiplier.h"
#include "RaidCombatHelpers.h"

class IcehowlChargeGuardMultiplier : public Multiplier
{
public:
    IcehowlChargeGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icehowl charge guard") {}
    float GetValue(Action* action) override;

private:
    RaidActionCache<bool> _competes;
};

class IcehowlBreathSpreadGuardMultiplier : public Multiplier
{
public:
    IcehowlBreathSpreadGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "icehowl breath spread guard") {}
    float GetValue(Action* action) override;
};

#endif
