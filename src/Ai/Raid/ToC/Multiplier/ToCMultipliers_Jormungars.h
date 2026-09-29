#ifndef PLAYERBOTS_RAID_TOCMULTIPLIERS_JORMUNGARS_H
#define PLAYERBOTS_RAID_TOCMULTIPLIERS_JORMUNGARS_H

#include <vector>

#include "Multiplier.h"

// Holds other movers while a reposition walk is in flight, since every MoveTo clears the MotionMaster
// and any of them would cancel it. Attacks, `avoid aoe` and the beasts' own movers stay.
class NorthrendWormsMoveGuardMultiplier : public Multiplier
{
public:
    NorthrendWormsMoveGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "northrend worms move guard") {}
    float GetValue(Action* action) override;
};

// A Burning Bile carrier the reposition walked off the raid stays off it: its pulse hits everyone
// within 10 yd, melee included, and `reach melee` or a charge would put it back in the stack.
class NorthrendWormsBileReachGuardMultiplier : public Multiplier
{
public:
    NorthrendWormsBileReachGuardMultiplier(PlayerbotAI* botAI)
        : Multiplier(botAI, "northrend worms bile reach guard") {}
    float GetValue(Action* action) override;
};

// The class redirects pick one tank for the whole raid. Here each worm has its own holder, and the
// encounter's redirect node casts them.
class NorthrendWormsRedirectGuardMultiplier : public Multiplier
{
public:
    NorthrendWormsRedirectGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "northrend worms redirect guard") {}
    float GetValue(Action* action) override;
};

void AddToCJormungarsMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers);

#endif
