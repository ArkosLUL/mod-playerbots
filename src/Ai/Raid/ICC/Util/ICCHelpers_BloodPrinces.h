/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCHELPERS_BLOODPRINCES_H
#define PLAYERBOTS_ICCHELPERS_BLOODPRINCES_H

#include <vector>

#include "Define.h"
#include "Position.h"
#include "RaidBurstWindow.h"

class Creature;
class Player;
class PlayerbotAI;
class Unit;
struct RaidPredictedHit;

constexpr uint32 NPC_BPC_VALANAR = 37970;
constexpr uint32 NPC_BPC_KELESETH = 37972;
constexpr uint32 NPC_BPC_TALDARAM = 37973;
constexpr uint32 NPC_BPC_DARK_NUCLEUS = 38369;
constexpr uint32 NPC_BPC_BALL_OF_FLAME = 38332;
constexpr uint32 NPC_BPC_BALL_OF_INFERNO_FLAME = 38451;
constexpr uint32 NPC_BPC_KINETIC_BOMB = 38454;
constexpr uint32 NPC_BPC_SHOCK_VORTEX = 38422;

constexpr uint32 BPC_DATA_KELESETH_GUID = 19;
constexpr uint32 BPC_DATA_TALDARAM_GUID = 20;
constexpr uint32 BPC_DATA_VALANAR_GUID = 21;

constexpr uint32 SPELL_BPC_INVOCATION_VALANAR = 70952;
constexpr uint32 SPELL_BPC_INVOCATION_KELESETH = 70981;
constexpr uint32 SPELL_BPC_INVOCATION_TALDARAM = 70982;
constexpr uint32 SPELL_BPC_SHADOW_RESONANCE = 71822;
constexpr uint32 SPELL_BPC_KINETIC_BOMB_KNOCKBACK = 72087;
constexpr uint32 SPELL_BPC_EMPOWERED_SHOCK_VORTEX = 72039;
constexpr uint32 SPELL_BPC_BALL_OF_FLAMES_PROC = 71756;
constexpr uint32 SPELL_BPC_FLAMES = 71393;
constexpr uint32 SPELL_BPC_SHADOW_LANCE = 71405;
constexpr uint32 SPELL_BPC_EMPOWERED_SHADOW_LANCE = 71815;
constexpr uint32 SPELL_BPC_GLITTERING_SPARKS = 71807;

constexpr uint8 BPC_PRINCE_VALANAR = 1;
constexpr uint8 BPC_PRINCE_KELESETH = 2;
constexpr uint8 BPC_PRINCE_TALDARAM = 4;

inline Position const ICC_BPC_ROOM_CENTER(4638.71f, 2769.37f, 361.17f);
constexpr float BPC_ROOM_RADIUS = 45.0f;

constexpr float BPC_NODE_SPACING = 13.5f;
inline Position const ICC_BPC_VALANAR_SPOT(4655.00f, 2749.00f, 361.187f);
inline Position const ICC_BPC_TALDARAM_SPOT(4643.31f, 2742.25f, 361.178f);
inline Position const ICC_BPC_KELESETH_SPOT(4655.00f, 2789.50f, 361.187f);
inline Position const ICC_BPC_NODES[] = {
    {4643.31f, 2742.25f, 361.178f}, {4643.31f, 2728.75f, 361.164f}, {4655.00f, 2735.50f, 361.178f},
    {4666.69f, 2742.25f, 361.266f}, {4678.38f, 2749.00f, 363.798f}, {4619.93f, 2728.75f, 361.160f},
    {4631.62f, 2735.50f, 361.167f}, {4666.69f, 2755.75f, 362.261f}, {4678.38f, 2762.50f, 364.086f},
    {4619.93f, 2742.25f, 361.164f}, {4631.62f, 2749.00f, 361.167f}, {4643.31f, 2755.75f, 361.178f},
    {4655.00f, 2762.50f, 361.189f}, {4666.69f, 2769.25f, 362.756f}, {4678.38f, 2776.00f, 364.086f},
    {4608.23f, 2749.00f, 361.164f}, {4619.93f, 2755.75f, 361.164f}, {4631.62f, 2762.50f, 361.167f},
    {4643.31f, 2769.25f, 361.178f}, {4655.00f, 2776.00f, 361.189f}, {4666.69f, 2782.75f, 362.267f},
    {4678.38f, 2789.50f, 363.815f}, {4596.54f, 2755.75f, 361.153f}, {4608.23f, 2762.50f, 361.164f},
    {4619.93f, 2769.25f, 361.164f}, {4631.62f, 2776.00f, 361.167f}, {4643.31f, 2782.75f, 361.178f},
    {4666.69f, 2796.25f, 361.283f}, {4596.54f, 2769.25f, 361.153f}, {4608.23f, 2776.00f, 361.164f},
    {4619.93f, 2782.75f, 361.164f}, {4631.62f, 2789.50f, 361.167f}, {4643.31f, 2796.25f, 361.178f},
    {4655.00f, 2803.00f, 361.178f}, {4596.54f, 2782.75f, 361.153f}, {4608.23f, 2789.50f, 361.164f},
    {4619.93f, 2796.25f, 361.164f}, {4631.62f, 2803.00f, 361.167f}, {4643.31f, 2809.75f, 361.164f},
    {4608.23f, 2803.00f, 361.162f}, {4619.93f, 2809.75f, 361.160f},
};
constexpr uint8 BPC_NODE_COUNT = sizeof(ICC_BPC_NODES) / sizeof(ICC_BPC_NODES[0]);
constexpr uint8 BPC_TALDARAM_NODE = 1;

constexpr float BPC_STAND_PARK = 0.75f;
constexpr float BPC_STAND_UNPARK = 2.5f;
static_assert(BPC_STAND_PARK < BPC_STAND_UNPARK, "a walk has to end inside its own trigger");
static_assert(BPC_NODE_SPACING >= 12.0f + 2 * BPC_STAND_PARK, "two parked neighbours must not share a vortex");

constexpr float BPC_TANK_DEFENSIVE_PCT = 40.0f;
constexpr float BPC_TANK_LAST_RESORT_PCT = 25.0f;
constexpr uint8 BPC_NUCLEUS_STACKS = 6;

bool IsBpcLive(PlayerbotAI* botAI);
Creature* GetBpcValanar(PlayerbotAI* botAI);
Creature* GetBpcEmpoweredPrince(PlayerbotAI* botAI);

Unit* GetBpcTarget(PlayerbotAI* botAI);
Unit* GetBpcPetTarget(PlayerbotAI* botAI);
Unit* GetBpcDirectHitTarget(PlayerbotAI* botAI);
bool IsBpcDirectHitPick(PlayerbotAI* botAI);
Creature* GetBpcBrakeTarget(PlayerbotAI* botAI);
bool IsBpcBraked(PlayerbotAI* botAI);
bool IsBpcStandIn(PlayerbotAI* botAI);
Unit* GetBpcPrinceToTaunt(PlayerbotAI* botAI);

Unit* GetBpcNucleusToTaunt(PlayerbotAI* botAI);
bool GetBpcNucleusWalk(PlayerbotAI* botAI, Position& spot);

bool GetBpcBallWalk(PlayerbotAI* botAI, Position& spot);
bool GetBpcSoakWalk(PlayerbotAI* botAI, Position& spot);
bool IsBpcSoakHeld(PlayerbotAI* botAI);

bool IsBpcInVortex(PlayerbotAI* botAI);
bool FindBpcDodgeSpot(PlayerbotAI* botAI, Position& spot);

bool IsBpcHoldingStand(PlayerbotAI* botAI);
bool IsBpcHoldingTankStand(PlayerbotAI* botAI);
bool GetBpcStandWalk(PlayerbotAI* botAI, Position& spot);
bool IsBpcScattering(PlayerbotAI* botAI);
bool IsBpcMovementOwned(PlayerbotAI* botAI);
bool IsBpcPrisonHeld(PlayerbotAI* botAI);

uint32 GetBpcTankDefensive(PlayerbotAI* botAI, char const* noteKind);
bool IsBpcTankDefensiveHeld(PlayerbotAI* botAI);
bool GetBpcLanceHit(PlayerbotAI* botAI, RaidPredictedHit& hit);
uint32 GetBpcFlameDefensive(PlayerbotAI* botAI);
Player* GetBpcDispelTarget(PlayerbotAI* botAI, char const*& spell);

bool IsBpcAreaDamageHeld(PlayerbotAI* botAI);

void TickBpcEncounter(PlayerbotAI* botAI);

RaidBurstWindow IccBloodPrincesBurstWindow(PlayerbotAI* botAI);
std::vector<uint32> const& GetBpcOpenerBosses(PlayerbotAI* botAI);
uint8 IccBloodPrincesResistSchools(PlayerbotAI* botAI, uint8 previous);

#endif
