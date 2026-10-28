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

// The tanks hold him here. Every stand is navprobe-clean (--nav 0x09): the tank spots settle at 541.11,
// the stack at 540.64 and the slots at 539.28-539.29, with normal paths from the stack to each.
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
// that far apart plus the park tolerance of both. 1-9 lie within 31 yd of his spot, inside the 32 yd at
// which reach spell walks a caster in. 10 and 11 at 33 yd are for healers only.
constexpr float DBS_SLOT_SPACING = 13.5f;
inline Position const ICC_DBS_SLOTS[] = {
    {-515.85f, 2211.33f, 539.29f}, {-511.07f, 2224.12f, 539.29f}, {-511.07f, 2198.54f, 539.29f},
    {-499.07f, 2230.64f, 539.29f}, {-499.07f, 2192.02f, 539.29f}, {-525.19f, 2221.26f, 539.29f},
    {-525.19f, 2201.40f, 539.29f}, {-511.60f, 2237.74f, 539.29f}, {-511.60f, 2184.92f, 539.29f},
    {-499.23f, 2244.20f, 539.29f}, {-499.23f, 2178.46f, 539.29f},
};
constexpr uint8 DBS_SLOT_COUNT = sizeof(ICC_DBS_SLOTS) / sizeof(ICC_DBS_SLOTS[0]);
constexpr uint8 DBS_INNER_SLOTS = 9;

// Stored in dbs.slot: 1-11 a slot, the stack, then a tank spot each
constexpr uint8 DBS_STAND_STACK = DBS_SLOT_COUNT + 1;
constexpr uint8 DBS_STAND_TANK = DBS_STAND_STACK + 1;
constexpr uint8 DBS_TANK_SPOTS = sizeof(ICC_DBS_TANK_SPOTS) / sizeof(ICC_DBS_TANK_SPOTS[0]);

// A stand parks within DBS_STAND_PARK and walks back once pushed past DBS_STAND_UNPARK
constexpr float DBS_STAND_PARK = 1.0f;
constexpr float DBS_STAND_UNPARK = 2.5f;
static_assert(DBS_STAND_PARK < DBS_STAND_UNPARK, "a walk has to end inside its own trigger");

// He is alive and in combat while his boss state is IN_PROGRESS
bool IsDbsLive(PlayerbotAI* botAI);
Creature* GetDbsBoss(PlayerbotAI* botAI);
// Army of the Dead is held while he is live or the bot is in his room
bool IsDbsArmyHeld(PlayerbotAI* botAI);

// Tanks and DPS. The holder keeps him, the rest take a player's skull, then a tank's beast, a ranged
// kill duty, a beast in a melee's reach, else him. The rune tank not holding him gets nothing but a beast.
// Notes dbs.target.
Unit* GetDbsTarget(PlayerbotAI* botAI);

// This bot is the tank to take him: free of Rune of Blood, taunt ready, within 30 yd, lowest guid, while
// the rune sits on a tank holding him or a non-tank holds him. Notes dbs.swap.
bool IsDbsSwapTaunter(PlayerbotAI* botAI);
// A tank carrying Rune of Blood while another tank holds him. Notes dbs.yield.
bool IsDbsRuneYielding(PlayerbotAI* botAI);
// The tank not holding him, its taunt ready: the beast on a non-tank nearest its victim, while its own
// rune leaves the taunt time to come back for the next swap or the beast carries Scent of Blood
Unit* GetDbsBeastToTaunt(PlayerbotAI* botAI);

// The beast this bot holds the control duty on and the spell it casts, null without a duty
Unit* GetDbsControlDuty(PlayerbotAI* botAI, char const*& spell);

// The mark this healer's duty wants worked on now, and whether that is its Beacon of Light
Player* GetDbsMarkWork(PlayerbotAI* botAI, bool& beacon);
// A paladin holding a Mark duty, so its class beacon nodes would move the beacon off the mark
bool IsDbsBeaconDutyHeld(PlayerbotAI* botAI);

bool IsDbsHoldingStand(PlayerbotAI* botAI);
bool IsDbsHoldingSpreadStand(PlayerbotAI* botAI);
bool IsDbsHoldingTankStand(PlayerbotAI* botAI);
// The held stand wants a walk
bool GetDbsStandWalk(PlayerbotAI* botAI, Position& spot);

// dbs.phase, the beast scan, the slot roster, the kill, control and Mark duties, the stand latch, the
// reset after a wipe
void TickDbsEncounter(PlayerbotAI* botAI);

// Lust waits for Frenzy
RaidBurstWindow IccSaurfangBurstWindow(PlayerbotAI* botAI);
std::vector<uint32> const& GetDbsOpenerBosses(PlayerbotAI* botAI);

#endif
