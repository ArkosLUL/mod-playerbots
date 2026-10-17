#ifndef PLAYERBOTS_RAID_TOCHELPERS_FACTIONCHAMPIONSDEFENCE_H
#define PLAYERBOTS_RAID_TOCHELPERS_FACTIONCHAMPIONSDEFENCE_H

#include <vector>

#include "EncounterHelpers.h"
#include "PlayerbotAI.h"
#include "Position.h"
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

// The Enhancement Shaman's totems. The raid's own shamans summon the same entries.
constexpr uint32 NPC_GROUNDING_TOTEM = 5925;
constexpr uint32 NPC_WINDFURY_TOTEM = 6112;
constexpr uint32 NPC_TREMOR_TOTEM = 5913;

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

// The champion totem this bot kills, written to fc.totem. Each goes to the nearest non-healer bot within
// 30 yd holding none, and stays with it until the totem dies or the bot dies, loses control or is past 30 yd.
Unit* FactionChampionsTotemTarget(PlayerbotAI* botAI);

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

// The kill target and the stand to Death Grip it from, when this bot is the DK on grip duty: the kill
// target has 2+ other champions within 8 yd, and the stand pulls it clear of them inside heal range.
// Latched raid-wide, written to fc.grip.
Unit* FactionChampionsGripTarget(PlayerbotAI* botAI, Position& stand);

// The melee champion this bot snares and the spell id, when its victim is a healer or under half
// health and nothing slows it yet. One snarer per champion.
Unit* FactionChampionsSnareTarget(PlayerbotAI* botAI, uint32& spellId);

// The group member this bot covers with an external defensive and the spell id, when two or more
// physical champions are on that member and none of the four covers it yet. One caster per member.
Unit* FactionChampionsProtectTarget(PlayerbotAI* botAI, uint32& spellId);

// The corpse this druid bot battle-resurrects and the Rebirth rank: fewer than 2 champions within 8 yd
// of it, healers first, one druid per corpse.
Unit* FactionChampionsBattleResTarget(PlayerbotAI* botAI, uint32& spellId);

// Where a bot dodging a Bladestorm or Hellfire goes: the nearest clear ring, the spot in it nearest the
// healers' centre, within 30 yd of another living healer when one is that close.
Position FactionChampionsAoeDodgeSpot(PlayerbotAI* botAI);

}

#endif
