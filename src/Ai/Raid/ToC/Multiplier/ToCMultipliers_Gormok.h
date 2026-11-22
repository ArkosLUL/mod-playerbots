#ifndef PLAYERBOTS_RAID_TOCMULTIPLIERS_GORMOK_H
#define PLAYERBOTS_RAID_TOCMULTIPLIERS_GORMOK_H

#include "Multiplier.h"
#include "RaidCombatHelpers.h"

class GormokSnoboldCarrierMultiplier : public Multiplier
{
public:
    GormokSnoboldCarrierMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "gormok snobold carrier") {}
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

#endif
