/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCHELPERS_SVALNA_H
#define PLAYERBOTS_ICCHELPERS_SVALNA_H

#include "Define.h"
#include "Position.h"

class Creature;
class Item;
class PlayerbotAI;
class Unit;

constexpr uint32 NPC_SS_SVALNA = 37126;
constexpr uint32 NPC_SS_ARNATH_UNDEAD = 37491;
constexpr uint32 NPC_SS_BRANDON_UNDEAD = 37493;
constexpr uint32 NPC_SS_GRONDEL_UNDEAD = 37494;
constexpr uint32 NPC_SS_RUPERT_UNDEAD = 37495;

constexpr uint32 NPC_SS_BATTLE_MAIDEN = 37132;
constexpr uint32 NPC_SS_DEATHBRINGER = 38125;
constexpr uint32 NPC_SS_FROSTBINDER = 37127;
constexpr uint32 NPC_SS_HUNTRESS = 37134;
constexpr uint32 NPC_SS_WARLORD = 37133;
constexpr uint32 NPC_SS_CORRUPTED_YMIRJAR = 38184;
constexpr uint32 NPC_SS_WARHAWK = 38154;

constexpr uint32 SS_DATA_SVALNA_GUID = 9;
constexpr uint32 SS_DATA_CAPTAIN_ARNATH_GUID = 31;
constexpr uint8 SS_CAPTAINS = 4;

constexpr uint32 SPELL_SS_IMPALING_SPEAR = 71443;
constexpr uint32 SPELL_SS_CAPTAIN_DIVINE_SHIELD = 71550;
constexpr uint32 SPELL_SS_DOMINATE_MIND = 14515;
constexpr uint32 SPELL_SS_SUMMON_UNDEAD = 69516;
constexpr uint32 SPELL_SS_WHIRLWIND = 41056;
constexpr uint32 SPELL_SS_FROZEN_ORB = 71274;
constexpr uint32 SPELL_SS_ARNATH_FLASH_HEAL = 71782;
constexpr uint32 SPELL_SS_EMPOWERED_SHADOW_BOLT = 69528;

constexpr float SS_TANK_DEFENSIVE_PCT = 40.0f;
constexpr float SS_TANK_LAST_RESORT_PCT = 25.0f;

bool IsSsLive(PlayerbotAI* botAI);
Creature* GetSsSvalna(PlayerbotAI* botAI);
bool IsSsShielded(PlayerbotAI* botAI, Unit* svalna);

Unit* GetSsTarget(PlayerbotAI* botAI);
Unit* GetSsCaptainToTaunt(PlayerbotAI* botAI);

Creature* GetSsSpearToPull(PlayerbotAI* botAI);
Item* GetSsSpearToThrow(PlayerbotAI* botAI);

Unit* GetSsInterrupt(PlayerbotAI* botAI, char const*& spell);

uint32 GetSsTankDefensive(PlayerbotAI* botAI, char const* noteKind);
bool IsSsTankDefensiveHeld(PlayerbotAI* botAI);

void TickSsEncounter(PlayerbotAI* botAI);

bool IsSsGauntletLive(PlayerbotAI* botAI);
Unit* GetSsGauntletTarget(PlayerbotAI* botAI);
Unit* GetSsGauntletTauntTarget(PlayerbotAI* botAI);
Unit* GetSsGauntletInterrupt(PlayerbotAI* botAI, char const*& spell);
bool IsSsGauntletInDanger(PlayerbotAI* botAI);
bool FindSsGauntletDodgeSpot(PlayerbotAI* botAI, Position& spot);
bool IsSsGauntletReachHeld(PlayerbotAI* botAI);
uint32 GetSsGauntletTankDefensive(PlayerbotAI* botAI, char const* noteKind);
bool IsSsGauntletTankDefensiveHeld(PlayerbotAI* botAI);
void TickSsGauntletEncounter(PlayerbotAI* botAI);

#endif
