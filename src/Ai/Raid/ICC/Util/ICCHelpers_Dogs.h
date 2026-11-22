/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCHELPERS_DOGS_H
#define PLAYERBOTS_ICCHELPERS_DOGS_H

#include "Define.h"
#include "ICCData.h"
#include "RaidDefensive.h"

class Creature;
class PlayerbotAI;
class Unit;

constexpr uint32 NPC_DOGS_PLAGUED_ZOMBIE = 38104;

constexpr uint32 SPELL_DOGS_MORTAL_WOUND = 71127;
constexpr uint32 SPELL_DOGS_DECIMATE = 71123;
constexpr uint32 SPELL_DOGS_PLAGUE_STENCH = 71805;
constexpr uint32 SPELL_DOGS_PLAGUE_STENCH_TICK = 71160;

constexpr uint32 DOGS_SWAP_STACKS = 6;
constexpr uint32 DOGS_SWAP_LEAD = 4;
constexpr float DOGS_BRAKE_CAP = 0.9f;
constexpr float DOGS_BRAKE_RELEASE = 0.8f;
constexpr float DOGS_TANK_DEFENSIVE_PCT = 40.0f;
constexpr float DOGS_TANK_LAST_RESORT_PCT = 25.0f;
constexpr uint32 DOGS_DECIMATE_HIT_MS = 6000;

Creature* GetDogsBoss(PlayerbotAI* botAI);
bool IsDogsLive(PlayerbotAI* botAI);

Unit* GetDogsTarget(PlayerbotAI* botAI);

bool IsDogsSwapTaunter(PlayerbotAI* botAI);
bool IsDogsYielding(PlayerbotAI* botAI);

Unit* GetDogsZombieTaunt(PlayerbotAI* botAI);

uint32 GetDogsTankDefensive(PlayerbotAI* botAI, char const* noteKind);
bool IsDogsTankDefensiveHeld(PlayerbotAI* botAI);

bool GetDogsDecimateHit(PlayerbotAI* botAI, RaidPredictedHit& hit);
RaidCover GetDogsDecimateCover(PlayerbotAI* botAI);

void TickDogsEncounter(PlayerbotAI* botAI);

#endif
