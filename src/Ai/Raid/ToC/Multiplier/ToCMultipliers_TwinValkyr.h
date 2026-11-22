#ifndef PLAYERBOTS_RAID_TOCMULTIPLIERS_TWINVALKYR_H
#define PLAYERBOTS_RAID_TOCMULTIPLIERS_TWINVALKYR_H

#include "Multiplier.h"
#include "RaidBurstWindow.h"
#include "RaidCombatHelpers.h"

class TwinValkyrControlTankMovementMultiplier : public Multiplier
{
public:
    TwinValkyrControlTankMovementMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "twin valkyr control tank movement multiplier") {}
    float GetValue(Action* action) override;
};

class TwinValkyrTauntGuardMultiplier : public Multiplier
{
public:
    TwinValkyrTauntGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "twin valkyr taunt guard multiplier") {}
    float GetValue(Action* action) override;
};

enum class TwinInterruptKind : uint8
{
    None,
    Kick,
    Refused
};

class TwinValkyrInterruptHoldMultiplier : public Multiplier
{
public:
    TwinValkyrInterruptHoldMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "twin valkyr interrupt hold multiplier") {}
    float GetValue(Action* action) override;

private:
    RaidActionCache<TwinInterruptKind> _kinds;
};

class TwinValkyrRedirectGuardMultiplier : public Multiplier
{
public:
    TwinValkyrRedirectGuardMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "twin valkyr redirect guard multiplier") {}
    float GetValue(Action* action) override;

private:
    RaidActionCache<bool> _redirects;
};

RaidBurstWindow ToCTwinValkyrBurstWindow(PlayerbotAI* botAI);

#endif
