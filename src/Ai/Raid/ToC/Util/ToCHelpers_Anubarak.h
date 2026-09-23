#ifndef PLAYERBOTS_RAID_TOCHELPERS_ANUBARAK_H
#define PLAYERBOTS_RAID_TOCHELPERS_ANUBARAK_H

#include "PlayerbotAI.h"
#include "ToCData.h"

namespace TrialOfTheCrusaderHelpers
{

// True during the submerge phase (phase 2): a Pursuing Spike is alive, or the boss carries the
// submerge aura. Swarm Scarabs don't count, they outlive the phase.
bool AnubarakSubmerged(PlayerbotAI* botAI);

// True during the final phase (phase 3): the boss carries the Leeching Swarm aura, or has dropped
// below 30% health. Gates the saved-up Bloodlust/Heroism burn.
bool AnubarakLeechingSwarmActive(PlayerbotAI* botAI);

// Nearest grounded Permafrost patch (a Frost Sphere that has been destroyed and now carries the
// Permafrost aura) within radius. Returns nullptr when no patch has been seeded yet.
Unit* GetNearestPermafrost(Player* bot, float radius);

}

#endif
