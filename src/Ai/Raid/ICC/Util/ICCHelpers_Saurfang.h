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

constexpr uint32 NPC_DBS_SAURFANG = 37813;
constexpr uint32 NPC_DBS_BLOOD_BEAST = 38508;

constexpr uint32 SPELL_DBS_RUNE_OF_BLOOD = 72410;
constexpr uint32 SPELL_DBS_MARK = 72293;
constexpr uint32 SPELL_DBS_FRENZY = 72737;
constexpr uint32 SPELL_DBS_BEACON_OF_LIGHT = 53563;
constexpr uint32 SPELL_DBS_FORBEARANCE = 25771;
constexpr uint32 SPELL_DBS_MARK_SPLASH = 72255;
constexpr uint32 SPELL_DBS_BOILING_BLOOD = 72385;

inline Position const ICC_DBS_BOSS_SPOT(-496.35f, 2211.33f, 541.11f);
inline Position const ICC_DBS_TANK_SPOTS[] = {
    {-494.27f, 2211.55f, 541.11f},
    {-494.27f, 2213.05f, 541.11f},
    {-494.27f, 2210.05f, 541.11f},
};
inline Position const ICC_DBS_STACK_SPOT(-499.85f, 2211.33f, 540.64f);

constexpr float DBS_SLOT_SPACING = 13.5f;
inline Position const ICC_DBS_SLOTS[] = {
    {-515.85f, 2211.33f, 539.29f}, {-511.07f, 2224.12f, 539.29f}, {-511.07f, 2198.54f, 539.29f},
    {-499.07f, 2230.64f, 539.29f}, {-499.07f, 2192.02f, 539.29f}, {-525.19f, 2221.26f, 539.29f},
    {-525.19f, 2201.40f, 539.29f}, {-511.60f, 2237.74f, 539.29f}, {-511.60f, 2184.92f, 539.29f},
    {-478.85f, 2211.33f, 541.11f}, {-465.35f, 2211.33f, 541.11f},
};
constexpr uint8 DBS_SLOT_COUNT = sizeof(ICC_DBS_SLOTS) / sizeof(ICC_DBS_SLOTS[0]);

constexpr uint8 DBS_STAND_STACK = DBS_SLOT_COUNT + 1;
constexpr uint8 DBS_STAND_TANK = DBS_STAND_STACK + 1;
constexpr uint8 DBS_TANK_SPOTS = sizeof(ICC_DBS_TANK_SPOTS) / sizeof(ICC_DBS_TANK_SPOTS[0]);

constexpr float DBS_STAND_PARK = 0.75f;
constexpr float DBS_STAND_UNPARK = 2.5f;
static_assert(DBS_STAND_PARK < DBS_STAND_UNPARK, "a walk has to end inside its own trigger");
static_assert(DBS_SLOT_SPACING >= 12.0f + 2 * DBS_STAND_PARK, "two parked neighbours must not share a Blood Nova");

constexpr uint32 DBS_BLOOD_POWER_PEAK = 70;
constexpr uint32 DBS_BLOOD_POWER_PEAK_MARKED = 40;
constexpr uint32 DBS_BLOOD_POWER_RESET_DROP = 30;
constexpr float DBS_TANK_DEFENSIVE_PCT = 40.0f;
constexpr float DBS_TANK_LAST_RESORT_PCT = 25.0f;
constexpr int32 DBS_PROTECT_MIN_LEFT_MS = 7000;
constexpr float DBS_PROTECT_RANGE = 30.0f;

bool IsDbsLive(PlayerbotAI* botAI);
Creature* GetDbsBoss(PlayerbotAI* botAI);
bool IsDbsArmyHeld(PlayerbotAI* botAI);

Unit* GetDbsTarget(PlayerbotAI* botAI);

bool IsDbsSwapTaunter(PlayerbotAI* botAI);
bool IsDbsRuneYielding(PlayerbotAI* botAI);
Unit* GetDbsBeastToTaunt(PlayerbotAI* botAI);

Unit* GetDbsControlDuty(PlayerbotAI* botAI, char const*& spell);

Player* GetDbsMarkWork(PlayerbotAI* botAI, bool& beacon);
bool IsDbsBeaconDutyHeld(PlayerbotAI* botAI);

Player* GetDbsProtectTarget(PlayerbotAI* botAI);

uint32 GetDbsTankDefensive(PlayerbotAI* botAI, char const* noteKind);
bool IsDbsTankDefensiveHeld(PlayerbotAI* botAI);
void LatchDbsTankDefensive(PlayerbotAI* botAI);

bool GetDbsMarkHit(PlayerbotAI* botAI, RaidPredictedHit& hit);

bool IsDbsHoldingStand(PlayerbotAI* botAI);
bool IsDbsHoldingSpreadStand(PlayerbotAI* botAI);
bool IsDbsHoldingTankStand(PlayerbotAI* botAI);
bool GetDbsStandWalk(PlayerbotAI* botAI, Position& spot);

void TickDbsEncounter(PlayerbotAI* botAI);

RaidBurstWindow IccSaurfangBurstWindow(PlayerbotAI* botAI);
std::vector<uint32> const& GetDbsOpenerBosses(PlayerbotAI* botAI);

#endif
