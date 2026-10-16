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

// A DPS bot's damage on him, and its area damage within 30 yd of him, while its threat cap latch holds.
// Heals and anything aimed elsewhere go through. Wired from the definition.
class AnubarakThreatCapMultiplier : public Multiplier
{
public:
    AnubarakThreatCapMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "anubarak threat cap") {}
    float GetValue(Action* action) override;
};

// Nothing aimed at a Frost Sphere but the bot's own duty sphere, and no crowd control even on that.
// Area spells have no target and stay unguarded. Wired from the definition.
class AnubarakSphereGuardMultiplier : public Multiplier
{
public:
    AnubarakSphereGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "anubarak sphere guard") {}
    float GetValue(Action* action) override;
};

void AddToCAnubarakMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers);

ToCBurstWindow ToCAnubarakBurstWindow(PlayerbotAI* botAI);

#endif
