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

// The champion and the spell to purge it with: the kill target holding a buff worth taking for the raid's
// purger, a pack champion holding Bloodlust, Heroism, Avenging Wrath or Hand of Freedom for the next one,
// melee champions on the frontline first.
Unit* FactionChampionsPurgeTarget(PlayerbotAI* botAI, char const*& spell);

// Where a physical attacker goes while the kill target is immune to physical damage only (Hand of
// Protection). Latched raid-wide, written to fc.physical.
Unit* FactionChampionsPhysicalSwitchTarget(PlayerbotAI* botAI);

// The champion totem this bot kills, written to fc.totem. Each goes to the nearest non-healer bot within
// 30 yd holding none, and stays with it until the totem dies or the bot dies, loses control or is past 30 yd.
Unit* FactionChampionsTotemTarget(PlayerbotAI* botAI);

// The warlock whose Hellfire this bot interrupts at once, and the spell. One bot per channel: the nearest
// with an interrupt ready that reaches him, ranged first (Counterspell, Wind Shear, Silence, Strangulate,
// Silencing Shot, a felhunter's Spell Lock), else a melee kick. Written to fc.kick.
Unit* FactionChampionsChannelInterruptTarget(PlayerbotAI* botAI, char const*& spell);

// The three below skip a Hellfire for its first 2 s for its interrupter when that's a melee kick, so it
// stays in place to land it.

// Inside an active Bladestorm or Hellfire, plus a yard
bool FactionChampionsInAoe(PlayerbotAI* botAI);
// The active Bladestorms and Hellfires at the distance a dodge keeps from them
std::vector<EncounterHelpers::HazardCircle> FactionChampionsAoeClearances(PlayerbotAI* botAI);
// A melee bot whose current target is an AoE source or stands in one, so reaching it walks into it
bool FactionChampionsReachIntoAoe(PlayerbotAI* botAI);

// The target carries Unstable Affliction, so a dispel that removes Magic may hit the dispeller
bool FactionChampionsDispelBackfires(PlayerbotAI* botAI, Unit* target);

// The champion and the stand to Death Grip it from, when this bot is the non-tank DK on grip duty. On a
// player's pull: a walk to the grip post (walkOnly) until the DK tank's grip has landed the champion,
// then the grip. Without one, only a pulled kill target still under 45 yd off the pack centre or in 40 yd
// of another champion healer. Never on a champion in the air. The stand is the DK's own spot when the
// champion is in reach there and 12+ yd nearer the pack, else 27 yd off the champion on the floor
// bearing farthest from the pack. Latched raid-wide, written to fc.grip.
Unit* FactionChampionsGripTarget(PlayerbotAI* botAI, Position& stand, bool* walkOnly = nullptr);

// The grip chain, which reads the Faction Champions stage, not the fight, so it works before the pull.
// A tank's generic pull at a champion (the player targets it and types pull) becomes the chain's
// request, written to fc.chain, and the generic pull is ended. The DK tank grips first, the bot DK
// once the champion lands, the player last. The request drops after 15 s with no tank grip, or 10 s
// after it with no bot DK grip.
void FactionChampionsTakePullRequest(PlayerbotAI* botAI);

// This bot is the chain's DK tank: a bot tank DK knowing Death Grip, lowest guid
bool FactionChampionsIsChainOpener(PlayerbotAI* botAI);

// The DK tank owes the open request its grip
bool FactionChampionsOpenerGripDue(PlayerbotAI* botAI);

// The champion this DK tank grips and the stand: its own spot when in reach and the grip lands the
// champion 12+ yd farther from the pack (any grip before the pull), else 27 yd off the champion toward
// the grip post. Death Grip more than 3 s off its cooldown: tells the master and drops the request.
Unit* FactionChampionsChainGripTarget(PlayerbotAI* botAI, Position& stand);

// A Death Grip went out on target: the DK tank's marks the first link, the bot DK's ends the request
void FactionChampionsNoteChainGrip(PlayerbotAI* botAI, Unit* target);

// The open request's champion, empty without one
ObjectGuid FactionChampionsChainTarget(Player* bot);

// A DK is on its way to grip the kill target out of the pack, which it hasn't left yet
bool FactionChampionsGripPending(PlayerbotAI* botAI);

// Roots, stuns, fears, confuses, charges or knocks back, itself or one trigger or linked spell down.
// A rooted champion can't take Death Grip's pull, and a root or stun landing mid-flight ends the jump.
bool FactionChampionsSpellHoldsTarget(uint32 spellId);

// Nothing may hold it: the player's pull until 6 s after the bot DK's grip, a grip duty's target, and any
// champion in the air
bool FactionChampionsGripGuarded(PlayerbotAI* botAI, Unit* champion);

// Some champion is guarded
bool FactionChampionsGripGuardActive(PlayerbotAI* botAI);

// The guard's chain half, no fight refresh: the player's pull, its 6 s tail, or in the air. Safe inside
// the champion refresh.
bool FactionChampionsChainGuarded(Player* bot, Unit* champion);

// A pet autocast to flip, and which way: taunts off on heroic for the fight, holds off while a guard is
// active and back on after it. 0 when nothing needs flipping.
uint32 FactionChampionsPetAutocastToggle(PlayerbotAI* botAI, bool& enable);

// The melee champion this bot snares and the spell id, when its victim is a healer or under half
// health, or it is loose off the tank, and nothing slows it yet. One snarer per champion.
Unit* FactionChampionsSnareTarget(PlayerbotAI* botAI, uint32& spellId);

// The melee champion this bot peels and the spell id: one off the tanks and frontline within 10 yd of
// its victim, or
// the Warrior champion with 2 or more of ours (pets too) in Bladestorm reach, Warrior first, then healer
// victims, then the lowest victim. Not one CC'd, rooted or immune to the spell. The nearest free bot
// with a ready peel takes it, not the grip DK or one on a CC duty: Disarm or Dismantle (not on one
// already disarmed), Frost Nova, Entangling Roots, Psychic Horror, or Death Grip from 12+ yd nearer the
// pack by a pack tank or a DK other than the one kept for the grip chain, never on the kill target. No
// root within 12 yd of a pack tank. Written to fc.peel.
Unit* FactionChampionsPeelTarget(PlayerbotAI* botAI, uint32& spellId);

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
