#ifndef PLAYERBOTS_RAID_TOCHELPERS_OPENER_H
#define PLAYERBOTS_RAID_TOCHELPERS_OPENER_H

#include <vector>

#include "Define.h"
#include "RaidOpener.h"
#include "ToCData.h"

class PlayerbotAI;

namespace TrialOfTheCrusaderHelpers
{

// The live encounter's bosses. Every encounter but the champions has an opener.
std::vector<uint32> const& ToCOpenerBosses(PlayerbotAI* botAI);

inline constexpr RaidOpenerConfig TOC_OPENER{TRIAL_OF_THE_CRUSADER_MAP_ID, "toc.opener", &ToCOpenerBosses};

}

#endif
