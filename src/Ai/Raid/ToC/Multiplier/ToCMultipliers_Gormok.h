#ifndef PLAYERBOTS_RAID_TOCMULTIPLIERS_GORMOK_H
#define PLAYERBOTS_RAID_TOCMULTIPLIERS_GORMOK_H

#include <vector>

#include "Multiplier.h"

// Keeps dps assist from flipping a bot off its snobold back onto the skull
class GormokSnoboldTargetGuardMultiplier : public Multiplier
{
public:
    GormokSnoboldTargetGuardMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "gormok snobold target guard") {}
    float GetValue(Action* action) override;
};

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
