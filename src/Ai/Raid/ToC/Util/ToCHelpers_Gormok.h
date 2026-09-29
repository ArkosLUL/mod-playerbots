#ifndef PLAYERBOTS_RAID_TOCHELPERS_GORMOK_H
#define PLAYERBOTS_RAID_TOCHELPERS_GORMOK_H

#include <vector>

#include "ObjectDefines.h"
#include "PlayerbotAI.h"
#include "Position.h"
#include "ToCData.h"

namespace TrialOfTheCrusaderHelpers
{

// boss_northrend_beasts.cpp GormokSpells, no difficulty row. On the rider while its snobold lives.
constexpr uint32 SPELL_SNOBOLLED = 66406;
// Fire Bomb's 8 yd hit round the bomb, from the snobold's 66313. No difficulty row.
constexpr uint32 SPELL_FIRE_BOMB_IMPACT = 66317;

// boss_northrend_beasts.cpp GormokNPCs
constexpr uint32 NPC_FIRE_BOMB = 34854;

// The bomb is a 60 s TEMPSUMMON_TIMED_DESPAWN, so its age is the lifetime minus GetTimer(). It spawns
// on its target, and 66317 hits after the 1 s cast of 66313 plus the 14 yd/s flight from the thrower.
constexpr uint32 FIRE_BOMB_LIFETIME_MS = 60000;
constexpr uint32 FIRE_BOMB_CAST_MS = 1000;
constexpr uint32 FIRE_BOMB_IMPACT_SLACK_MS = 500;
// Impact age when neither the thrower nor Gormok resolves
constexpr uint32 FIRE_BOMB_FALLBACK_IMPACT_MS = 5000;
constexpr uint32 FIRE_BOMB_SCAN_MS = 200;
constexpr float FIRE_BOMB_MISSILE_SPEED = 14.0f;
// Centre to centre: the bomb is a creature and 66317 an area round it
constexpr float FIRE_BOMB_IMPACT_RADIUS = 8.0f;
// A dodge lands past its own trigger, or the next tick fires it again
constexpr float FIRE_BOMB_TRIGGER = 9.0f;
constexpr float FIRE_BOMB_TIGHT_CLEARANCE = 9.5f;
constexpr float FIRE_BOMB_CLEARANCE = 11.0f;
// A landed bomb pulses 66320 at 2 yd, and `avoid aoe` flees it from 2 yd past both combat reaches:
// the bomb's 1 and a player's 1.5. A dodge spot inside that sets `avoid aoe` off, and its flee can
// step back into a young bomb's trigger.
constexpr float FIRE_BOMB_PULSE_RADIUS = 2.0f;
constexpr float FIRE_BOMB_COMBAT_REACH = 1.0f;
constexpr float FIRE_BOMB_PULSE_CLEARANCE = 6.0f;
constexpr float FIRE_BOMB_SWEEP_RADIUS = 20.0f;
static_assert(FIRE_BOMB_IMPACT_RADIUS < FIRE_BOMB_TRIGGER && FIRE_BOMB_TRIGGER < FIRE_BOMB_TIGHT_CLEARANCE &&
              FIRE_BOMB_TIGHT_CLEARANCE < FIRE_BOMB_CLEARANCE);
static_assert(FIRE_BOMB_PULSE_CLEARANCE > FIRE_BOMB_PULSE_RADIUS + FIRE_BOMB_COMBAT_REACH + DEFAULT_COMBAT_REACH,
              "a dodge spot sits past avoid aoe's reach for a landed bomb");

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

// Same without the range test: a ranged dps or healer carrying nothing, while he's engaged
Unit* GetGormokForStompCaster(PlayerbotAI* botAI);

// Where a Fire Bomb dodge breaks ties: Gormok for melee dps and snobold carriers, else his victim.
// False while he isn't engaged or has no victim.
bool GetFireBombDodgeAnchor(PlayerbotAI* botAI, Position& anchor);

// Fire Bombs on this bot's instance, split by whether their impact is still to come. Landed ones keep
// pulsing until they despawn. Writes the 66317 haz circle once per bomb.
void GetFireBombs(PlayerbotAI* botAI, std::vector<Position>& young, std::vector<Position>& old);

// Alive, inside FIRE_BOMB_TRIGGER of a bomb still to land, and not being hit by an engaged beast:
// moving a beast's victim drags the beast and its melee along.
bool IsInFireBombImpact(PlayerbotAI* botAI);

}

#endif
