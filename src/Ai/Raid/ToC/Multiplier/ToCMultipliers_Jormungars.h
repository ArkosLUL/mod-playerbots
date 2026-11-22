#ifndef PLAYERBOTS_RAID_TOCMULTIPLIERS_JORMUNGARS_H
#define PLAYERBOTS_RAID_TOCMULTIPLIERS_JORMUNGARS_H

#include "Multiplier.h"
#include "RaidCombatHelpers.h"
#include "RaidSplash.h"

class NorthrendWormsMoveGuardMultiplier : public Multiplier
{
public:
    NorthrendWormsMoveGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "northrend worms move guard") {}
    float GetValue(Action* action) override;

private:
    struct Facts
    {
        bool spellMover = false;
        bool mover = false;
        bool staysFree = false;
    };

    RaidActionCache<Facts> _facts;
};

class NorthrendWormsBileReachGuardMultiplier : public Multiplier
{
public:
    NorthrendWormsBileReachGuardMultiplier(PlayerbotAI* botAI)
        : Multiplier(botAI, "northrend worms bile reach guard") {}
    float GetValue(Action* action) override;

private:
    RaidActionCache<bool> _reach;
};

class NorthrendWormsRedirectGuardMultiplier : public Multiplier
{
public:
    NorthrendWormsRedirectGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "northrend worms redirect guard") {}
    float GetValue(Action* action) override;

private:
    RaidActionCache<bool> _classRedirect;
};

class NorthrendWormsThreatBrakeMultiplier : public Multiplier
{
public:
    NorthrendWormsThreatBrakeMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "northrend worms threat brake") {}
    float GetValue(Action* action) override;

private:
    RaidSplashCache _splash;
};

#endif
