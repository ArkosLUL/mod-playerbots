/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCHELPERS_SINDRAGOSAADDS_H
#define PLAYERBOTS_ICCHELPERS_SINDRAGOSAADDS_H

#include "Define.h"
#include "Position.h"

class Creature;
class PlayerbotAI;
class Unit;

// icecrown_citadel.h CreaturesIds. GetEntry reports these on every difficulty.
constexpr uint32 NPC_SG_DRAKE_RIMEFANG = 37533;
constexpr uint32 NPC_SG_DRAKE_SPINESTALKER = 37534;
// Where an Icy Blast lands, for 40 s
constexpr uint32 NPC_SG_DRAKE_ICY_BLAST = 38223;
// icecrown_citadel.h DataTypes: the drakes' guid slots
constexpr uint32 SG_DRAKE_DATA_SPINESTALKER = 24;
constexpr uint32 SG_DRAKE_DATA_RIMEFANG = 25;

// No difficulty row: one id on every difficulty
constexpr uint32 SPELL_SG_DRAKE_FROST_BREATH = 71386;
constexpr uint32 SPELL_SG_DRAKE_ICY_BLAST = 71376;
constexpr uint32 SPELL_SG_DRAKE_BELLOWING_ROAR = 36922;

// Their landing spots, and the stands round them on the floor at z 203.38, navprobed --nav 0x09. Each drake
// turns to its tank 10 yd off the landing spot, so its front points away from the others.
inline Position const ICC_SG_RIMEFANG_LAND(4413.309f, 2456.421f, 203.385f);
inline Position const ICC_SG_RIMEFANG_TANK(4413.31f, 2446.42f, 203.38f);
inline Position const ICC_SG_RIMEFANG_MELEE(4405.31f, 2456.42f, 203.38f);
inline Position const ICC_SG_RIMEFANG_RANGED(4391.31f, 2456.42f, 203.38f);
// Tail Sweep is a back cone: 24 degrees on 25-man, all but his front 82 on 10-man, so melee take his flank.
// The ranged spot is 37 yd out of Bellowing Roar's 35.
inline Position const ICC_SG_SPINESTALKER_LAND(4418.895f, 2514.233f, 203.385f);
inline Position const ICC_SG_SPINESTALKER_TANK(4418.90f, 2524.23f, 203.38f);
inline Position const ICC_SG_SPINESTALKER_MELEE(4410.90f, 2514.23f, 203.38f);
inline Position const ICC_SG_SPINESTALKER_RANGED(4384.13f, 2526.89f, 203.38f);

// A stand parks within SG_DRAKE_STAND_PARK and walks back once pushed past SG_DRAKE_STAND_UNPARK
constexpr float SG_DRAKE_STAND_PARK = 1.0f;
constexpr float SG_DRAKE_STAND_UNPARK = 2.5f;
static_assert(SG_DRAKE_STAND_PARK < SG_DRAKE_STAND_UNPARK, "a walk has to end inside its own trigger");

// Any drake tank under SG_DRAKE_TANK_DEFENSIVE_PCT spends a button, Rimefang's under
// SG_DRAKE_BREATH_DEFENSIVE_PCT on each Frost Breath. Under SG_DRAKE_TANK_LAST_RESORT_PCT its class buttons
// are free again.
constexpr float SG_DRAKE_TANK_DEFENSIVE_PCT = 40.0f;
constexpr float SG_DRAKE_BREATH_DEFENSIVE_PCT = 60.0f;
constexpr float SG_DRAKE_TANK_LAST_RESORT_PCT = 25.0f;

// icecrown_citadel.h: the gauntlet's controller, whose entry is also its guid slot
constexpr uint32 NPC_SG_GAUNTLET_CONTROLLER = 37503;
constexpr uint32 NPC_SG_GAUNTLET_WARRIOR = 37228;
constexpr uint32 NPC_SG_GAUNTLET_SORCERESS = 37229;
constexpr uint32 NPC_SG_GAUNTLET_BROODLING = 37232;
constexpr uint32 NPC_SG_GAUNTLET_CHAMPION = 37501;
constexpr uint32 NPC_SG_GAUNTLET_WEBWEAVER = 37502;
// A Frostwarden Warrior's sword, whirling 70305 within 12 yd every second for most of its 14 s
constexpr uint32 NPC_SG_GAUNTLET_FROSTBLADE = 38199;

// A drake in combat with the bot's group, from the encounter gate
Creature* GetSgDrake(PlayerbotAI* botAI);
bool IsSgDrakesLive(PlayerbotAI* botAI);

// Tanks and DPS: a player's skull on a drake, a tank's own drake, else the first fighting drake, Rimefang
// first. Nothing for a braked bot. Notes sg.drake.target.
Unit* GetSgDrakeTarget(PlayerbotAI* botAI);
// Past the threat brake on its drake: a tank holding none, or anyone not holding Rimefang while he flies
bool IsSgDrakeBraked(PlayerbotAI* botAI);
// The drake a braked bot keeps off
Creature* GetSgDrakeBrakeTarget(PlayerbotAI* botAI);
// A tank holding no drake, whose taunt would take one off its holder
bool IsSgDrakeTauntHeld(PlayerbotAI* botAI);

// Standing within reach of an Icy Blast puddle, of a missile in flight or of the spot Rimefang is casting
// one at. Notes sg.drake.dodge.
bool IsSgDrakeInHazard(PlayerbotAI* botAI);
bool FindSgDrakeDodgeSpot(PlayerbotAI* botAI, Position& spot);

// The bot's latched stand by its drake while that drake is on the ground
bool IsSgDrakeHoldingStand(PlayerbotAI* botAI);
bool GetSgDrakeStandWalk(PlayerbotAI* botAI, Position& spot);
// A stand or a dodge owns the bot's feet
bool IsSgDrakeMovementOwned(PlayerbotAI* botAI);

// From 8 s before Spinestalker's next Bellowing Roar can come until he has cast it
bool IsSgSpinestalkerFearWindow(PlayerbotAI* botAI);

// The button this drake tank spends now, 0 for none
uint32 GetSgDrakeTankDefensive(PlayerbotAI* botAI, char const* noteKind);
bool IsSgDrakeTankDefensiveHeld(PlayerbotAI* botAI);

// The scan, the tank roster, the roar clock, the brake, the stand latch, the calls to a human tank, the
// pull mark, the reset after the fight
void TickSgDrakesEncounter(PlayerbotAI* botAI);
// Frost while the drakes are up: Rimefang's Frost Aura reaches the whole raid within 100 yd
uint8 IccSindragosaDrakesResistSchools(PlayerbotAI* botAI, uint8 previous);

// The gauntlet is in progress and the bot within reach of its controller
bool IsSgGauntletLive(PlayerbotAI* botAI);
// Tanks and DPS: a player's skull, a tank's Champion or Warrior on a non-tank, else the kill order
// Sorceress, Webweaver, Champion, Warrior, Broodling, nearest first. Notes sg.gauntlet.target.
Unit* GetSgGauntletTarget(PlayerbotAI* botAI);
// That Champion or Warrior, for the tank nearest it with its taunt ready. Notes sg.gauntlet.taunt.
Unit* GetSgGauntletTauntTarget(PlayerbotAI* botAI);
// Inside a Frostblade's whirl. Notes sg.gauntlet.dodge.
bool IsSgGauntletInBlade(PlayerbotAI* botAI);
bool FindSgGauntletDodgeSpot(PlayerbotAI* botAI, Position& spot);
// A melee bot whose target stands in a Frostblade's whirl: reaching it would walk the bot in
bool IsSgGauntletBladeReachHeld(PlayerbotAI* botAI);

// The scan, the reset once the gauntlet stops
void TickSgGauntletEncounter(PlayerbotAI* botAI);

#endif
