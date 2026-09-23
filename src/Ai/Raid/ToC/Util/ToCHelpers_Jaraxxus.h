#ifndef PLAYERBOTS_RAID_TOCHELPERS_JARAXXUS_H
#define PLAYERBOTS_RAID_TOCHELPERS_JARAXXUS_H

#include "PlayerbotAI.h"
#include "ToCData.h"

namespace TrialOfTheCrusaderHelpers
{

// True if Jaraxxus currently has any difficulty variant of the Nether Power buff
bool JaraxxusHasNetherPower(Unit* jaraxxus);

// Incinerate Flesh heal absorb on the unit, under the map difficulty's id
bool HasIncinerateFlesh(Unit* unit);

// The add that should be killed first: Mistress of Pain if up, otherwise Fel Infernal
Unit* GetPriorityJaraxxusAdd(PlayerbotAI* botAI);

// The second add to tank when both spawn types overlap (Fel Infernal while a Mistress of Pain
// is still up). Returns nullptr unless both adds are alive, so a lone add is handled by the
// priority assignment alone.
Unit* GetSecondaryJaraxxusAdd(PlayerbotAI* botAI);

}

#endif
