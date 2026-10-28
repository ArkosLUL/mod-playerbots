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

// Awaken Plagued Zombies (71159) summons this, eleven at a time. Precious despawns them when she dies.
constexpr uint32 NPC_DOGS_PLAGUED_ZOMBIE = 38104;

// One id on every difficulty. Decimate is a 3 s cast that sets everyone within 100 yd (SpellInfoCorrections)
// above 15% health to 15%, then stuns the dog itself for 3 s.
constexpr uint32 SPELL_DOGS_MORTAL_WOUND = 71127;
constexpr uint32 SPELL_DOGS_DECIMATE = 71123;
// Stinky's aura ticks 71160 every 3 s at everyone within 100 yd, row 1962: remap the tick at the call
constexpr uint32 SPELL_DOGS_PLAGUE_STENCH = 71805;
constexpr uint32 SPELL_DOGS_PLAGUE_STENCH_TICK = 71160;

// Mortal Wound takes 10% off healing taken a stack, lasts 15 s and lands every 1.5-2.5 s. A tank holding a
// dog at SWAP_STACKS is taken over by one carrying SWAP_LEAD fewer.
constexpr uint32 DOGS_SWAP_STACKS = 6;
constexpr uint32 DOGS_SWAP_LEAD = 4;
constexpr float DOGS_TANK_DEFENSIVE_PCT = 40.0f;
constexpr float DOGS_TANK_LAST_RESORT_PCT = 25.0f;
// The raid cover after Decimate: two Plague Stench ticks from the moment it landed
constexpr uint32 DOGS_DECIMATE_HIT_MS = 6000;

// The dog in combat with the bot or its group, from the encounter gate
Creature* GetDogsBoss(PlayerbotAI* botAI);
bool IsDogsLive(PlayerbotAI* botAI);

// A player's skull, else the holder's dog, a free tank's zombie on a non-tank, the dog. A yielding tank gets
// a zombie or nothing. Notes dogs.target.
Unit* GetDogsTarget(PlayerbotAI* botAI);

// This bot is the tank to take the dog: a tank holds it at DOGS_SWAP_STACKS or a non-tank holds it, and this
// one carries DOGS_SWAP_LEAD fewer, the fewest, its taunt ready. Notes dogs.swap.
bool IsDogsSwapTaunter(PlayerbotAI* botAI);
// A tank not holding the dog that still carries Mortal Wound. Notes dogs.yield.
bool IsDogsYielding(PlayerbotAI* botAI);

// The button this tank spends now, 0 for none: the holder through Decimate's cast, any tank under
// DOGS_TANK_DEFENSIVE_PCT. Physical only.
uint32 GetDogsTankDefensive(PlayerbotAI* botAI, char const* noteKind);
bool IsDogsTankDefensiveHeld(PlayerbotAI* botAI);

// While the dog stuns itself after Decimate, with everyone at 15%
bool GetDogsDecimateHit(PlayerbotAI* botAI, RaidPredictedHit& hit);
RaidCover GetDogsDecimateCover(PlayerbotAI* botAI);

// The pull mark, the calls to a human tank, the reset after the fight
void TickDogsEncounter(PlayerbotAI* botAI);

#endif
