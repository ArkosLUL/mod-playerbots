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

constexpr uint32 VD_DATA_VALITHRIA_GUID = 10;
constexpr uint32 VD_DATA_COMBAT_TRIGGER_GUID = 35;

constexpr float VD_HOME_X = 4203.65f;
constexpr float VD_HOME_Y = 2483.89f;

constexpr uint32 SPELL_VD_DREAM_PORTAL_VISUAL_PRE = 71304;
constexpr uint32 SPELL_VD_NIGHTMARE_PORTAL_VISUAL_PRE = 71986;
constexpr uint32 SPELL_VD_GUARDIAN_SPIRIT = 47788;
constexpr uint32 SPELL_VD_BEACON_OF_LIGHT = 53563;
constexpr uint32 SPELL_VD_BLOODLUST = 2825;
constexpr uint32 SPELL_VD_HEROISM = 32182;
constexpr uint32 SPELL_VD_EXHAUSTION = 57723;
constexpr uint32 SPELL_VD_SATED = 57724;
constexpr uint32 SPELL_VD_COLUMN_OF_FROST = 70702;
constexpr uint32 SPELL_VD_FROSTBOLT_VOLLEY = 70759;
constexpr uint32 SPELL_VD_GUT_SPRAY = 70633;

constexpr float VD_PUSH_PCT = 80.0f;
constexpr float VD_TANK_DEFENSIVE_PCT = 40.0f;
constexpr float VD_TANK_SPRAYED_PCT = 50.0f;
constexpr float VD_TANK_LAST_RESORT_PCT = 25.0f;

constexpr float VD_FLOOR_RADIUS = 30.0f;
constexpr float VD_KITE_RADIUS = 25.0f;
constexpr uint8 VD_KITE_POINTS = 24;

constexpr float VD_STAND_PARK = 1.0f;
constexpr float VD_STAND_UNPARK = 2.5f;
static_assert(VD_STAND_PARK < VD_STAND_UNPARK, "a walk has to end inside its own trigger");

constexpr uint8 VD_NIGHTMARE_STACK_CAP = 25;

enum class VdFlight : uint8
{
    None,
    Refresh,
    Gather,
    Cloud,
    Human,
    Hover,
    Descend,
    Land
};

bool VdGateOpen(PlayerbotAI* botAI, uint32 encounterId);
inline constexpr EncounterGate VdGate{&VdGateOpen, &BossStateGateLive};

bool IsVdLive(PlayerbotAI* botAI);
Creature* GetVdValithria(PlayerbotAI* botAI);
bool IsVdDreaming(PlayerbotAI* botAI);

Unit* GetVdTarget(PlayerbotAI* botAI);
Unit* GetVdTankTaunt(PlayerbotAI* botAI);
Unit* GetVdZombieToSnare(PlayerbotAI* botAI, char const*& spell);

bool IsVdInDanger(PlayerbotAI* botAI);
bool FindVdDodgeSpot(PlayerbotAI* botAI, Position& spot);
bool GetVdVoidStep(PlayerbotAI* botAI, Position& spot);

bool IsVdKiting(PlayerbotAI* botAI);
bool GetVdKiteLeg(PlayerbotAI* botAI, Position& leg);

bool GetVdPortalWalk(PlayerbotAI* botAI, Position& spot, Creature*& portal);
bool IsVdHoldingPortal(PlayerbotAI* botAI);
Creature* GetVdHealTarget(PlayerbotAI* botAI);

VdFlight GetVdFlight(PlayerbotAI* botAI, Position& dest);

bool IsVdPushWindow(PlayerbotAI* botAI);
char const* GetVdLust(PlayerbotAI* botAI);
Creature* GetVdGuardianSpiritTarget(PlayerbotAI* botAI);
bool IsVdBeaconOnHer(PlayerbotAI* botAI);
Creature* GetVdBeaconTarget(PlayerbotAI* botAI);
Unit* GetVdInterrupt(PlayerbotAI* botAI, char const*& spell);
Unit* GetVdFreedom(PlayerbotAI* botAI, char const*& spell);
uint32 GetVdTankDefensive(PlayerbotAI* botAI, char const* noteKind);
bool IsVdTankDefensiveHeld(PlayerbotAI* botAI);

bool IsVdMovementOwned(PlayerbotAI* botAI);
bool IsVdKiteHeld(PlayerbotAI* botAI);

bool IsVdSplashHeld(PlayerbotAI* botAI);

void TickVdEncounter(PlayerbotAI* botAI);

uint8 IccValithriaResistSchools(PlayerbotAI* botAI, uint8 previous);

#endif
