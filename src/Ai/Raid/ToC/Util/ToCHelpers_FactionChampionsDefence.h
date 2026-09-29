#ifndef PLAYERBOTS_RAID_TOCHELPERS_FACTIONCHAMPIONSDEFENCE_H
#define PLAYERBOTS_RAID_TOCHELPERS_FACTIONCHAMPIONSDEFENCE_H

#include <vector>

#include "EncounterHelpers.h"
#include "PlayerbotAI.h"
#include "ToCData.h"

namespace TrialOfTheCrusaderHelpers
{

// Faction Champion spells (boss_faction_champions.cpp). These sit in a spell difficulty row, so read
// them through sSpellMgr->GetSpellIdForDifficulty.
constexpr uint32 SPELL_UNSTABLE_AFFLICTION = 65812;  // dispelling it silences and hits the dispeller
constexpr uint32 SPELL_HELLFIRE = 65816;             // channel, the aura sits on the warlock
constexpr uint32 SPELL_HAND_OF_FREEDOM = 68757;      // the script casts the 10H id of row 66115
constexpr uint32 SPELL_POWER_WORD_SHIELD = 66099;
constexpr uint32 SPELL_RENEW = 66177;
constexpr uint32 SPELL_RIPTIDE = 66053;
constexpr uint32 SPELL_REJUVENATION = 66065;
constexpr uint32 SPELL_LIFEBLOOM = 66093;
constexpr uint32 SPELL_REGROWTH = 66067;

// One id on every difficulty
constexpr uint32 SPELL_POLYMORPH = 65801;
constexpr uint32 SPELL_FEAR = 65809;
constexpr uint32 SPELL_PSYCHIC_SCREAM = 65543;
constexpr uint32 SPELL_REPENTANCE = 66008;
constexpr uint32 SPELL_HAMMER_OF_JUSTICE_HOLY = 66613;
constexpr uint32 SPELL_HAMMER_OF_JUSTICE_RET = 66007;
constexpr uint32 SPELL_SILENCE = 65542;
constexpr uint32 SPELL_STRANGULATE = 66018;
constexpr uint32 SPELL_HEX = 66054;
constexpr uint32 SPELL_WYVERN_STING = 65877;
constexpr uint32 SPELL_BLADESTORM = 65947;
constexpr uint32 SPELL_HAND_OF_PROTECTION = 66009;
constexpr uint32 SPELL_DIVINE_SHIELD = 66010;
constexpr uint32 SPELL_EARTH_SHIELD = 66063;
constexpr uint32 SPELL_HEROISM = 65983;
constexpr uint32 SPELL_BLOODLUST = 65980;
constexpr uint32 SPELL_AVENGING_WRATH = 66011;
constexpr uint32 SPELL_BARKSKIN = 65860;

// The player spell
constexpr uint32 SPELL_MASS_DISPEL = 32375;

// Every reader below answers null or false unless Faction Champions is live, and for any bot its
// rule doesn't name. All of them share one refresh per instance per millisecond.

// The group member this bot dispels a counted CC from, and the spell to use. Healers are freed
// first and non-healer dispellers used first; each dispeller takes one member.
Unit* FactionChampionsDispelCcTarget(PlayerbotAI* botAI, char const*& spell);

// The unit to aim Mass Dispel at when this bot is the priest on duty: the kill target under Hand of
// Protection or Divine Shield, else the suspended champion under Divine Shield. Nobody while a raider
// within 15 yd of it carries Unstable Affliction.
Unit* FactionChampionsMassDispelTarget(PlayerbotAI* botAI);

// The kill target and the spell to purge it with, when this bot is the raid's one purger and the kill
// target holds a buff worth taking.
Unit* FactionChampionsPurgeTarget(PlayerbotAI* botAI, char const*& spell);

// Where a physical attacker goes while the kill target is immune to physical damage only (Hand of
// Protection). Latched raid-wide, written to fc.physical.
Unit* FactionChampionsPhysicalSwitchTarget(PlayerbotAI* botAI);

// The three below skip a Hellfire for its first 2 s for one bot, left in place to kick it: the first
// by guid on the warlock, in melee range with a melee interrupt ready.

// Inside an active Bladestorm or Hellfire, plus a yard
bool FactionChampionsInAoe(PlayerbotAI* botAI);
// The active Bladestorms and Hellfires at the distance a dodge keeps from them
std::vector<EncounterHelpers::HazardCircle> FactionChampionsAoeClearances(PlayerbotAI* botAI);
// A melee bot whose current target is an AoE source or stands in one, so reaching it walks into it
bool FactionChampionsReachIntoAoe(PlayerbotAI* botAI);

// The target carries Unstable Affliction, so a dispel that removes Magic may hit the dispeller
bool FactionChampionsDispelBackfires(PlayerbotAI* botAI, Unit* target);

}

#endif
