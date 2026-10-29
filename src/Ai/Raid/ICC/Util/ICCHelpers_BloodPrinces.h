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

// icecrown_citadel.h CreaturesIds. GetEntry reports these on every difficulty.
constexpr uint32 NPC_BPC_VALANAR = 37970;
constexpr uint32 NPC_BPC_KELESETH = 37972;
constexpr uint32 NPC_BPC_TALDARAM = 37973;
constexpr uint32 NPC_BPC_DARK_NUCLEUS = 38369;
constexpr uint32 NPC_BPC_BALL_OF_FLAME = 38332;
constexpr uint32 NPC_BPC_BALL_OF_INFERNO_FLAME = 38451;
constexpr uint32 NPC_BPC_KINETIC_BOMB = 38454;
constexpr uint32 NPC_BPC_SHOCK_VORTEX = 38422;

// icecrown_citadel.h DataTypes: the instance's guid slots for the three princes
constexpr uint32 BPC_DATA_KELESETH_GUID = 19;
constexpr uint32 BPC_DATA_TALDARAM_GUID = 20;
constexpr uint32 BPC_DATA_VALANAR_GUID = 21;

// No difficulty row: one id on every difficulty
constexpr uint32 SPELL_BPC_INVOCATION_VALANAR = 70952;
constexpr uint32 SPELL_BPC_INVOCATION_KELESETH = 70981;
constexpr uint32 SPELL_BPC_INVOCATION_TALDARAM = 70982;
// A Dark Nucleus' beam on its victim, one application per nucleus: -35% shadow damage taken, 1000 a 3 s tick
constexpr uint32 SPELL_BPC_SHADOW_RESONANCE = 71822;
// On a Kinetic Bomb for 3 s after a hit, while it climbs
constexpr uint32 SPELL_BPC_KINETIC_BOMB_KNOCKBACK = 72087;
// Heroic: a stack for every second a player moves, gone after 10 s standing still
constexpr uint32 SPELL_BPC_SHADOW_PRISON = 72999;
// Difficulty rows: remap at the call
constexpr uint32 SPELL_BPC_EMPOWERED_SHOCK_VORTEX = 72039;
constexpr uint32 SPELL_BPC_BALL_OF_FLAMES_PROC = 71756;

// Prince bits in bpc.tank
constexpr uint8 BPC_PRINCE_VALANAR = 1;
constexpr uint8 BPC_PRINCE_KELESETH = 2;
constexpr uint8 BPC_PRINCE_TALDARAM = 4;

// The room's floor is flat at 361.16 out to 45 yd from here, the princes' dais east of x 4662 rises to 364.1
inline Position const ICC_BPC_ROOM_CENTER(4638.71f, 2769.37f, 361.17f);
constexpr float BPC_ROOM_RADIUS = 45.0f;

// Every tank spot and spread node is a point of one triangular lattice with 13.5 yd sides: Empowered
// Shock Vortex hits 12 yd round every player within 30 yd of Valanar, plus both park tolerances.
// navprobe --nav 0x09 settles every point within 0.1 yd of the Z given, and every node sees all three
// spots and Keleseth's place 10 yd east of his.
constexpr float BPC_NODE_SPACING = 13.5f;
// Valanar, and Taldaram as well with fewer than three tanks
inline Position const ICC_BPC_VALANAR_SPOT(4655.00f, 2749.00f, 361.187f);
// Taldaram's own tank, with three or more tanks; a spread node otherwise
inline Position const ICC_BPC_TALDARAM_SPOT(4643.31f, 2742.25f, 361.178f);
inline Position const ICC_BPC_KELESETH_SPOT(4655.00f, 2789.50f, 361.187f);
// Node 1 is the Taldaram spot. The east column climbs the dais stairs.
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

// A stand parks within BPC_STAND_PARK and walks back once pushed past BPC_STAND_UNPARK
constexpr float BPC_STAND_PARK = 0.75f;
constexpr float BPC_STAND_UNPARK = 2.5f;
static_assert(BPC_STAND_PARK < BPC_STAND_UNPARK, "a walk has to end inside its own trigger");
static_assert(BPC_NODE_SPACING >= 12.0f + 2 * BPC_STAND_PARK, "two parked neighbours must not share a vortex");

// Any tank under BPC_TANK_DEFENSIVE_PCT spends a button. Under BPC_TANK_LAST_RESORT_PCT its class buttons
// are free again.
constexpr float BPC_TANK_DEFENSIVE_PCT = 40.0f;
constexpr float BPC_TANK_LAST_RESORT_PCT = 25.0f;
// Keleseth's tank stops collecting nuclei here: past six the ticks cost more than the cut saves
constexpr uint8 BPC_NUCLEUS_STACKS = 6;

// Valanar is alive and in combat and the bot is in the room. Any prince's engage pulls him in the same
// update, so this is the whole fight.
bool IsBpcLive(PlayerbotAI* botAI);
Creature* GetBpcValanar(PlayerbotAI* botAI);
// The prince Invocation of Blood is on, the only one that takes damage, null between pulls. Publishes
// bpc.phase.
Creature* GetBpcEmpoweredPrince(PlayerbotAI* botAI);

// Tanks and DPS: the tank's own princes, a bomb a duty bot shoots, a player's skull, else the empowered
// prince. Notes bpc.target.
Unit* GetBpcTarget(PlayerbotAI* botAI);
// A prince this tank owns, held by somebody else, its taunt ready and the prince below taunt DR level 3
Unit* GetBpcPrinceToTaunt(PlayerbotAI* botAI);

// Keleseth's tank: the nucleus to taunt now, or a walk toward one out of taunt range. Notes bpc.nucleus.
Unit* GetBpcNucleusToTaunt(PlayerbotAI* botAI);
bool GetBpcNucleusWalk(PlayerbotAI* botAI, Position& spot);

// The bomb this duty bot shoots now: one that falls low and carries no knockback
Unit* GetBpcBombShot(PlayerbotAI* botAI);
bool IsBpcBombShooting(PlayerbotAI* botAI);

// A ball's victim walks clear of everyone else. Notes bpc.ball.
bool GetBpcBallWalk(PlayerbotAI* botAI, Position& spot);
// A soaker's intercept point on a Ball of Inferno Flame's path, false while it should stand where it is
bool GetBpcSoakWalk(PlayerbotAI* botAI, Position& spot);

// A bot near a Shock Vortex with no node to fall back on. Notes bpc.dodge.
bool IsBpcInVortex(PlayerbotAI* botAI);
bool FindBpcDodgeSpot(PlayerbotAI* botAI, Position& spot);

bool IsBpcHoldingStand(PlayerbotAI* botAI);
bool IsBpcHoldingTankStand(PlayerbotAI* botAI);
bool GetBpcStandWalk(PlayerbotAI* botAI, Position& spot);
// The Empowered Shock Vortex cast is on and this bot runs to a node: it drops a cast to get there
bool IsBpcScattering(PlayerbotAI* botAI);
// A stand, a dodge, a ball, a soak or a nucleus walk owns the bot's feet
bool IsBpcMovementOwned(PlayerbotAI* botAI);
// Heroic: every move stacks Shadow Prison, so only the encounter's movers and reach move a bot
bool IsBpcPrisonHeld(PlayerbotAI* botAI);

// The button this tank spends now, 0 for none: Keleseth's tank through his empowered Shadow Lance, any
// tank under BPC_TANK_DEFENSIVE_PCT
uint32 GetBpcTankDefensive(PlayerbotAI* botAI, char const* noteKind);
bool IsBpcTankDefensiveHeld(PlayerbotAI* botAI);

// A damage dealer whose area attack would reach a prince without the Invocation, which turns the damage
// into threat, or a Dark Nucleus, which then leaves its tank
bool IsBpcAreaDamageHeld(PlayerbotAI* botAI);

// The scan, the tank roster, the nodes, the bomb and soak duties, the stand latch, the calls to a human
// tank, the reset after a wipe
void TickBpcEncounter(PlayerbotAI* botAI);

// Lust waits for Keleseth's phase
RaidBurstWindow IccBloodPrincesBurstWindow(PlayerbotAI* botAI);
std::vector<uint32> const& GetBpcOpenerBosses(PlayerbotAI* botAI);
// Shadow (Shadow Lance, Shadow Resonance, heroic Shadow Prison) and fire (Flames, Empowered Flare,
// Glittering Sparks) all fight
uint8 IccBloodPrincesResistSchools(PlayerbotAI* botAI, uint8 previous);

#endif
