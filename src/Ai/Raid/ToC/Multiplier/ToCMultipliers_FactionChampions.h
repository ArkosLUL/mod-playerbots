#ifndef PLAYERBOTS_RAID_TOCMULTIPLIERS_FACTIONCHAMPIONS_H
#define PLAYERBOTS_RAID_TOCMULTIPLIERS_FACTIONCHAMPIONS_H

#include "Multiplier.h"
#include "RaidAntiFear.h"
#include "RaidBurstWindow.h"
#include "RaidCombatHelpers.h"
#include "RaidSplash.h"

class FactionChampionsSuppressAoeMultiplier : public Multiplier
{
public:
    FactionChampionsSuppressAoeMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "faction champions suppress aoe multiplier") {}
    float GetValue(Action* action) override;

private:
    bool Splashes(Action* action);

    struct Names
    {
        bool ownSplash = false;
        bool callOfTheElements = false;
    };

    RaidSplashCache _splash;
    RaidActionCache<Names> _names;
};

class FactionChampionsThreatRedirectVetoMultiplier : public Multiplier
{
public:
    FactionChampionsThreatRedirectVetoMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "faction champions threat redirect veto multiplier") {}
    float GetValue(Action* action) override;

private:
    struct Facts
    {
        bool mainTankRedirect = false;
        bool tricks = false;
    };

    RaidActionCache<Facts> _facts;
};

class FactionChampionsAntiFearTotemGuardMultiplier : public RaidAntiFearTotemGuardMultiplier
{
public:
    FactionChampionsAntiFearTotemGuardMultiplier(PlayerbotAI* botAI)
        : RaidAntiFearTotemGuardMultiplier(botAI, "faction champions anti fear totem guard multiplier")
    {
    }

protected:
    bool FearWindowActive() override;
};

class FactionChampionsCcGuardMultiplier : public Multiplier
{
public:
    FactionChampionsCcGuardMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "faction champions cc guard multiplier") {}
    float GetValue(Action* action) override;

private:
    struct Facts
    {
        bool classCc = false;
        bool spreadDot = false;
    };

    RaidActionCache<Facts> _facts;
};

class FactionChampionsLateInterruptMultiplier : public Multiplier
{
public:
    FactionChampionsLateInterruptMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "faction champions late interrupt multiplier") {}
    float GetValue(Action* action) override;

private:
    struct Facts
    {
        CastSpellAction* cast = nullptr;
        bool healerKick = false;
        RaidSpellIdRef spellId;
    };

    RaidActionCache<Facts> _facts;
};

RaidBurstWindow ToCFactionChampionsBurstWindow(PlayerbotAI* botAI);

#endif
