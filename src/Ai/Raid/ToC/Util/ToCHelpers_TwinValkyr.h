#ifndef PLAYERBOTS_RAID_TOCHELPERS_TWINVALKYR_H
#define PLAYERBOTS_RAID_TOCHELPERS_TWINVALKYR_H

#include "PlayerbotAI.h"
#include "ToCData.h"

namespace TrialOfTheCrusaderHelpers
{

// True while either twin (Fjola or Eydis) is alive. Gates the shared strategy's Twin Val'kyr
// multipliers so they stay inert during the other ToC encounters.
bool TwinValkyrEncounterActive(PlayerbotAI* botAI);

// Essence aura checks on a unit (typically the bot itself).
bool HasLightEssence(Unit* unit);
bool HasDarkEssence(Unit* unit);
bool HasAnyEssence(Unit* unit);

// Heroic Touch of Light / Touch of Darkness on the unit, under the map difficulty's id
bool HasLightTouch(Unit* unit);
bool HasDarkTouch(Unit* unit);

// True while the matching-coloured Vortex is being cast. Detected via the casting twin's current
// spell (Light Vortex from Fjola, Dark Vortex from Eydis), mirroring the Jaraxxus fel-fireball idiom.
bool TwinValkyrLightVortexActive(PlayerbotAI* botAI);
bool TwinValkyrDarkVortexActive(PlayerbotAI* botAI);

// The twin (Fjola or Eydis) currently channeling its Twin's Pact heal-to-full, else nullptr. The
// channel is interruptible; retargeting an interrupter onto it breaks the heal.
Unit* GetTwinCastingPact(PlayerbotAI* botAI);

}

#endif
