#ifndef PLAYERBOTS_RAID_TOCHELPERS_GORMOK_H
#define PLAYERBOTS_RAID_TOCHELPERS_GORMOK_H

#include "PlayerbotAI.h"
#include "ToCData.h"

namespace TrialOfTheCrusaderHelpers
{

// boss_northrend_beasts.cpp GormokSpells, no difficulty row. On the rider while its snobold lives.
constexpr uint32 SPELL_SNOBOLLED = 66406;

// boss_northrend_beasts.cpp GormokNPCs
constexpr uint32 NPC_FIRE_BOMB = 34854;

// Holder's Impale stacks that send the swap tank in. A taunter still carrying Impale resumes its own
// stacks, so the swap also waits for the taunter's to expire.
constexpr uint32 GORMOK_IMPALE_SWAP_STACKS = 3;
// Holder pops a tank cooldown here, or already at the swap count with no swap tank left
constexpr uint32 GORMOK_IMPALE_DEFENSIVE_STACKS = 5;

// Staggering Stomp's school lockout, centre to centre
constexpr float GORMOK_STOMP_INTERRUPT_RADIUS = 20.0f;
// Casters inside this step out to the clearance, past the trigger so the next tick doesn't re-fire
constexpr float GORMOK_STOMP_TRIGGER = 22.0f;
constexpr float GORMOK_STOMP_CLEARANCE = 24.0f;
static_assert(GORMOK_STOMP_TRIGGER > GORMOK_STOMP_INTERRUPT_RADIUS && GORMOK_STOMP_CLEARANCE > GORMOK_STOMP_TRIGGER);

// Snobold rider inside this of Gormok's centre is in reach of the melee on him
constexpr float GORMOK_CARRIER_MELEE_RANGE = 10.0f;

// Total Impale stack count on a unit (0 if none)
uint32 GetGormokImpaleStacks(Unit* unit);

// Gormok when this bot holds GormokSwap and should taunt him now: his victim is the Gormok holder at
// GORMOK_IMPALE_SWAP_STACKS or more and this bot's own Impale is gone. Notes nb.swap.
Unit* GetGormokSwapTauntTarget(PlayerbotAI* botAI);

// This bot holds Gormok and its Impale stacks call for a cooldown
bool GormokTankNeedsDefensive(PlayerbotAI* botAI);

// The snobold this dps bot should kill, or nullptr. Notes nb.snobold.
Unit* GetGormokSnoboldPick(PlayerbotAI* botAI);

// Gormok while this bot is a ranged dps or healer carrying a snobold and he's engaged
Unit* GetGormokForSnoboldCarrier(PlayerbotAI* botAI);

// Gormok while this bot is a ranged dps or healer inside GORMOK_STOMP_TRIGGER of him, carrying nothing
Unit* GetGormokStompThreat(PlayerbotAI* botAI);

}

#endif
