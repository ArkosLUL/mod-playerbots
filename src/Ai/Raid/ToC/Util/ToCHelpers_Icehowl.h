#ifndef PLAYERBOTS_RAID_TOCHELPERS_ICEHOWL_H
#define PLAYERBOTS_RAID_TOCHELPERS_ICEHOWL_H

#include "PlayerbotAI.h"
#include "ToCData.h"

namespace TrialOfTheCrusaderHelpers
{

// True if the bot sits inside Icehowl's forward charge corridor (the ~12y wide lane he tramples)
bool IsBotInChargeCorridor(Player* bot, Unit* icehowl, float halfWidth);

// True if the bot has any difficulty variant of the Massive Crash aura (charge is imminent)
bool HasMassiveCrashAura(Player* bot);

}

#endif
