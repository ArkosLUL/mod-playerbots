/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCHELPERS_FESTERGUT_H
#define PLAYERBOTS_ICCHELPERS_FESTERGUT_H

#include <vector>

#include "Define.h"
#include "ICCData.h"
#include "Position.h"
#include "RaidDefensive.h"

class Creature;
class Player;
class PlayerbotAI;
class Unit;

// icecrown_citadel.h CreaturesIds. GetEntry reports it on every difficulty.
constexpr uint32 NPC_FG_FESTERGUT = 36626;

// boss_festergut.cpp. All four have difficulty rows (2198, 2115, 1988, 2131): remap at the call.
// Inhaled Blight's heroic columns are empty and fall back to the normal id of the same size.
constexpr uint32 SPELL_FG_GASTRIC_BLOAT = 72219;
constexpr uint32 SPELL_FG_INHALED_BLIGHT = 69166;
constexpr uint32 SPELL_FG_PUNGENT_BLIGHT = 69195;
constexpr uint32 SPELL_FG_INOCULATED = 69291;

// His spawn. Every stand below is navprobe-clean (--nav 0x09): the floor is flat at 360.385 out to 45 yd
// round it, every stand sees him and both tank spots from a 3x3 grid at 1 yd, and the walks between
// them are straight normal paths. All inside his boss boundary (x 4205-4325, y 3082-3195).
inline Position const ICC_FG_BOSS_SPOT(4267.92f, 3137.26f, 360.385f);
// 6.5 yd from his spot at 40 degrees either side of west, inside his 10.83 yd melee range (reach 8 +
// 1.5 + 4/3), so he turns to the holder without moving. 8.4 yd apart: goo on the free tank misses the
// holder.
inline Position const ICC_FG_TANK_SPOTS[] = {
    {4263.74f, 3142.24f, 360.385f},
    {4272.10f, 3142.24f, 360.385f},
};
constexpr uint8 FG_TANK_SPOTS = sizeof(ICC_FG_TANK_SPOTS) / sizeof(ICC_FG_TANK_SPOTS[0]);
// Between the tank spots: the tanks and melee stack here for a spore
inline Position const ICC_FG_CLOSE_ANCHOR(4267.92f, 3142.24f, 360.385f);
// His legs, 7 yd out south-east and north-east: behind him whichever tank he faces, 9.9 yd apart
inline Position const ICC_FG_LEGS[] = {
    {4262.97f, 3132.31f, 360.385f},
    {4272.87f, 3132.31f, 360.385f},
};

// Vile Gas lands on a ranged player and everyone within 8 yd of each victim, so slots sit 10.4 yd apart
// at least. Rows at 18 and 28.5 yd behind and beside him: past the 14.5 yd its picker counts as melee,
// inside reach spell's 38.5 yd. Healers take the middle ones, which reach both tank spots and nearly every
// slot in 40 yd.
inline Position const ICC_FG_SLOTS[] = {
    {4267.92f, 3119.26f, 360.385f}, {4257.99f, 3122.25f, 360.385f}, {4277.85f, 3122.25f, 360.385f},
    {4251.35f, 3130.23f, 360.385f}, {4284.49f, 3130.23f, 360.385f}, {4250.22f, 3140.54f, 360.385f},
    {4285.62f, 3140.54f, 360.385f}, {4267.92f, 3108.76f, 360.385f}, {4257.71f, 3110.65f, 360.385f},
    {4278.13f, 3110.65f, 360.385f}, {4248.85f, 3116.08f, 360.385f}, {4286.99f, 3116.08f, 360.385f},
    {4242.53f, 3124.32f, 360.385f}, {4293.31f, 3124.32f, 360.385f}, {4239.58f, 3134.28f, 360.385f},
    {4296.26f, 3134.28f, 360.385f}, {4240.39f, 3144.64f, 360.385f}, {4295.45f, 3144.64f, 360.385f},
};
constexpr uint8 FG_SLOT_COUNT = sizeof(ICC_FG_SLOTS) / sizeof(ICC_FG_SLOTS[0]);
// The ranged spore stack, on slots 1 and 5, 19.9 yd apart
inline Position const ICC_FG_FAR_ANCHORS[] = {
    {4267.92f, 3119.26f, 360.385f},
    {4284.49f, 3130.23f, 360.385f},
};

// Stored in fg.slot: 1-18 a slot, then the legs, then a tank spot each
constexpr uint8 FG_STAND_LEG = FG_SLOT_COUNT + 1;
constexpr uint8 FG_STAND_TANK = FG_STAND_LEG + 1;

constexpr float FG_VILE_GAS_RADIUS = 8.0f;
constexpr float FG_STAND_PARK = 0.75f;
constexpr float FG_STAND_UNPARK = 2.5f;
constexpr float FG_SLOT_SPACING = 10.37f;
static_assert(FG_STAND_PARK < FG_STAND_UNPARK, "a walk has to end inside its own trigger");
static_assert(FG_SLOT_SPACING >= FG_VILE_GAS_RADIUS + 2 * FG_STAND_PARK,
              "two parked neighbours must not share Vile Gas");

// Blighted Spores reaches 8 yd from its carrier. A carrier stands on its group's anchor, the rest on a ring
// this far round it.
constexpr float FG_SPORE_BURST = 8.0f;
constexpr float FG_SPORE_RING = 2.5f;
static_assert(FG_SPORE_RING + 2 * FG_STAND_PARK < FG_SPORE_BURST, "the ring sits inside the burst");

// Legs hold while he stands this close to his spot: 7 + 3.5 stays inside his melee range
constexpr float FG_BOSS_DRIFT = 3.5f;

// Malleable Goo (72297 row 2197) hits 5 yd round where its target stood when Putricide threw it. A bot
// inside RUN dodges, to a spot CLEARANCE out. The farthest stand is 97 yd from his balcony point and the
// vial flies 12 yd/s, so an impact counts for HAZARD_MS.
constexpr float FG_GOO_RADIUS = 5.0f;
constexpr float FG_GOO_RUN = FG_GOO_RADIUS + 1.5f;
constexpr float FG_GOO_CLEARANCE = FG_GOO_RUN + 1.0f;
constexpr uint32 FG_GOO_HAZARD_MS = 8500;

// Gastric Bloat explodes at 10. It lasts 100 s and lands every 15-17.5 s, so a tank relieved at 9 is clean
// again before its partner reaches 9. The taunter carries at most TAUNTER_MAX.
constexpr uint32 FG_SWAP_STACKS = 9;
constexpr uint32 FG_TAUNTER_MAX_STACKS = FG_SWAP_STACKS - 2;

// A tank not holding him stops attacking at CAP of the holder's threat, starts again under RELEASE
constexpr float FG_BRAKE_CAP = 0.9f;
constexpr float FG_BRAKE_RELEASE = 0.8f;

// Inhaled Blight adds 25-30% damage and attack speed a stack. The holder spends one button at MINOR, and
// keeps one up from MAJOR to Pungent Blight. Any tank under DEFENSIVE_PCT spends one, and under
// LAST_RESORT_PCT its class buttons are free again.
constexpr uint32 FG_BLIGHT_MINOR = 2;
constexpr uint32 FG_BLIGHT_MAJOR = 3;
constexpr float FG_TANK_DEFENSIVE_PCT = 40.0f;
constexpr float FG_TANK_LAST_RESORT_PCT = 25.0f;

// Festergut is in combat and the bot fights in his room. His boss state only moves once Putricide reaches
// his balcony, about 7 s into the pull, so nothing here reads it.
Creature* GetFgBoss(PlayerbotAI* botAI);
bool IsFgFight(PlayerbotAI* botAI);

// Any group member carries Gas Spore. Notes fg.phase.
bool IccFestergutSporeCycle(Player* bot);

// A player's skull, else him for tanks and dps. A braked tank gets nothing. Notes fg.target.
Unit* GetFgTarget(PlayerbotAI* botAI);

// This bot is the tank to take him: a tank holds him at FG_SWAP_STACKS or a non-tank holds him, and this
// one, on a tank spot unless no such tank can, carries the fewest stacks, at most FG_TAUNTER_MAX_STACKS,
// its taunt ready. Notes fg.swap.
bool IsFgSwapTaunter(PlayerbotAI* botAI);
// A tank not holding him past the threat brake
bool IsFgBraked(PlayerbotAI* botAI);

// The button this tank spends now, 0 for none. Physical only.
uint32 GetFgTankDefensive(PlayerbotAI* botAI, char const* noteKind);
bool IsFgTankDefensiveHeld(PlayerbotAI* botAI);
void LatchFgTankDefensive(PlayerbotAI* botAI);

// Pungent Blight while he casts it. The cover's hit takes the lowest Inoculated stack on the map, the
// personal one the bot's own: the defensive code doesn't count Inoculated (AoE damage avoidance).
bool GetFgPungentHit(PlayerbotAI* botAI, RaidPredictedHit& hit, bool own);
RaidCover GetFgPungentCover(PlayerbotAI* botAI);
bool IsFgPungentCoverHeld(PlayerbotAI* botAI);

// Live goo impacts, positions only
bool GetFgGoo(PlayerbotAI* botAI, std::vector<Position>& impacts);
// Not the holder, within FG_GOO_RUN of a live impact
bool IsFgInGoo(PlayerbotAI* botAI);
// Where the dodge goes: the bot's own stand when that is clear, else the nearest clear spot. Notes
// fg.goo with the branch.
bool GetFgGooDodge(PlayerbotAI* botAI, Position& spot);

bool IsFgHoldingStand(PlayerbotAI* botAI);
// The held stand wants a walk, and its spot is clear of goo
bool GetFgStandWalk(PlayerbotAI* botAI, Position& spot);

// fg.phase, the roster, the spore deal, the leg and far anchor flips, the brake, the stand latch, the
// Inhaled Blight cycle, the calls to a human tank, the reset after a wipe
void TickFgEncounter(PlayerbotAI* botAI);

// Every school his raid damage comes in: Gaseous Blight, Blighted Spores and Pungent Blight
uint8 IccFestergutResistSchools(PlayerbotAI* botAI, uint8 previous);

#endif
