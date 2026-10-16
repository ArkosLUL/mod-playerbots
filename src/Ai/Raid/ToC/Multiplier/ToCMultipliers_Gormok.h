#ifndef PLAYERBOTS_RAID_TOCMULTIPLIERS_GORMOK_H
#define PLAYERBOTS_RAID_TOCMULTIPLIERS_GORMOK_H

#include <vector>

#include "Multiplier.h"

// Holds generic movers off a ranged dps or healer carrying a snobold, so it stays in Gormok's melee
// until the snobold dies
class GormokSnoboldCarrierMultiplier : public Multiplier
{
public:
    GormokSnoboldCarrierMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "gormok snobold carrier") {}
    float GetValue(Action* action) override;
};

void AddToCGormokMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers);

#endif
