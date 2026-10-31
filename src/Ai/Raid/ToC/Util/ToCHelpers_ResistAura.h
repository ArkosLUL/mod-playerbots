#ifndef PLAYERBOTS_RAID_TOCHELPERS_RESISTAURA_H
#define PLAYERBOTS_RAID_TOCHELPERS_RESISTAURA_H

#include "Define.h"
#include "RaidResistAura.h"
#include "ToCData.h"

class PlayerbotAI;

namespace TrialOfTheCrusaderHelpers
{

// Schools come from the live encounter, else the staged arrival, else none, so paladins go back to
// their own aura between pulls.
uint8 ToCResistWantedSchools(PlayerbotAI* botAI, uint8 previous);
// Jaraxxus and the Twins: Aura Mastery is spent on Burning Inferno or a normal Light Vortex
bool ToCResistFireWantsMastery(PlayerbotAI* botAI);
// Heroic Anub'arak's phase 3: Aura Mastery is spent on a deadly Penetrating Cold round. Earlier the
// paladin with it keeps Concentration Aura.
bool ToCResistFrostWantsMastery(PlayerbotAI* botAI);

inline constexpr RaidResistAuraConfig TOC_RESIST_AURA{TRIAL_OF_THE_CRUSADER_MAP_ID, "toc.aura", &ToCResistWantedSchools,
                                                      &ToCResistFireWantsMastery, &ToCResistFrostWantsMastery};

// For the hold rules, no note
bool IsToCResistPaladinHolding(PlayerbotAI* botAI);
bool IsToCResistHunterHolding(PlayerbotAI* botAI);

}

#endif
