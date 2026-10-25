#ifndef PLAYERBOTS_RAID_TOCHELPERS_FACTIONCHAMPIONSDEFENCE_H
#define PLAYERBOTS_RAID_TOCHELPERS_FACTIONCHAMPIONSDEFENCE_H

#include <array>
#include <vector>

#include "EncounterHelpers.h"
#include "PlayerbotAI.h"
#include "Position.h"
#include "ToCData.h"

namespace TrialOfTheCrusaderHelpers
{

struct ToCPredictedHit;

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

// Warlock Death Coil, highest rank first: the peel and the escape both cast it
constexpr std::array<uint32, 6> DEATH_COIL_RANKS = {47860, 47859, 27223, 17926, 17925, 6789};

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
// melee champions on their holders first.
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

// Inside an active Bladestorm or Hellfire, plus a yard. Not a Bladestorm whose warrior the bot kites from
// while it can move: the kite runs from him instead.
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

// The melee champion this bot snares and the spell id, when its victim kites from it
// (FactionChampionsKitesFrom) or it is loose off the tank, and nothing slows it yet. Healer victims first,
// then the lowest. Not by the grip or a wall grip DK. One snarer per champion.
Unit* FactionChampionsSnareTarget(PlayerbotAI* botAI, uint32& spellId);

// The melee champion this bot peels and the spell id: the danger champion wherever it is while it isn't
// disarmed, one within 10 yd of its victim when that's neither a tank nor its holder, or the Warrior
// champion with 2 or more of ours (pets too) in Bladestorm reach; danger first, then Warrior, then healer
// victims, then the lowest victim. Not one CC'd, rooted or immune to the spell. The nearest free bot with a
// ready peel takes it, not the grip DK or one on a CC duty: Disarm or Dismantle (not on one already
// disarmed), Hammer of Justice on the danger champion short of its third stun step, else Repentance, Blind,
// Freezing Arrow, Death Coil, Frost Nova, Entangling Roots or Psychic Horror; a DK's grips go to the wall
// instead. A CC a hit breaks, roots aside, only on a champion nobody of ours hits but its holder, which stops
// for it, and with no group DoT. None its DR would make immune. Nothing that pops the trinket on a held
// champion with it up. No root within 12 yd of a pack tank. Written to fc.peel.
Unit* FactionChampionsPeelTarget(PlayerbotAI* botAI, uint32& spellId);

// A peel duty's spell that a hit breaks is aimed at the champion
bool FactionChampionsBreakablePeelAt(PlayerbotAI* botAI, Unit* champion);

// The group member this bot covers with an external defensive and the spell id, when none of the four
// covers it yet: Hand of Protection on a non-tank non-holder with a physical champion or four of any on it,
// the others with four or more champions on a non-tank, or two at 60% health or under, casters counted.
// Champions about to pick it at their next re-pick (fc.nextfocus) count as on it. One caster per member.
// A non-tank with four on it that got none gets Divine Sacrifice from a paladin with Divine Guardian, while
// none runs. Lay on Hands goes this way too: a member at 20% or under with a champion on it, one per member
// in 15 s, from a paladin bot other than the member within 40 yd, healers first, held until the cast goes
// out or 2 s.
Unit* FactionChampionsProtectTarget(PlayerbotAI* botAI, uint32& spellId);

// Live, and 2 or more champions stand within 8 yd of this dead bot: a self-res there comes back low inside
// the pack and is their next pick
bool FactionChampionsCorpseCrowded(PlayerbotAI* botAI);

// The corpse this druid bot battle-resurrects and the Rebirth rank: fewer than 2 champions within 8 yd
// of it, healers then tanks, one druid per corpse. Never a dps.
Unit* FactionChampionsBattleResTarget(PlayerbotAI* botAI, uint32& spellId);

// The bot is focused: 2 or more champions have it as victim and its health loss over the last 2 s, run
// on for 4 s, would leave it under TOC_DEFENSIVE_FLOOR_PCT; or 4 or more on a non-tank, a hit just past
// that floor. Champions about to pick it at their next re-pick count as on it. Fills a measured hit for
// GetToCPersonalDefensive, physical when a physical champion is among them, focus at four or more.
bool FactionChampionsPersonalHit(PlayerbotAI* botAI, ToCPredictedHit& hit);

// Where a bot dodging a Bladestorm or Hellfire goes: the nearest clear ring, the spot in it nearest the
// healers' centre, within 30 yd of another living healer and 8+ yd off every melee champion when one is.
Position FactionChampionsAoeDodgeSpot(PlayerbotAI* botAI);

// The member 4+ champions are about to pick at their next re-pick (fc.nextfocus). Empty for none
ObjectGuid FactionChampionsNextFocus(PlayerbotAI* botAI);

// A holder's spell on its own champion, none of which pops its trinket: a warrior Disarms it, and Hamstrings
// it while it's on someone else; a DK slows it with Chains of Ice while it's on someone else.
Unit* FactionChampionsHoldControl(PlayerbotAI* botAI, uint32& spellId);

// The melee champion this bot DK grips to the wall, its stand, and the spell to cast now (0 while it walks
// or waits). A holder DK takes its own champion while it's on someone else; any other DK, not a pack tank,
// one within 50 yd on a raider that's neither a tank nor its holder, healer victims first, then the lowest.
// Not the grip duty's DK, nor the chain's two DKs while a pull is open. Never the kill target, nor one rooted,
// CC'd, guarded, held by our breakable CC or gripped by anyone in the last 8 s. The stand is 27 or 22 yd off
// the champion, 34-44 yd off ARENA_CENTER, within 38 yd of a healer, clear of AoE and a walk of 6 s or less,
// landing the champion 12+ yd farther from its victim; the one farthest from it. Taken only once Death Grip
// is ready by the arrival plus 1 s. Death Grip within 1.5 yd of the stand, then Chains of Ice once it lands;
// gripped says the grip is out. The stand is the DK's own spot once it's there. Dropped before the grip once the
// cooldown outlasts the walk, the stand is past 29 yd of the champion, 8 s pass, or someone else grips or
// holds it or it leaves its victim; after it once the champion is slowed or 4 s pass. Written to fc.wall.
Unit* FactionChampionsWallGrip(PlayerbotAI* botAI, Position& stand, uint32& spellId, bool* gripped = nullptr);

// This DK's wall grip went out on champion
void FactionChampionsNoteWallGrip(PlayerbotAI* botAI, Unit* champion);

// The group member this paladin bot puts Hand of Freedom on and the spell id: a member a champion rooted or
// snared (Dazed aside) with a melee champion it kites from within 15 yd, healers first, then the lowest. The
// nearest free paladin bot with it ready, within 30 yd and in sight, not on a protect duty. One per member.
Unit* FactionChampionsFreedomTarget(PlayerbotAI* botAI, uint32& spellId);

}

#endif
