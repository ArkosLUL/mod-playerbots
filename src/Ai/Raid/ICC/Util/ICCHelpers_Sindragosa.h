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

constexpr uint32 SG_DATA_SINDRAGOSA = 11;
constexpr uint32 NPC_SG_SINDRAGOSA = 36853;
constexpr uint32 NPC_SG_ICE_TOMB = 36980;
constexpr uint32 NPC_SG_FROST_BOMB = 37186;

constexpr uint32 SPELL_SG_BLISTERING_COLD = 70123;
constexpr uint32 SPELL_SG_FROST_BREATH = 69649;
constexpr uint32 SPELL_SG_FROST_BREATH_LAST = 73061;
constexpr uint32 SPELL_SG_MYSTIC_BUFFET = 70127;
constexpr uint32 SPELL_SG_UNCHAINED_MAGIC = 69762;
constexpr uint32 SPELL_SG_INSTABILITY = 69766;
constexpr uint32 SPELL_SG_CHILLED_TO_THE_BONE = 70106;
constexpr uint32 SPELL_SG_FROST_BEACON = 70126;
constexpr uint32 SPELL_SG_ICE_TOMB = 70157;
constexpr uint32 SPELL_SG_ASPHYXIATION = 71665;
constexpr uint32 SPELL_SG_FROST_BOMB_VISUAL = 70022;
constexpr uint32 SPELL_SG_HAND_OF_FREEDOM = 1044;

inline Position const ICC_SG_LAND(4419.19f, 2484.57f, 203.385f);
inline Position const ICC_SG_FAR_STATION(4525.6f, 2485.15f, 245.082f);
inline Position const ICC_SG_STAGE(4346.0f, 2484.57f, 206.2f);
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

IccSindragosaPhaseId IccSindragosaPhase(Player* bot, Unit const* boss);

Creature* GetSgBoss(PlayerbotAI* botAI);
bool IsSgLive(PlayerbotAI* botAI);
bool IsSgPresent(PlayerbotAI* botAI);

Unit* GetSgTarget(PlayerbotAI* botAI);
bool IsSgTargetHeld(PlayerbotAI* botAI);

bool GetSgColdWalk(PlayerbotAI* botAI, Position& spot);
bool GetSgSplashWalk(PlayerbotAI* botAI, Position& spot);
bool GetSgBombHideWalk(PlayerbotAI* botAI, Position& spot);
bool GetSgBeaconWalk(PlayerbotAI* botAI, Position& spot);
bool GetSgBuffetHideWalk(PlayerbotAI* botAI, Position& spot);
bool GetSgUnchainedWalk(PlayerbotAI* botAI, Position& spot);
bool GetSgStandWalk(PlayerbotAI* botAI, Position& spot);
bool IsSgMovementOwned(PlayerbotAI* botAI);
bool IsSgStandHalted(PlayerbotAI* botAI);

Unit* GetSgTauntTarget(PlayerbotAI* botAI, bool& swap);
void NoteSgSwapTaunt(PlayerbotAI* botAI);

bool IsSgUnchainedHeld(PlayerbotAI* botAI);
bool IsSgRestrained(PlayerbotAI* botAI);

constexpr uint8 SG_HOLD_HER = 1 << 0;
constexpr uint8 SG_HOLD_ALL = 1 << 1;
constexpr uint8 SG_HOLD_CAST = 1 << 2;
constexpr uint8 SG_HOLD_TOMB = 1 << 3;
uint8 GetSgHoldDue(PlayerbotAI* botAI);

bool IsSgHeldTomb(PlayerbotAI* botAI, Unit* unit);
bool IsSgAtTombFloor(Unit* tomb);

struct SgHeldTomb
{
    ObjectGuid guid;
    Position pos;
};

void GetSgHeldTombs(PlayerbotAI* botAI, std::vector<SgHeldTomb>& out);

uint32 GetSgTankDefensive(PlayerbotAI* botAI, char const* noteKind);
bool IsSgTankDefensiveHeld(PlayerbotAI* botAI);

Unit* GetSgFreedomTarget(PlayerbotAI* botAI);
Unit* GetSgCoverTarget(PlayerbotAI* botAI, std::string& spell);
Unit* GetSgBeaconHealTarget(PlayerbotAI* botAI, std::string& spell);

void TickSgEncounter(PlayerbotAI* botAI);

RaidBurstWindow IccSindragosaBurstWindow(PlayerbotAI* botAI);
std::vector<uint32> const& GetSgOpenerBosses(PlayerbotAI* botAI);
uint8 IccSindragosaResistSchools(PlayerbotAI* botAI, uint8 previous);

#endif
