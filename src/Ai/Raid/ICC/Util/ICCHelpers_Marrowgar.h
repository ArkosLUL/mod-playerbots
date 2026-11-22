/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCHELPERS_MARROWGAR_H
#define PLAYERBOTS_ICCHELPERS_MARROWGAR_H

#include <vector>

#include "Define.h"
#include "ICCData.h"
#include "Position.h"
#include "RaidDefensive.h"

class Creature;
class Player;
class PlayerbotAI;
class Unit;

constexpr uint32 NPC_LORD_MARROWGAR = 36612;
constexpr uint32 SPELL_LM_COLDFLAME_PATCH = 69146;
constexpr uint32 SPELL_LM_COLDFLAME_STORM = 72705;
constexpr uint32 SPELL_LM_BONE_STORM_HIT = 69075;

constexpr float LM_COMBAT_REACH = 20.25f;
constexpr float LM_COLDFLAME_REACH = 4.5f;
constexpr float LM_COLDFLAME_STEP = 5.0f;
constexpr float LM_COLDFLAME_FIRST = LM_COMBAT_REACH / 2.0f;
constexpr float LM_INNER_RADIUS = 2.0f;
constexpr float LM_INNER_ARC_DEG = 160.0f;
constexpr float LM_BAIT_RADIUS = 31.0f;
constexpr float LM_BAIT_SPACING_DEG = 20.0f;
constexpr uint32 LM_BAIT_SLOTS = 10;
static_assert(LM_INNER_RADIUS + 1.0f < LM_COLDFLAME_FIRST - LM_COLDFLAME_REACH,
              "a parked inner bot stays clear of every line");
static_assert(LM_BAIT_RADIUS - 1.0f > LM_COMBAT_REACH + 1.5f + 4.0f / 3.0f + 5.0f, "a parked hunter bait can shoot");

constexpr float LM_TANK_STACK_OFFSET = 2.0f;
constexpr float LM_SLICE_KEEPOUT = 8.0f;

constexpr float LM_MELEE_SPIKE_REACH = 10.0f;
constexpr float LM_STORM_RANGED_REACH = 28.0f;

constexpr uint32 LM_SLICE_DELAY_MS = 10000;
constexpr uint32 LM_DEFENSIVE_LEAD_MS = 1000;
constexpr uint32 LM_DEFENSIVE_SPAN_MS = 6000;
constexpr float LM_TANK_DEFENSIVE_PCT = 40.0f;
constexpr float LM_TANK_LAST_RESORT_PCT = 25.0f;

constexpr uint32 LM_STORM_TICK_MS = 2000;
constexpr uint32 LM_STORM_WINDOW_TICKS = 3;
constexpr float LM_STORM_RAID_DISTANCE = 28.0f;

constexpr float LM_STACK_SLOT_RADIUS = 1.5f;

constexpr float LM_STAND_PARK = 1.0f;
constexpr float LM_STAND_UNPARK = 2.5f;
static_assert(LM_STAND_PARK < LM_STAND_UNPARK, "a walk has to end inside its own trigger");

constexpr float LM_DODGE_TRIGGER = LM_COLDFLAME_REACH + 0.5f;
constexpr float LM_DODGE_CLEARANCE = LM_COLDFLAME_REACH + LM_STAND_PARK + 1.5f;
constexpr float LM_DODGE_TIGHT_CLEARANCE = LM_DODGE_TRIGGER + 0.5f;
static_assert(LM_DODGE_TRIGGER < LM_DODGE_TIGHT_CLEARANCE && LM_DODGE_TIGHT_CLEARANCE < LM_DODGE_CLEARANCE);

inline Position const ICC_LM_BOSS_SPOT(-401.37f, 2211.14f, 41.99f);
inline Position const ICC_LM_TANK_SPOT(-415.37f, 2211.14f, 41.99f);
inline Position const ICC_LM_STORM_STACK(-384.0f, 2211.0f, 41.99f);
inline Position const ICC_LM_STORM_ANCHOR_180(-418.0f, 2211.0f, 41.99f);
inline Position const ICC_LM_STORM_ANCHOR_60(-367.0f, 2240.44f, 41.76f);
inline Position const ICC_LM_STORM_ANCHOR_300(-367.0f, 2181.56f, 41.76f);
constexpr float LM_GATE_X_LIMIT = -340.0f;

enum class IccLmPhase : uint8
{
    Normal,
    StormCast,
    Storm
};

enum class IccLmDuty : uint8
{
    None = 0,
    MainTank = 1,
    OffTank = 2,
    Inner = 3,
    Bait = 4
};

bool IsLmLive(PlayerbotAI* botAI);
Creature* GetLmBoss(PlayerbotAI* botAI);
IccLmPhase GetLmPhase(Player* bot, Creature* boss);

Unit* GetLmTarget(PlayerbotAI* botAI);

bool IccLmHoldsStand(PlayerbotAI* botAI);
bool IccLmHoldsNormalStand(PlayerbotAI* botAI);
bool IccLmHoldsStormStand(PlayerbotAI* botAI);
bool GetLmStandSpot(PlayerbotAI* botAI, Position& spot);
bool GetLmStandWalk(PlayerbotAI* botAI, Position& spot);

void GetLmColdflame(PlayerbotAI* botAI, std::vector<Position>& circles);
bool IsLmInColdflame(PlayerbotAI* botAI);

uint32 GetLmTankDefensive(PlayerbotAI* botAI, char const* noteKind);
bool IsLmTankDefensiveHeld(PlayerbotAI* botAI);
void LatchLmTankDefensive(PlayerbotAI* botAI);

bool GetLmStormHit(PlayerbotAI* botAI, RaidPredictedHit& hit);
RaidCover GetLmStormCover(PlayerbotAI* botAI);
bool IsLmStormCoverHeld(PlayerbotAI* botAI);

void TickLmEncounter(PlayerbotAI* botAI);

std::vector<uint32> const& GetLmOpenerBosses(PlayerbotAI* botAI);

#endif
