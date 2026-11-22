#ifndef PLAYERBOTS_RAID_TOCHELPERS_RESISTAURA_H
#define PLAYERBOTS_RAID_TOCHELPERS_RESISTAURA_H

#include "Define.h"
#include "RaidResistAura.h"
#include "ToCData.h"

class PlayerbotAI;

namespace TrialOfTheCrusaderHelpers
{

uint8 ToCResistWantedSchools(PlayerbotAI* botAI, uint8 previous);
bool ToCResistFireWantsMastery(PlayerbotAI* botAI);
bool ToCResistFrostWantsMastery(PlayerbotAI* botAI);

inline constexpr RaidResistAuraConfig TOC_RESIST_AURA{TRIAL_OF_THE_CRUSADER_MAP_ID, "toc.aura", &ToCResistWantedSchools,
                                                      &ToCResistFireWantsMastery, &ToCResistFrostWantsMastery};

bool IsToCResistPaladinHolding(PlayerbotAI* botAI);
bool IsToCResistHunterHolding(PlayerbotAI* botAI);

}

#endif
