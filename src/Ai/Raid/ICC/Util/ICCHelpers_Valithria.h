/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCHELPERS_VALITHRIA_H
#define PLAYERBOTS_ICCHELPERS_VALITHRIA_H

#include "Define.h"
#include "Position.h"
#include "RaidEncounter.h"

class Creature;
class PlayerbotAI;
class Unit;

// icecrown_citadel.h DataTypes: the instance's guid slots for her and for the combat trigger, which shares
// the Dream State phase and so sees the clouds
constexpr uint32 VD_DATA_VALITHRIA_GUID = 10;
constexpr uint32 VD_DATA_COMBAT_TRIGGER_GUID = 35;

// Her spawn, where she stays all fight; she despawns 4 s after the win
constexpr float VD_HOME_X = 4203.65f;
constexpr float VD_HOME_Y = 2483.89f;

// No difficulty row: one id on every difficulty
constexpr uint32 SPELL_VD_DREAM_PORTAL_VISUAL_PRE = 71304;
constexpr uint32 SPELL_VD_NIGHTMARE_PORTAL_VISUAL_PRE = 71986;
constexpr uint32 SPELL_VD_GUARDIAN_SPIRIT = 47788;
constexpr uint32 SPELL_VD_BEACON_OF_LIGHT = 53563;
constexpr uint32 SPELL_VD_BLOODLUST = 2825;
constexpr uint32 SPELL_VD_HEROISM = 32182;
constexpr uint32 SPELL_VD_EXHAUSTION = 57723;
constexpr uint32 SPELL_VD_SATED = 57724;
// Difficulty rows: remap at the call
constexpr uint32 SPELL_VD_COLUMN_OF_FROST = 70702;
constexpr uint32 SPELL_VD_FROSTBOLT_VOLLEY = 70759;
constexpr uint32 SPELL_VD_GUT_SPRAY = 70633;

// The push: lust and Guardian Spirit once she is this healthy and the dreamers are back with their stacks
constexpr float VD_PUSH_PCT = 80.0f;
// An abomination's tank spends a button under this, under the higher one while Gut Spray adds 25% physical
// taken, and the class nodes keep theirs until the last resort
constexpr float VD_TANK_DEFENSIVE_PCT = 40.0f;
constexpr float VD_TANK_SPRAYED_PCT = 50.0f;
constexpr float VD_TANK_LAST_RESORT_PCT = 25.0f;

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

// The boss state gate, held open past DONE for a bot the dream left flagged in the air near her place:
// the win comes while Dream State still runs, and the bot has to land
bool VdGateOpen(PlayerbotAI* botAI, uint32 encounterId);
inline constexpr EncounterGate VdGate{&VdGateOpen, &BossStateGateLive};

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

// She is past VD_PUSH_PCT, no bot healer is in Dream State and a healer carries Emerald Vigor. Notes vd.push.
bool IsVdPushWindow(PlayerbotAI* botAI);
// The lust this shaman casts now, null for none: the lowest-guid shaman bot able to, in the push window,
// with no lust running
char const* GetVdLust(PlayerbotAI* botAI);
// Valithria while this priest puts Guardian Spirit on her: the push window, hers has run out, and no
// lower-guid priest bot can cast it
Creature* GetVdGuardianSpiritTarget(PlayerbotAI* botAI);
// This paladin keeps its Beacon of Light on her: a bot healer on the raid duty, or on any duty on heroic
bool IsVdBeaconOnHer(PlayerbotAI* botAI);
// Valithria while that beacon needs a cast. Notes vd.beacon.
Creature* GetVdBeaconTarget(PlayerbotAI* botAI);
// The archmage casting Frostbolt Volley this bot is elected to kick, and the kick. Notes vd.interrupt.
Unit* GetVdInterrupt(PlayerbotAI* botAI, char const*& spell);
// The kiter Frostbolt Volley slowed that this bot frees, and the spell. Notes vd.freedom.
Unit* GetVdFreedom(PlayerbotAI* botAI, char const*& spell);
// The button this tank spends now, 0 for none: its own abomination hits it under VD_TANK_DEFENSIVE_PCT
uint32 GetVdTankDefensive(PlayerbotAI* botAI, char const* noteKind);
bool IsVdTankDefensiveHeld(PlayerbotAI* botAI);

// A dream, a kite, a portal claim, a dodge or a void step owns the bot's feet
bool IsVdMovementOwned(PlayerbotAI* botAI);
// IsVdKiting without the note, for the rule that holds a kiter's feet
bool IsVdKiteHeld(PlayerbotAI* botAI);

// A zombie on somebody else stands near the bot or its target: area damage would put the zombie on the bot
bool IsVdSplashHeld(PlayerbotAI* botAI);

// The scans, the healer split, portal claims, abomination owners, snares, kicks, freedom, the dream group,
// the human tank call, the reset
void TickVdEncounter(PlayerbotAI* botAI);

// Fire (Lay Waste), frost (Frostbolt Volley, Column of Frost), nature (Acid Burst, Corrosion, Gut Spray)
uint8 IccValithriaResistSchools(PlayerbotAI* botAI, uint8 previous);

#endif
