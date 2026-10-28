/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCHELPERS_ROTFACE_H
#define PLAYERBOTS_ICCHELPERS_ROTFACE_H

#include <string>

#include "Define.h"
#include "Position.h"

class Creature;
class Player;
class PlayerbotAI;
class Unit;

// icecrown_citadel.h CreaturesIds. GetEntry reports these on every difficulty.
constexpr uint32 NPC_RF_ROTFACE = 36627;
constexpr uint32 NPC_RF_LITTLE_OOZE = 36897;
constexpr uint32 NPC_RF_BIG_OOZE = 36899;
constexpr uint32 NPC_RF_STICKY_OOZE = 37006;
constexpr uint32 NPC_RF_PUDDLE_STALKER = 37013;
constexpr uint32 NPC_RF_SPRAY_STALKER = 37986;
constexpr uint32 NPC_RF_EXPLOSION_STALKER = 38107;

// No difficulty row: one id on every difficulty
constexpr uint32 SPELL_RF_SLIME_SPRAY = 69508;
constexpr uint32 SPELL_RF_UNSTABLE_OOZE = 69558;
constexpr uint32 SPELL_RF_UNSTABLE_OOZE_EXPLOSION = 69839;
constexpr uint32 SPELL_RF_UNSTABLE_OOZE_SUICIDE = 71441;
// The flooded quadrant's ground stalkers carry one of the four warnings for 5 s, then the flood for 20 s
constexpr uint32 SPELL_RF_OOZE_FLOOD_WARNINGS[] = {69783, 69797, 69799, 69802};
constexpr uint32 SPELL_RF_OOZE_FLOOD_ACTIVE = 69788;
// Difficulty rows: remap at the call
constexpr uint32 SPELL_RF_MUTATED_INFECTION = 69674;
constexpr uint32 SPELL_RF_VILE_GAS = 69240;

// His spawn and the room's centre. The floor is flat at 360.385 out to 42 yd from it (navprobe --nav 0x09,
// 72 headings at 38, 40 and 42 yd), and every stand below sees it.
inline Position const ICC_RF_CENTER(4445.87f, 3137.31f, 360.385f);
constexpr float RF_ROOM_RADIUS = 41.0f;

// The main tank's spot, east of him. He has 8 yd of combat reach, so he only follows a tank more than
// 10.8 yd away: one off his spawn by RF_BOSS_DRIFT is dragged back by a tank RF_DRAG_DISTANCE past it.
constexpr float RF_TANK_SPOT_DISTANCE = 5.0f;
constexpr float RF_BOSS_DRIFT = 4.5f;
constexpr float RF_BOSS_SETTLED = 2.5f;
constexpr float RF_DRAG_DISTANCE = 9.0f;

// Spread slots on two rings round his spawn, every slot at least 9.5 yd from every other: Vile Gas hits
// 8 yd round its victim, plus both park tolerances. The inner ring stays inside 14.5 yd of him, where Vile
// Gas doesn't pick while enough players stand outside, and never floods (a flood stops 13.9 yd short). Its
// slots sit 30 degrees off the tank spot and the melee behind him, 8 yd from both.
constexpr float RF_INNER_RADIUS = 12.0f;
constexpr uint8 RF_INNER_SLOTS = 6;
constexpr float RF_INNER_OFFSET_DEG = 30.0f;
// 9.5 yd out from the inner ring, so a slot there clears every inner slot whatever its bearing
constexpr float RF_OUTER_RADIUS = 21.5f;
constexpr uint8 RF_OUTER_SLOTS = 13;
constexpr float RF_OUTER_OFFSET_DEG = 180.0f / RF_OUTER_SLOTS;
constexpr uint8 RF_SLOT_COUNT = RF_INNER_SLOTS + RF_OUTER_SLOTS;
// Behind him, for whoever finds no free slot
constexpr float RF_STACK_DISTANCE = 8.0f;

// The Big Ooze is kited on this ring, 14.5 yd out from the outer slots: its Radiating Ooze reaches 10 yd,
// and a chaser cuts a few yards inside its kiter's ring.
constexpr float RF_KITE_RADIUS = 36.0f;
constexpr uint8 RF_KITE_POINTS = 36;

// A stand parks within RF_STAND_PARK and walks back once pushed past RF_STAND_UNPARK
constexpr float RF_STAND_PARK = 0.75f;
constexpr float RF_STAND_UNPARK = 2.5f;
static_assert(RF_STAND_PARK < RF_STAND_UNPARK, "a walk has to end inside its own trigger");

// Hazard reach, centre to centre: every one is a creature's area or a player's on players
constexpr float RF_FLOOD_RADIUS = 25.0f;
constexpr float RF_STICKY_RADIUS = 6.0f;
constexpr float RF_MARK_RADIUS = 6.0f;
constexpr float RF_VILE_GAS_RADIUS = 8.0f;
constexpr float RF_RADIATING_RADIUS = 10.0f;
// Two oozes this close merge on the next half-second tick
constexpr float RF_MERGE_RADIUS = 8.0f;
// Slime Spray: 25 degrees on 10-man normal (spell_cone), the 24 degree target type elsewhere, at any range,
// and everyone within 2 yd of him whatever the bearing
constexpr float RF_SPRAY_ARC_DEG = 25.0f;
constexpr float RF_SPRAY_BOUNDARY = 2.0f;
// Unstable Ooze Explosion's missiles fly from the burst ooze at 5 yd/s to where each player stood
constexpr float RF_MARK_SPEED = 5.0f;

// A carrier this close to the ooze it brings its own to is merged once its infection goes
constexpr float RF_MERGE_TOUCH = 6.0f;
constexpr float RF_DISPEL_RANGE = 30.0f;
// A carrier this low is cured where it stands
constexpr float RF_DISPEL_EMERGENCY_PCT = 35.0f;

// The kiter holds a Big Ooze at the ooze spot below this many Unstable Ooze stacks, kites it from there
constexpr uint8 RF_KITE_FROM_STACKS = 3;
// A Big Ooze this strong in reach of its tank opens the tank's defensive window
constexpr uint8 RF_OOZE_DEFENSIVE_STACKS = 4;
// Any tank under RF_TANK_DEFENSIVE_PCT spends a button; under RF_TANK_LAST_RESORT_PCT its class buttons are
// free again
constexpr float RF_TANK_DEFENSIVE_PCT = 40.0f;
constexpr float RF_TANK_LAST_RESORT_PCT = 25.0f;

// Rotface is alive and in combat and the bot is in his room. His boss state moves only when Putricide
// reaches his balcony point, seconds into the pull, so nothing here waits for it.
bool IsRfLive(PlayerbotAI* botAI);
Creature* GetRfBoss(PlayerbotAI* botAI);

// Tanks and DPS: the kiter's Big Ooze, then a player's skull for all but the tank holding him, else Rotface.
// Notes rf.target.
Unit* GetRfTarget(PlayerbotAI* botAI);

// The kiter's Big Ooze it still has to take: not on it, in taunt range, taunt ready and the ooze not
// immune to it yet
Unit* GetRfOozeToTaunt(PlayerbotAI* botAI);
// The kiter's next leg on the kite ring while it kites, in `dir` (1 counter-clockwise, -1 clockwise)
bool GetRfKiteWalk(PlayerbotAI* botAI, Position& spot, int8& dir);
void CommitRfKiteDir(PlayerbotAI* botAI, int8 dir);
// The kite leg is still worth walking: dry and clear of the ooze
bool IsRfKiteSpotSafe(PlayerbotAI* botAI, Position const& spot);
bool IsRfKiter(PlayerbotAI* botAI);
bool IsRfKiteHeld(PlayerbotAI* botAI);

// A bot with Mutated Infection or a Little Ooze on it walks to the ooze it merges with: the Big Ooze, the
// other Little Ooze, or the ooze spot
bool GetRfCarrierWalk(PlayerbotAI* botAI, Position& spot);
bool IsRfCarrierHeld(PlayerbotAI* botAI);
// The carrier this bot cures now and the spell
Player* GetRfDispelTarget(PlayerbotAI* botAI, char const*& spell);

// The hazard the bot stands in, nullptr when none: spray, mark, sticky, flood, gas, ooze
char const* GetRfHazardKind(PlayerbotAI* botAI);
// A spot clear of every hazard, for the dodge. `interrupt` says the hit is close enough to drop a cast for.
bool FindRfDodgeSpot(PlayerbotAI* botAI, Position& spot, bool& interrupt, std::string& branch);
// A dodge spot in flight is still clear of every hazard
bool IsRfDodgeSpotClear(PlayerbotAI* botAI, Position const& spot);
// Explosion marks still in flight or Slime Spray on: generic movers would walk the bot back into them
bool IsRfHazardHeld(PlayerbotAI* botAI);

bool IsRfHoldingStand(PlayerbotAI* botAI);
bool GetRfStandWalk(PlayerbotAI* botAI, Position& spot);
// A stand, the kite or a carrier walk owns the bot's feet
bool IsRfMovementOwned(PlayerbotAI* botAI);

// The button this tank spends now, 0 for none: the kiter under a Big Ooze of RF_OOZE_DEFENSIVE_STACKS in
// reach, any tank under RF_TANK_DEFENSIVE_PCT
uint32 GetRfTankDefensive(PlayerbotAI* botAI, char const* noteKind);
bool IsRfTankDefensiveHeld(PlayerbotAI* botAI);

// A damage dealer with an ooze near it or its target holds its area attacks, which would pull the ooze
bool IsRfAreaDamageHeld(PlayerbotAI* botAI);

// The ooze scan, the kiter, the slots, the ooze spot, the dispel duties, the stand latch, the calls to a
// human tank, the reset after a wipe
void TickRfEncounter(PlayerbotAI* botAI);

#endif
