#ifndef PLAYERBOTS_RAID_TOCMULTIPLIERS_JARAXXUS_H
#define PLAYERBOTS_RAID_TOCMULTIPLIERS_JARAXXUS_H

#include <vector>

#include "Multiplier.h"
#include "ToCMultipliers_Shared.h"

// Keep tanks anchored on Jaraxxus and his adds instead of drifting with the combat formation
class JaraxxusControlTankMovementMultiplier : public Multiplier
{
public:
    JaraxxusControlTankMovementMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "jaraxxus control tank movement multiplier") {}
    float GetValue(Action* action) override;
};

// No class taunt off another tank holding Jaraxxus
class JaraxxusTauntGuardMultiplier : public Multiplier
{
public:
    JaraxxusTauntGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "jaraxxus taunt guard") {}
    float GetValue(Action* action) override;
};

// Mistress' Kiss punishes a cast still running on its 0.5 s check, so a kissed bot casts instants only
class JaraxxusKissCastHoldMultiplier : public Multiplier
{
public:
    JaraxxusKissCastHoldMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "jaraxxus kiss cast hold") {}
    float GetValue(Action* action) override;
};

// The release attacks whoever stands nearest, so nothing may walk the main tank off him during the intro
class JaraxxusIntroHoldMultiplier : public Multiplier
{
public:
    JaraxxusIntroHoldMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "jaraxxus intro hold") {}
    float GetValue(Action* action) override;
};

// AvoidAoeAction flees a flame by FleePosition out to 5.5 yd (GetDistance takes off both combat
// reaches), past where our own dodge starts, so the flames stay ours alone
class JaraxxusAvoidAoeGuardMultiplier : public Multiplier
{
public:
    JaraxxusAvoidAoeGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "jaraxxus avoid aoe guard") {}
    float GetValue(Action* action) override;
};

void AddToCJaraxxusMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers);

ToCBurstWindow ToCJaraxxusBurstWindow(PlayerbotAI* botAI);

#endif
