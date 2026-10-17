#ifndef PLAYERBOTS_RAID_TOCHELPERS_RESISTAURA_H
#define PLAYERBOTS_RAID_TOCHELPERS_RESISTAURA_H

#include "Define.h"

class PlayerbotAI;

// Who raises which resistance aura: one bot paladin per wanted school (fire, frost, shadow) and the
// generic nature hunter for Aspect of the Wild. Schools come from the live encounter, else the staged
// arrival, else none, so paladins go back to their own aura between pulls. Nothing resolves off map 649.
namespace TrialOfTheCrusaderHelpers
{

enum class ToCResistSchool : uint8
{
    None,
    Fire,
    Frost,
    Shadow,
    Nature
};

// The aura this bot should hold right now. Notes toc.aura for paladin and hunter bots.
ToCResistSchool GetToCResistDuty(PlayerbotAI* botAI);

// The class action name, null for None.
char const* ToCResistAuraName(ToCResistSchool school);

// Has a duty but not the aura. A paladin goes by its own application: a weaker duplicate keeps its
// application with the effects stripped, and HasAura by name walks effects.
bool ToCResistAuraMissing(PlayerbotAI* botAI);

// For the hold rules, no note
bool IsToCResistPaladinHolding(PlayerbotAI* botAI);
bool IsToCResistHunterHolding(PlayerbotAI* botAI);

}

#endif
