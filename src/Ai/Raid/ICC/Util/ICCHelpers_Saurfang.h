/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCHELPERS_SAURFANG_H
#define PLAYERBOTS_ICCHELPERS_SAURFANG_H

#include <vector>

#include "Define.h"
#include "ICCData.h"
#include "Position.h"
#include "RaidBurstWindow.h"
#include "RaidDefensive.h"

class Creature;
class Player;
class PlayerbotAI;
class Unit;

// icecrown_citadel.h CreaturesIds. GetEntry reports these on every difficulty.
constexpr uint32 NPC_DBS_SAURFANG = 37813;
constexpr uint32 NPC_DBS_BLOOD_BEAST = 38508;

// No difficulty row: one id on every difficulty.
constexpr uint32 SPELL_DBS_RUNE_OF_BLOOD = 72410;
constexpr uint32 SPELL_DBS_MARK = 72293;
constexpr uint32 SPELL_DBS_FRENZY = 72737;
constexpr uint32 SPELL_DBS_BEACON_OF_LIGHT = 53563;
constexpr uint32 SPELL_DBS_FORBEARANCE = 25771;
// These two have difficulty rows (72444-6 and 72441-3): remap at the call
constexpr uint32 SPELL_DBS_MARK_SPLASH = 72255;
constexpr uint32 SPELL_DBS_BOILING_BLOOD = 72385;

// The tanks hold him here. Every stand is navprobe-clean (--nav 0x09): the tank spots and the east slots
// settle at 541.11, the stack at 540.64 and the west slots at 539.28-539.29, with normal paths from the
// stack to each. All of them see him and every tank spot, checked with him up to 2.5 yd off his spot.
inline Position const ICC_DBS_BOSS_SPOT(-496.35f, 2211.33f, 541.11f);
// All inside his 4.33 yd melee reach, east of him, so he faces east and the raid spreads west
inline Position const ICC_DBS_TANK_SPOTS[] = {
    {-494.27f, 2211.55f, 541.11f},
    {-494.27f, 2213.05f, 541.11f},
    {-494.27f, 2210.05f, 541.11f},
};
// Behind him with the melee, for ranged and healers past the slots
inline Position const ICC_DBS_STACK_SPOT(-499.85f, 2211.33f, 540.64f);

// Blood Nova hits everyone within 12 yd of its victim, centre to centre from a creature, so slots sit
// that far apart plus the park tolerance of both. All lie within 31 yd of his spot, inside the 32 yd at
// which reach spell walks a caster in. 1-9 are west of him on the floor; 10 and 11 are east of the tanks
// on the causeway (12 yd wide), because the platform's edge hides the tank spots from anything
// 33 yd north or south of him.
constexpr float DBS_SLOT_SPACING = 13.5f;
inline Position const ICC_DBS_SLOTS[] = {
    {-515.85f, 2211.33f, 539.29f}, {-511.07f, 2224.12f, 539.29f}, {-511.07f, 2198.54f, 539.29f},
    {-499.07f, 2230.64f, 539.29f}, {-499.07f, 2192.02f, 539.29f}, {-525.19f, 2221.26f, 539.29f},
    {-525.19f, 2201.40f, 539.29f}, {-511.60f, 2237.74f, 539.29f}, {-511.60f, 2184.92f, 539.29f},
    {-478.85f, 2211.33f, 541.11f}, {-465.35f, 2211.33f, 541.11f},
};
constexpr uint8 DBS_SLOT_COUNT = sizeof(ICC_DBS_SLOTS) / sizeof(ICC_DBS_SLOTS[0]);

// Stored in dbs.slot: 1-11 a slot, the stack, then a tank spot each
constexpr uint8 DBS_STAND_STACK = DBS_SLOT_COUNT + 1;
constexpr uint8 DBS_STAND_TANK = DBS_STAND_STACK + 1;
constexpr uint8 DBS_TANK_SPOTS = sizeof(ICC_DBS_TANK_SPOTS) / sizeof(ICC_DBS_TANK_SPOTS[0]);

// A stand parks within DBS_STAND_PARK and walks back once pushed past DBS_STAND_UNPARK
constexpr float DBS_STAND_PARK = 0.75f;
constexpr float DBS_STAND_UNPARK = 2.5f;
static_assert(DBS_STAND_PARK < DBS_STAND_UNPARK, "a walk has to end inside its own trigger");
static_assert(DBS_SLOT_SPACING >= 12.0f + 2 * DBS_STAND_PARK, "two parked neighbours must not share a Blood Nova");

// Blood Power: his energy, 0-100, +1% damage a point, reset by a Mark at 100. The holder spends a tank
// button from PEAK once per cycle, from PEAK_MARKED when it carries a Mark (the splash hits it every
// swing too). A drop of RESET_DROP or more between two reads is a Mark.
constexpr uint32 DBS_BLOOD_POWER_PEAK = 70;
constexpr uint32 DBS_BLOOD_POWER_PEAK_MARKED = 40;
constexpr uint32 DBS_BLOOD_POWER_RESET_DROP = 30;
// Any tank under DEFENSIVE_PCT spends a button, holding him or not. Under LAST_RESORT_PCT its class
// buttons are free again.
constexpr float DBS_TANK_DEFENSIVE_PCT = 40.0f;
constexpr float DBS_TANK_LAST_RESORT_PCT = 25.0f;
// Hand of Protection lasts 10 s, Boiling Blood 15: cast with at least this much of the latter left
constexpr int32 DBS_PROTECT_MIN_LEFT_MS = 7000;
constexpr float DBS_PROTECT_RANGE = 30.0f;

// He is alive and in combat while his boss state is IN_PROGRESS
bool IsDbsLive(PlayerbotAI* botAI);
Creature* GetDbsBoss(PlayerbotAI* botAI);
// Army of the Dead is held while he is live or the bot is in his room
bool IsDbsArmyHeld(PlayerbotAI* botAI);

// Tanks and DPS. The holder keeps him, the rest take a player's skull, then a tank's beast, a ranged
// kill duty, a beast in a melee's reach, else him. The rune tank not holding him gets nothing but a beast.
// Notes dbs.target.
Unit* GetDbsTarget(PlayerbotAI* botAI);

// This bot is the tank to take him: free of Rune of Blood, taunt ready in its current form, within 30 yd,
// lowest guid, while the rune sits on a tank holding him or a non-tank holds him. Notes dbs.swap.
bool IsDbsSwapTaunter(PlayerbotAI* botAI);
// A tank carrying Rune of Blood while another tank holds him. Notes dbs.yield.
bool IsDbsRuneYielding(PlayerbotAI* botAI);
// The tank not holding him, its taunt ready: the beast on a non-tank nearest its victim, while its own
// rune leaves the taunt time to come back for the next swap or Scent of Blood is up
Unit* GetDbsBeastToTaunt(PlayerbotAI* botAI);

// The beast this bot holds the control duty on and the spell it casts, null without a duty
Unit* GetDbsControlDuty(PlayerbotAI* botAI, char const*& spell);

// The mark this healer's duty wants worked on now, and whether that is its Beacon of Light
Player* GetDbsMarkWork(PlayerbotAI* botAI, bool& beacon);
// A paladin holding a Mark duty, so its class beacon nodes would move the beacon off the mark
bool IsDbsBeaconDutyHeld(PlayerbotAI* botAI);

// The player this paladin casts Hand of Protection on: a bot with a Mark and Boiling Blood at once that
// loses nothing to a pacify. Notes dbs.protect.
Player* GetDbsProtectTarget(PlayerbotAI* botAI);

// The button this tank spends now, 0 for none: the holder from Blood Power PEAK once per cycle and all
// through Frenzy, any tank under DBS_TANK_DEFENSIVE_PCT. Physical only, all his damage is.
uint32 GetDbsTankDefensive(PlayerbotAI* botAI, char const* noteKind);
// Class defensive nodes stay off until GetDbsTankDefensive or DBS_TANK_LAST_RESORT_PCT
bool IsDbsTankDefensiveHeld(PlayerbotAI* botAI);
void LatchDbsTankDefensive(PlayerbotAI* botAI);

// The splash a marked bot takes from Blood Power PEAK on and in Frenzy, as a hit for
// GetRaidPersonalDefensive. False for a tank: the holder has the ladder above.
bool GetDbsMarkHit(PlayerbotAI* botAI, RaidPredictedHit& hit);

bool IsDbsHoldingStand(PlayerbotAI* botAI);
bool IsDbsHoldingSpreadStand(PlayerbotAI* botAI);
bool IsDbsHoldingTankStand(PlayerbotAI* botAI);
// The held stand wants a walk
bool GetDbsStandWalk(PlayerbotAI* botAI, Position& spot);

// dbs.phase, the beast scan, the Blood Power cycle, the slot roster, the kill, control, Mark and Hand of
// Protection duties, the stand latch, the calls to a human tank, the reset after a wipe
void TickDbsEncounter(PlayerbotAI* botAI);

// Lust waits for Frenzy
RaidBurstWindow IccSaurfangBurstWindow(PlayerbotAI* botAI);
std::vector<uint32> const& GetDbsOpenerBosses(PlayerbotAI* botAI);

#endif
