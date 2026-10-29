/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCHELPERS_SINDRAGOSA_H
#define PLAYERBOTS_ICCHELPERS_SINDRAGOSA_H

#include <string>
#include <vector>

#include "Define.h"
#include "ObjectGuid.h"
#include "Position.h"
#include "RaidBurstWindow.h"

class Creature;
class Player;
class PlayerbotAI;
class Unit;

// icecrown_citadel.h: her boss id and guid slot, and what her air phase leaves on the floor. GetEntry
// reports these on every difficulty.
constexpr uint32 SG_DATA_SINDRAGOSA = 11;
constexpr uint32 NPC_SG_SINDRAGOSA = 36853;
constexpr uint32 NPC_SG_ICE_TOMB = 36980;
constexpr uint32 NPC_SG_FROST_BOMB = 37186;

// Difficulty rows: remap at the call
constexpr uint32 SPELL_SG_BLISTERING_COLD = 70123;
constexpr uint32 SPELL_SG_FROST_BREATH = 69649;
constexpr uint32 SPELL_SG_FROST_BREATH_LAST = 73061;
constexpr uint32 SPELL_SG_MYSTIC_BUFFET = 70127;
// No difficulty row: one id on every difficulty
constexpr uint32 SPELL_SG_UNCHAINED_MAGIC = 69762;
constexpr uint32 SPELL_SG_INSTABILITY = 69766;
constexpr uint32 SPELL_SG_CHILLED_TO_THE_BONE = 70106;
constexpr uint32 SPELL_SG_FROST_BEACON = 70126;
// On the prisoner of a tomb
constexpr uint32 SPELL_SG_ICE_TOMB = 70157;
constexpr uint32 SPELL_SG_ASPHYXIATION = 71665;
// On a bomb marker until it goes off
constexpr uint32 SPELL_SG_FROST_BOMB_VISUAL = 70022;
constexpr uint32 SPELL_SG_HAND_OF_FREEDOM = 1044;

// Where she lands after the fly-in and after every air phase
inline Position const ICC_SG_LAND(4419.19f, 2484.57f, 203.385f);
// Her far air station, where she drops the bombs. The near one, where she casts the beacons, is 49.6 yd off.
inline Position const ICC_SG_FAR_STATION(4525.6f, 2485.15f, 245.082f);
// On the stairs, out of the bombs' drop box (x 4350-4425)
inline Position const ICC_SG_STAGE(4346.0f, 2484.57f, 206.2f);
// Air beacon spots at the stair foot, in assignment order. navprobe --nav 0x09 settles each within 0.03 of
// 203.38, and paths from the stage reach them. Ice Tomb hits everyone within 10 yd, so a pair 6 yd apart takes
// two hits each and the next pair sits 13 yd off, out of it even with both parked a yard short.
inline Position const ICC_SG_BEACON_SPOTS_25[] = {
    {4362.0f, 2471.57f, 203.38f}, {4362.0f, 2497.57f, 203.38f}, {4368.0f, 2471.57f, 203.38f},
    {4368.0f, 2497.57f, 203.38f}, {4362.0f, 2484.57f, 203.38f}, {4368.0f, 2484.57f, 203.38f}};
inline Position const ICC_SG_BEACON_SPOTS_10[] = {{4364.0f, 2478.07f, 203.38f}, {4364.0f, 2491.07f, 203.38f}};

enum class IccSindragosaPhaseId : uint8
{
    Ground,
    Air,
    Last,
};

// Sindragosa's phase as her script runs it: Air from takeoff to touchdown, while it holds her passive,
// else Last once her health is no longer above 35%. Publishes sg.phase.
IccSindragosaPhaseId IccSindragosaPhase(Player* bot, Unit const* boss);

// Her unit from the instance's guid slot, alive, with the bot within 120 yd of her landing spot: the gate
// stays open between pulls and through the drakes, so nothing of hers may run without her
Creature* GetSgBoss(PlayerbotAI* botAI);
// The same, in combat with the raid
bool IsSgLive(PlayerbotAI* botAI);

// Tanks and DPS: a player's skull on her or a tomb, then tombs and her per the phase. Nothing for a bot
// holding its spells or hiding from Mystic Buffet, nor on her for a braked or chilled bot. Notes sg.target.
Unit* GetSgTarget(PlayerbotAI* botAI);
// A tank or DPS bot the resolver hands nothing while she is live
bool IsSgTargetHeld(PlayerbotAI* botAI);

// Each walk answers true with its spot while it is the bot's current walk and the bot has not parked
bool GetSgColdWalk(PlayerbotAI* botAI, Position& spot);
bool GetSgSplashWalk(PlayerbotAI* botAI, Position& spot);
bool GetSgBombHideWalk(PlayerbotAI* botAI, Position& spot);
bool GetSgBeaconWalk(PlayerbotAI* botAI, Position& spot);
bool GetSgBuffetHideWalk(PlayerbotAI* botAI, Position& spot);
bool GetSgUnchainedWalk(PlayerbotAI* botAI, Position& spot);
bool GetSgStandWalk(PlayerbotAI* botAI, Position& spot);
// A walk, a parked walk or the air phase's hold owns the bot's feet
bool IsSgMovementOwned(PlayerbotAI* botAI);

// Her, when this bot's taunt is due: the holder's taunt back off a non-tank, or the last phase swap.
// `swap` says which.
Unit* GetSgTauntTarget(PlayerbotAI* botAI, bool& swap);
// The swap taunt landed: the roles flip on the next duty pass
void NoteSgSwapTaunt(PlayerbotAI* botAI);

// Unchained Magic: no spell may go out
bool IsSgUnchainedHeld(PlayerbotAI* botAI);
// Braked or chilled: nothing may land on her
bool IsSgRestrained(PlayerbotAI* botAI);

// What the hold node still has to stop, as SG_HOLD_* bits, 0 for nothing
constexpr uint8 SG_HOLD_HER = 1 << 0;
constexpr uint8 SG_HOLD_ALL = 1 << 1;
constexpr uint8 SG_HOLD_CAST = 1 << 2;
constexpr uint8 SG_HOLD_TOMB = 1 << 3;
uint8 GetSgHoldDue(PlayerbotAI* botAI);

// A tomb whose prisoner needs it to stay up for now: damage takes it no lower than 20%
bool IsSgHeldTomb(PlayerbotAI* botAI, Unit* unit);
bool IsSgAtTombFloor(Unit* tomb);

struct SgHeldTomb
{
    ObjectGuid guid;
    Position pos;
};

// Every tomb held right now
void GetSgHeldTombs(PlayerbotAI* botAI, std::vector<SgHeldTomb>& out);

// The button this tank spends now, 0 for none
uint32 GetSgTankDefensive(PlayerbotAI* botAI, char const* noteKind);
bool IsSgTankDefensiveHeld(PlayerbotAI* botAI);

// The tank this paladin frees from Frost Breath's slow now, or null
Unit* GetSgFreedomTarget(PlayerbotAI* botAI);
// The beaconed member this healer covers before the tomb, and the spell, or null
Unit* GetSgBeaconHealTarget(PlayerbotAI* botAI, std::string& spell);

// The scan, the duties, the tank roster and the swap, the per-bot walks, holds and brake, the calls to a
// human tank, the reset after a wipe
void TickSgEncounter(PlayerbotAI* botAI);

// Lust in the last phase
RaidBurstWindow IccSindragosaBurstWindow(PlayerbotAI* botAI);
std::vector<uint32> const& GetSgOpenerBosses(PlayerbotAI* botAI);
// Frost all fight: Frost Aura, the breaths, Blistering Cold, the tombs and bombs
uint8 IccSindragosaResistSchools(PlayerbotAI* botAI, uint8 previous);

#endif
