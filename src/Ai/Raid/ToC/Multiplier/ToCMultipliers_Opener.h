#ifndef PLAYERBOTS_RAID_TOCMULTIPLIERS_OPENER_H
#define PLAYERBOTS_RAID_TOCMULTIPLIERS_OPENER_H

#include "Multiplier.h"

// A DPS bot's damage on a boss still in its opener, and its area damage near one. The first pick of
// the boss goes through so melee run in; a repeat pick would restart the swing the opener stopped.
class ToCOpenerHoldMultiplier : public Multiplier
{
public:
    ToCOpenerHoldMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "toc opener hold") {}
    float GetValue(Action* action) override;
};

#endif
