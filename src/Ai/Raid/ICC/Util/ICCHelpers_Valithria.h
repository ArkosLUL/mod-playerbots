/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCHELPERS_VALITHRIA_H
#define PLAYERBOTS_ICCHELPERS_VALITHRIA_H

#include "Define.h"
#include "Position.h"

class Creature;
class PlayerbotAI;
class Unit;

// icecrown_citadel.h DataTypes: the instance's guid slots for her and for the combat trigger, which shares
// the Dream State phase and so sees the clouds
constexpr uint32 VD_DATA_VALITHRIA_GUID = 10;
constexpr uint32 VD_DATA_COMBAT_TRIGGER_GUID = 35;

// No difficulty row: one id on every difficulty
constexpr uint32 SPELL_VD_DREAM_PORTAL_VISUAL_PRE = 71304;
constexpr uint32 SPELL_VD_NIGHTMARE_PORTAL_VISUAL_PRE = 71986;
// Difficulty row: remap at the call
constexpr uint32 SPELL_VD_COLUMN_OF_FROST = 70702;

// navprobe --nav 0x09 settles every point within 30 yd of her at 364.86-364.88, 11 rings of up to 48
// headings, each in sight of her; every derived stand stays inside this disc
constexpr float VD_FLOOR_RADIUS = 30.0f;
// The zombie kite runs this ring round her: a 5 yd/s pursuer settles about 17 yd behind a 7 yd/s kiter
// on it, outside Acid Burst's 15 yd
constexpr float VD_KITE_RADIUS = 25.0f;
constexpr uint8 VD_KITE_POINTS = 24;

// A portal stand parks within VD_STAND_PARK and walks back once pushed past VD_STAND_UNPARK
constexpr float VD_STAND_PARK = 1.0f;
constexpr float VD_STAND_UNPARK = 2.5f;
static_assert(VD_STAND_PARK < VD_STAND_UNPARK, "a walk has to end inside its own trigger");

// Heroic Twisted Nightmares ticks 200 a stack a second: a healer this deep skips the portals
constexpr uint8 VD_NIGHTMARE_STACK_CAP = 25;

enum class VdFlight : uint8
{
    None,
    Refresh,  // the cloud nearest the bot, before its old stacks run out
    Gather,   // onto the group's leader
    Cloud,    // the group's shared cloud
    Human,    // onto a player in Dream State
    Hover,    // nothing to fly to
    Descend,  // low before Dream State ends
    Land      // Dream State is over and the bot is still in the air
};

// Her boss state is IN_PROGRESS and the bot is in her room
bool IsVdLive(PlayerbotAI* botAI);
Creature* GetVdValithria(PlayerbotAI* botAI);
bool IsVdDreaming(PlayerbotAI* botAI);

// Tanks and DPS: a player's skull, a tank's own abomination, then skeletons and archmages, worms and
// suppressers, zombies (ranged only) and held abominations, nearest her within a tier. Notes vd.target.
Unit* GetVdTarget(PlayerbotAI* botAI);
// A tank's own abomination on someone else, else a worm or, with no abomination of its own, a zombie on a
// healer, its taunt ready
Unit* GetVdTankTaunt(PlayerbotAI* botAI);
// The zombie this bot snares now and the spell, one bot per chasing zombie. Notes vd.snare.
Unit* GetVdZombieToSnare(PlayerbotAI* botAI, char const*& spell);

// A fresh Column of Frost or a dying zombie covers the bot. Notes vd.dodge.
bool IsVdInDanger(PlayerbotAI* botAI);
bool FindVdDodgeSpot(PlayerbotAI* botAI, Position& spot);
// A mana user inside a Mana Void, and where it steps to
bool GetVdVoidStep(PlayerbotAI* botAI, Position& spot);

// A zombie chases this bot, and it isn't a tank holding an abomination. Notes vd.kite.
bool IsVdKiting(PlayerbotAI* botAI);
bool GetVdKiteLeg(PlayerbotAI* botAI, Position& leg);

// The claimed portal: its spot, and the real portal once it opens. True while the healer walks or can
// click.
bool GetVdPortalWalk(PlayerbotAI* botAI, Position& spot, Creature*& portal);
bool IsVdHoldingPortal(PlayerbotAI* botAI);
// Valithria while this healer heals her: portal healers unless someone near is low, raid healers while
// everyone near is high
Creature* GetVdHealTarget(PlayerbotAI* botAI);

// Where a bot in or just out of Dream State flies. Notes vd.dream.
VdFlight GetVdFlight(PlayerbotAI* botAI, Position& dest);

// A dream, a kite, a portal claim, a dodge or a void step owns the bot's feet
bool IsVdMovementOwned(PlayerbotAI* botAI);
// IsVdKiting without the note, for the rule that holds a kiter's feet
bool IsVdKiteHeld(PlayerbotAI* botAI);

// A zombie on somebody else stands near the bot or its target: area damage would put the zombie on the bot
bool IsVdSplashHeld(PlayerbotAI* botAI);

// The scans, the healer split, portal claims, abomination owners, snares, the dream group, the reset
void TickVdEncounter(PlayerbotAI* botAI);

// Fire (Lay Waste), frost (Frostbolt Volley, Column of Frost), nature (Acid Burst, Corrosion, Gut Spray)
uint8 IccValithriaResistSchools(PlayerbotAI* botAI, uint8 previous);

#endif
