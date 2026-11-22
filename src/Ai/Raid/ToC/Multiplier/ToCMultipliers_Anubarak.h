#ifndef PLAYERBOTS_RAID_TOCMULTIPLIERS_ANUBARAK_H
#define PLAYERBOTS_RAID_TOCMULTIPLIERS_ANUBARAK_H

#include <unordered_map>

#include "Multiplier.h"
#include "RaidBurstWindow.h"
#include "RaidCombatHelpers.h"
#include "RaidSplash.h"

class AnubarakControlTankMovementMultiplier : public Multiplier
{
public:
    AnubarakControlTankMovementMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "anubarak control tank movement multiplier") {}
    float GetValue(Action* action) override;
};

class AnubarakProtectSpikeKiteMultiplier : public Multiplier
{
public:
    AnubarakProtectSpikeKiteMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "anubarak protect spike kite multiplier") {}
    float GetValue(Action* action) override;

private:
    static bool HeldWhileMarked(Action* action);

    RaidActionCache<bool> _held;
};

class AnubarakThreatCapMultiplier : public Multiplier
{
public:
    AnubarakThreatCapMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "anubarak threat cap") {}
    float GetValue(Action* action) override;

private:
    RaidSplashCache _splash;
};

class AnubarakSphereGuardMultiplier : public Multiplier
{
public:
    AnubarakSphereGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "anubarak sphere guard") {}
    float GetValue(Action* action) override;

private:
    struct Names
    {
        bool starfall = false;
        bool crowdControl = false;
    };

    RaidSplashCache _splash;
    RaidActionCache<Names> _names;
};

class AnubarakBurrowerGuardMultiplier : public Multiplier
{
public:
    AnubarakBurrowerGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "anubarak burrower guard") {}
    float GetValue(Action* action) override;

private:
    struct Names
    {
        bool bladestorm = false;
        bool mover = false;
    };

    RaidSplashCache _splash;
    RaidActionCache<Names> _names;
};

class AnubarakBurrowerThreatCapMultiplier : public Multiplier
{
public:
    AnubarakBurrowerThreatCapMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "anubarak burrower threat cap") {}
    float GetValue(Action* action) override;

private:
    RaidSplashCache _splash;
};

class AnubarakSideTankAreaGuardMultiplier : public Multiplier
{
public:
    AnubarakSideTankAreaGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "anubarak side tank area guard") {}
    float GetValue(Action* action) override;

private:
    RaidSplashCache _splash;
};

class AnubarakSwarmHealMultiplier : public Multiplier
{
public:
    AnubarakSwarmHealMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "anubarak swarm heal") {}
    float GetValue(Action* action) override;

private:
    std::unordered_map<Action*, bool> _heals;
};

RaidBurstWindow ToCAnubarakBurstWindow(PlayerbotAI* botAI);

#endif
