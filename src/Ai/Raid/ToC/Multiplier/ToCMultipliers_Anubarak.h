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

// The spike's target moves only through the kite, and never feigns death. Attack actions stay so it
// keeps a target.
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

// Nothing aimed at a Frost Sphere but the bot's own duty sphere, and on that only direct damage: no
// crowd control, DoT, channel or area spell. Heroic: no Starfall at all. Other area spells have no
// target and stay unguarded. Wired from the definition.
class AnubarakSphereGuardMultiplier : public Multiplier
{
public:
    AnubarakSphereGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "anubarak sphere guard") {}
    float GetValue(Action* action) override;
};

// A non-tank's damage on a burrower only on its own dps pick, so a multidot doesn't pull a loose one
// onto a raider. Interrupts and stuns on a casting one go through; a taunt or Death Grip never.
// Wired from the definition.
class AnubarakBurrowerGuardMultiplier : public Multiplier
{
public:
    AnubarakBurrowerGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "anubarak burrower guard") {}
    float GetValue(Action* action) override;
};

void AddToCAnubarakMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers);

ToCBurstWindow ToCAnubarakBurstWindow(PlayerbotAI* botAI);

#endif
