#ifndef PLAYERBOTS_RAID_TOCHELPERS_GORMOK_H
#define PLAYERBOTS_RAID_TOCHELPERS_GORMOK_H

#include "PlayerbotAI.h"
#include "ToCData.h"

namespace TrialOfTheCrusaderHelpers
{

// Impale stacks a bleed on the current tank; once the tank has this many, the off-tank taunts so the
// bleed decays before it becomes lethal (starting value; tune against actual bleed damage).
constexpr uint32 GORMOK_IMPALE_SWAP_STACKS = 3;

// Total Impale stack count on a unit (0 if none)
uint32 GetGormokImpaleStacks(Unit* unit);

}

#endif
