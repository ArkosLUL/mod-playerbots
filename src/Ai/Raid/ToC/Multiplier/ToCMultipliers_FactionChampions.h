#ifndef PLAYERBOTS_RAID_TOCMULTIPLIERS_FACTIONCHAMPIONS_H
#define PLAYERBOTS_RAID_TOCMULTIPLIERS_FACTIONCHAMPIONS_H

#include <vector>

#include "Multiplier.h"
#include "RaidAntiFear.h"
#include "ToCMultipliers_Shared.h"

// Single-target only: Champion's Aegis 68595 is a flat -75% to AoE damage on every champion
class FactionChampionsSuppressAoeMultiplier : public Multiplier
{
public:
    FactionChampionsSuppressAoeMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "faction champions suppress aoe multiplier") {}
    float GetValue(Action* action) override;
};

// Non-healers take their target from the focus node alone while a kill target is up
class FactionChampionsTargetGuardMultiplier : public Multiplier
{
public:
    FactionChampionsTargetGuardMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "faction champions target guard multiplier") {}
    float GetValue(Action* action) override;
};

// The script re-seeds threat every ~9 s, so a redirect onto the main tank buys nothing
class FactionChampionsThreatRedirectVetoMultiplier : public Multiplier
{
public:
    FactionChampionsThreatRedirectVetoMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "faction champions threat redirect veto multiplier") {}
    float GetValue(Action* action) override;
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

void AddToCFactionChampionsMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers);

ToCBurstWindow ToCFactionChampionsBurstWindow(PlayerbotAI* botAI);

#endif
