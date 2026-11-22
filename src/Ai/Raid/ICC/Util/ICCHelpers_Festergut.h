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

constexpr uint32 NPC_FG_FESTERGUT = 36626;

constexpr uint32 SPELL_FG_GASTRIC_BLOAT = 72219;
constexpr uint32 SPELL_FG_INHALED_BLIGHT = 69166;
constexpr uint32 SPELL_FG_PUNGENT_BLIGHT = 69195;
constexpr uint32 SPELL_FG_INOCULATED = 69291;
constexpr uint32 SPELL_FG_DIVINE_SHIELD = 642;

inline Position const ICC_FG_BOSS_SPOT(4267.92f, 3137.26f, 360.385f);
constexpr float FG_FLOOR_RADIUS = 40.0f;
inline Position const ICC_FG_TANK_SPOTS[] = {
    {4263.74f, 3142.24f, 360.385f},
    {4272.10f, 3142.24f, 360.385f},
};
constexpr uint8 FG_TANK_SPOTS = sizeof(ICC_FG_TANK_SPOTS) / sizeof(ICC_FG_TANK_SPOTS[0]);
inline Position const ICC_FG_CLOSE_ANCHOR(4267.92f, 3142.24f, 360.385f);
inline Position const ICC_FG_LEGS[] = {
    {4262.97f, 3132.31f, 360.385f},
    {4272.87f, 3132.31f, 360.385f},
};

inline Position const ICC_FG_SLOTS[] = {
    {4267.92f, 3119.26f, 360.385f}, {4257.99f, 3122.25f, 360.385f}, {4277.85f, 3122.25f, 360.385f},
    {4251.35f, 3130.23f, 360.385f}, {4284.49f, 3130.23f, 360.385f}, {4250.22f, 3140.54f, 360.385f},
    {4285.62f, 3140.54f, 360.385f}, {4267.92f, 3108.76f, 360.385f}, {4257.71f, 3110.65f, 360.385f},
    {4278.13f, 3110.65f, 360.385f}, {4248.85f, 3116.08f, 360.385f}, {4286.99f, 3116.08f, 360.385f},
    {4242.53f, 3124.32f, 360.385f}, {4293.31f, 3124.32f, 360.385f}, {4239.58f, 3134.28f, 360.385f},
    {4296.26f, 3134.28f, 360.385f}, {4240.39f, 3144.64f, 360.385f}, {4295.45f, 3144.64f, 360.385f},
};
constexpr uint8 FG_SLOT_COUNT = sizeof(ICC_FG_SLOTS) / sizeof(ICC_FG_SLOTS[0]);
inline Position const ICC_FG_FAR_ANCHORS[] = {
    {4267.92f, 3119.26f, 360.385f},
    {4284.49f, 3130.23f, 360.385f},
};

constexpr uint8 FG_STAND_LEG = FG_SLOT_COUNT + 1;
constexpr uint8 FG_STAND_TANK = FG_STAND_LEG + 1;

constexpr float FG_VILE_GAS_RADIUS = 8.0f;
constexpr float FG_STAND_PARK = 0.75f;
constexpr float FG_STAND_UNPARK = 2.5f;
constexpr float FG_SLOT_SPACING = 10.37f;
static_assert(FG_STAND_PARK < FG_STAND_UNPARK, "a walk has to end inside its own trigger");
static_assert(FG_SLOT_SPACING >= FG_VILE_GAS_RADIUS + 2 * FG_STAND_PARK,
              "two parked neighbours must not share Vile Gas");

constexpr float FG_SPORE_BURST = 8.0f;
constexpr float FG_SPORE_RING = 2.5f;
static_assert(FG_SPORE_RING + 2 * FG_STAND_PARK < FG_SPORE_BURST, "the ring sits inside the burst");

constexpr float FG_SHIFT_REACH = FG_SPORE_BURST - FG_STAND_PARK - 0.5f;

constexpr float FG_BOSS_DRIFT = 3.5f;
constexpr float FG_BOSS_SETTLED = 2.5f;
static_assert(FG_BOSS_SETTLED < FG_BOSS_DRIFT, "the drag has to stop short of where it starts");

constexpr float FG_GOO_RADIUS = 5.0f;
constexpr float FG_GOO_RUN = FG_GOO_RADIUS + 1.5f;
constexpr float FG_GOO_CLEARANCE = FG_GOO_RUN + 1.0f;
constexpr uint32 FG_GOO_HAZARD_MS = 8500;

constexpr uint32 FG_SWAP_STACKS = 9;
constexpr uint32 FG_TAUNTER_MAX_STACKS = FG_SWAP_STACKS - 2;
constexpr uint32 FG_BLOAT_CLEAR_GRACE_MS = 6000;

constexpr float FG_BRAKE_CAP = 0.9f;
constexpr float FG_BRAKE_RELEASE = 0.8f;

constexpr uint32 FG_BLIGHT_MINOR = 2;
constexpr uint32 FG_BLIGHT_MAJOR = 3;
constexpr float FG_TANK_DEFENSIVE_PCT = 40.0f;
constexpr float FG_TANK_LAST_RESORT_PCT = 25.0f;

Creature* GetFgBoss(PlayerbotAI* botAI);
bool IsFgFight(PlayerbotAI* botAI);

bool IccFestergutSporeCycle(Player* bot);

Unit* GetFgTarget(PlayerbotAI* botAI);

bool IsFgSwapTaunter(PlayerbotAI* botAI);
bool IsFgBraked(PlayerbotAI* botAI);
bool IsFgBloatClear(PlayerbotAI* botAI);

uint32 GetFgTankDefensive(PlayerbotAI* botAI, char const* noteKind);
bool IsFgTankDefensiveHeld(PlayerbotAI* botAI);
void LatchFgTankDefensive(PlayerbotAI* botAI);

bool GetFgPungentHit(PlayerbotAI* botAI, RaidPredictedHit& hit, bool own);
RaidCover GetFgPungentCover(PlayerbotAI* botAI);
bool IsFgPungentCoverHeld(PlayerbotAI* botAI);

bool GetFgGoo(PlayerbotAI* botAI, std::vector<Position>& impacts);
bool IsFgInGoo(PlayerbotAI* botAI);
bool GetFgGooDodge(PlayerbotAI* botAI, Position& spot);

bool IsFgHoldingStand(PlayerbotAI* botAI);
bool GetFgStandWalk(PlayerbotAI* botAI, Position& spot);

void TickFgEncounter(PlayerbotAI* botAI);

uint8 IccFestergutResistSchools(PlayerbotAI* botAI, uint8 previous);

#endif
