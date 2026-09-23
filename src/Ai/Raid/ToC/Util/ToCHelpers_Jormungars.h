#ifndef PLAYERBOTS_RAID_TOCHELPERS_JORMUNGARS_H
#define PLAYERBOTS_RAID_TOCHELPERS_JORMUNGARS_H

#include "PlayerbotAI.h"
#include "ToCData.h"

namespace TrialOfTheCrusaderHelpers
{

// Worm display ids per mobility form (boss_northrend_beasts.cpp Model)
enum class ToCDisplayIds : uint32
{
    MODEL_ACIDMAW_STATIONARY    = 29815,
    MODEL_ACIDMAW_MOBILE        = 29816,
    MODEL_DREADSCALE_STATIONARY = 26935,
    MODEL_DREADSCALE_MOBILE     = 24564,
};

// True while the worm is in its mobile (chasing) form rather than stationary
bool IsWormMobile(Unit* worm);

// The worm (Acidmaw or Dreadscale) currently casting its Sweep frontal cone, else nullptr
Unit* GetWormCastingSweep(PlayerbotAI* botAI);

}

#endif
