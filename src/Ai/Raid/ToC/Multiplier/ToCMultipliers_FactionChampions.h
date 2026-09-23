#ifndef PLAYERBOTS_RAID_TOCMULTIPLIERS_FACTIONCHAMPIONS_H
#define PLAYERBOTS_RAID_TOCMULTIPLIERS_FACTIONCHAMPIONS_H

#include <vector>

#include "Multiplier.h"
#include "ToCMultipliers_Shared.h"

// Suppress AoE damage during Faction Champions so they never gain the stacking Anti-AoE mitigation
class FactionChampionsSuppressAoeMultiplier : public Multiplier
{
public:
    FactionChampionsSuppressAoeMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "faction champions suppress aoe multiplier") {}
    float GetValue(Action* action) override;
};

void AddToCFactionChampionsMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers);

ToCBurstWindow ToCFactionChampionsBurstWindow(PlayerbotAI* botAI);

#endif
