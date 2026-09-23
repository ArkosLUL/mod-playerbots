#ifndef PLAYERBOTS_RAID_TOCHELPERS_ANUBARAK_H
#define PLAYERBOTS_RAID_TOCHELPERS_ANUBARAK_H

#include "PlayerbotAI.h"
#include "ToCData.h"

namespace TrialOfTheCrusaderHelpers
{

// On a flying Frost Sphere only: a killed sphere loses it when it lands as a Permafrost patch
constexpr uint32 SPELL_FROST_SPHERE = 67539;

// True during the submerge phase (phase 2): a Pursuing Spike is alive, or the boss carries the
// submerge aura. Swarm Scarabs don't count, they outlive the phase.
bool AnubarakSubmerged(PlayerbotAI* botAI);

// True during the final phase (phase 3): the bot carries Leeching Swarm, or the boss is below 30%
// health. Gates the saved-up Bloodlust/Heroism burn.
bool AnubarakLeechingSwarmActive(PlayerbotAI* botAI);

// A Frost Sphere still in the air and attackable. A killed one turns unselectable while it falls.
bool IsFrostSphereFlying(Unit* sphere);

// A Frost Sphere that has landed as a Permafrost patch. The patch never carries the Permafrost
// aura itself, its own persistent area aura skips the caster.
bool IsPermafrostPatch(Unit* sphere);

// Nearest Permafrost patch within radius, nullptr when none has been seeded yet
Unit* GetNearestPermafrost(Player* bot, float radius);

}

#endif
