/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCHELPERS_SINDRAGOSAADDS_H
#define PLAYERBOTS_ICCHELPERS_SINDRAGOSAADDS_H

#include <string>

#include "Define.h"
#include "Position.h"

class Creature;
class PlayerbotAI;
class Unit;

constexpr uint32 NPC_SG_DRAKE_RIMEFANG = 37533;
constexpr uint32 NPC_SG_DRAKE_SPINESTALKER = 37534;
constexpr uint32 NPC_SG_DRAKE_ICY_BLAST = 38223;
constexpr uint32 SG_DRAKE_DATA_SPINESTALKER = 24;
constexpr uint32 SG_DRAKE_DATA_RIMEFANG = 25;

constexpr uint32 SPELL_SG_DRAKE_FROST_BREATH = 71386;
constexpr uint32 SPELL_SG_DRAKE_ICY_BLAST = 71376;
constexpr uint32 SPELL_SG_DRAKE_BELLOWING_ROAR = 36922;

inline Position const ICC_SG_RIMEFANG_LAND(4413.309f, 2456.421f, 203.385f);
inline Position const ICC_SG_RIMEFANG_TANK(4413.31f, 2446.42f, 203.38f);
inline Position const ICC_SG_RIMEFANG_MELEE(4405.31f, 2456.42f, 203.38f);
inline Position const ICC_SG_RIMEFANG_RANGED(4391.31f, 2456.42f, 203.38f);
inline Position const ICC_SG_SPINESTALKER_LAND(4418.895f, 2514.233f, 203.385f);
inline Position const ICC_SG_SPINESTALKER_TANK(4418.90f, 2524.23f, 203.38f);
inline Position const ICC_SG_SPINESTALKER_MELEE(4410.90f, 2514.23f, 203.38f);
inline Position const ICC_SG_SPINESTALKER_RANGED(4384.13f, 2526.89f, 203.38f);

constexpr float SG_DRAKE_STAND_PARK = 1.0f;
constexpr float SG_DRAKE_STAND_UNPARK = 2.5f;
static_assert(SG_DRAKE_STAND_PARK < SG_DRAKE_STAND_UNPARK, "a walk has to end inside its own trigger");

constexpr float SG_DRAKE_TANK_DEFENSIVE_PCT = 40.0f;
constexpr float SG_DRAKE_BREATH_DEFENSIVE_PCT = 60.0f;
constexpr float SG_DRAKE_TANK_LAST_RESORT_PCT = 25.0f;

constexpr uint32 NPC_SG_GAUNTLET_CONTROLLER = 37503;
constexpr uint32 NPC_SG_GAUNTLET_WARRIOR = 37228;
constexpr uint32 NPC_SG_GAUNTLET_SORCERESS = 37229;
constexpr uint32 NPC_SG_GAUNTLET_BROODLING = 37232;
constexpr uint32 NPC_SG_GAUNTLET_CHAMPION = 37501;
constexpr uint32 NPC_SG_GAUNTLET_WEBWEAVER = 37502;
constexpr uint32 NPC_SG_GAUNTLET_FROSTBLADE = 38199;
constexpr uint32 SPELL_SG_GAUNTLET_ICE_TOMB = 71330;
constexpr uint32 SPELL_SG_GAUNTLET_FROST_NOVA = 71320;
constexpr uint32 SPELL_SG_GAUNTLET_WEB = 71327;

Creature* GetSgDrake(PlayerbotAI* botAI);
bool IsSgDrakesLive(PlayerbotAI* botAI);

Unit* GetSgDrakeTarget(PlayerbotAI* botAI);
bool IsSgDrakeBraked(PlayerbotAI* botAI);
Creature* GetSgDrakeBrakeTarget(PlayerbotAI* botAI);
bool IsSgDrakeTauntHeld(PlayerbotAI* botAI);

bool IsSgDrakeInHazard(PlayerbotAI* botAI);
bool FindSgDrakeDodgeSpot(PlayerbotAI* botAI, Position& spot);

bool IsSgDrakeHoldingStand(PlayerbotAI* botAI);
bool GetSgDrakeStandWalk(PlayerbotAI* botAI, Position& spot);
bool IsSgDrakeMovementOwned(PlayerbotAI* botAI);

bool IsSgSpinestalkerFearWindow(PlayerbotAI* botAI);

uint32 GetSgDrakeTankDefensive(PlayerbotAI* botAI, char const* noteKind);
bool IsSgDrakeTankDefensiveHeld(PlayerbotAI* botAI);

void TickSgDrakesEncounter(PlayerbotAI* botAI);
uint8 IccSindragosaDrakesResistSchools(PlayerbotAI* botAI, uint8 previous);

bool IsSgGauntletLive(PlayerbotAI* botAI);
Unit* GetSgGauntletTarget(PlayerbotAI* botAI);
Unit* GetSgGauntletTauntTarget(PlayerbotAI* botAI);
bool IsSgGauntletInBlade(PlayerbotAI* botAI);
bool FindSgGauntletDodgeSpot(PlayerbotAI* botAI, Position& spot);
bool IsSgGauntletBladeReachHeld(PlayerbotAI* botAI);
Unit* GetSgGauntletKickTarget(PlayerbotAI* botAI, std::string& spell);
Unit* GetSgGauntletFreeTarget(PlayerbotAI* botAI, std::string& spell);

void TickSgGauntletEncounter(PlayerbotAI* botAI);

#endif
