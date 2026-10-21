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

// Class CC nodes would pick their own targets, so CC goes out only through the CC duty. A DoT spread
// goes on no champion but the kill target.
class FactionChampionsCcGuardMultiplier : public Multiplier
{
public:
    FactionChampionsCcGuardMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "faction champions cc guard multiplier") {}
    float GetValue(Action* action) override;
};

// As in PvP: a champion's cast-time spell gets kicked in its last LATE_INTERRUPT_MS so the cast is wasted,
// a channel at once. Interrupts, silences and every stun on an enemy healer wait for it, and the mage due
// to Counterspell starts no cast meanwhile.
class FactionChampionsLateInterruptMultiplier : public Multiplier
{
public:
    FactionChampionsLateInterruptMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "faction champions late interrupt multiplier") {}
    float GetValue(Action* action) override;
};

void AddToCFactionChampionsMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers);

ToCBurstWindow ToCFactionChampionsBurstWindow(PlayerbotAI* botAI);

#endif
