#ifndef PLAYERBOTS_RAID_TOCMULTIPLIERS_ANUBARAK_H
#define PLAYERBOTS_RAID_TOCMULTIPLIERS_ANUBARAK_H

#include <vector>

#include "Multiplier.h"
#include "ToCMultipliers_Shared.h"

// Keep tanks anchored on Anub'arak and his burrowers instead of drifting with the combat formation
class AnubarakControlTankMovementMultiplier : public Multiplier
{
public:
    AnubarakControlTankMovementMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "anubarak control tank movement multiplier") {}
    float GetValue(Action* action) override;
};

// The spike's target moves only through the kite. Attack actions stay so it keeps a target.
class AnubarakProtectSpikeKiteMultiplier : public Multiplier
{
public:
    AnubarakProtectSpikeKiteMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "anubarak protect spike kite multiplier") {}
    float GetValue(Action* action) override;
};

// The boss hold, burrower holds and sphere duty pick these bots' targets, generic assist would drag
// them onto whatever the raid is hitting
class AnubarakTankTargetGuardMultiplier : public Multiplier
{
public:
    AnubarakTankTargetGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "anubarak tank target guard") {}
    float GetValue(Action* action) override;
};

void AddToCAnubarakMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers);

ToCBurstWindow ToCAnubarakBurstWindow(PlayerbotAI* botAI);

#endif
