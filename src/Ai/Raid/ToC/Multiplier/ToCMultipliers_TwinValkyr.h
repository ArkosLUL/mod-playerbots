#ifndef PLAYERBOTS_RAID_TOCMULTIPLIERS_TWINVALKYR_H
#define PLAYERBOTS_RAID_TOCMULTIPLIERS_TWINVALKYR_H

#include <vector>

#include "Multiplier.h"
#include "ToCMultipliers_Shared.h"

// Keeps a twin's tank on her instead of drifting with the combat formation
class TwinValkyrControlTankMovementMultiplier : public Multiplier
{
public:
    TwinValkyrControlTankMovementMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "twin valkyr control tank movement multiplier") {}
    float GetValue(Action* action) override;
};

// No taunting the twin another living tank holds
class TwinValkyrTauntGuardMultiplier : public Multiplier
{
public:
    TwinValkyrTauntGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "twin valkyr taunt guard multiplier") {}
    float GetValue(Action* action) override;
};

// Holds kicks while a shield makes the twin immune to them, and silences and stuns, which never land
// on the twins
class TwinValkyrInterruptHoldMultiplier : public Multiplier
{
public:
    TwinValkyrInterruptHoldMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "twin valkyr interrupt hold multiplier") {}
    float GetValue(Action* action) override;
};

// The class redirects pick one tank for the whole raid. Here each twin has her own, and the
// encounter's redirect node casts them.
class TwinValkyrRedirectGuardMultiplier : public Multiplier
{
public:
    TwinValkyrRedirectGuardMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "twin valkyr redirect guard multiplier") {}
    float GetValue(Action* action) override;
};

void AddToCTwinValkyrMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers);

ToCBurstWindow ToCTwinValkyrBurstWindow(PlayerbotAI* botAI);

#endif
