#ifndef PLAYERBOTS_RAID_TOCMULTIPLIERS_JARAXXUS_H
#define PLAYERBOTS_RAID_TOCMULTIPLIERS_JARAXXUS_H

#include "Multiplier.h"
#include "RaidBurstWindow.h"
#include "RaidSplash.h"

class JaraxxusControlTankMovementMultiplier : public Multiplier
{
public:
    JaraxxusControlTankMovementMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "jaraxxus control tank movement multiplier") {}
    float GetValue(Action* action) override;
};

class JaraxxusTauntGuardMultiplier : public Multiplier
{
public:
    JaraxxusTauntGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "jaraxxus taunt guard") {}
    float GetValue(Action* action) override;
};

class JaraxxusKissCastHoldMultiplier : public Multiplier
{
public:
    JaraxxusKissCastHoldMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "jaraxxus kiss cast hold") {}
    float GetValue(Action* action) override;
};

class JaraxxusAvoidAoeGuardMultiplier : public Multiplier
{
public:
    JaraxxusAvoidAoeGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "jaraxxus avoid aoe guard") {}
    float GetValue(Action* action) override;
};

class JaraxxusThreatCapMultiplier : public Multiplier
{
public:
    JaraxxusThreatCapMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "jaraxxus threat cap") {}
    float GetValue(Action* action) override;

private:
    RaidSplashCache _splash;
};

RaidBurstWindow ToCJaraxxusBurstWindow(PlayerbotAI* botAI);

#endif
