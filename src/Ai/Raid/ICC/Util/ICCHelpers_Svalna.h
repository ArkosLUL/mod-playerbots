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

// icecrown_citadel.h CreatureIds: the captains Svalna raises keep their instance guid slots under these
constexpr uint32 NPC_SS_SVALNA = 37126;
constexpr uint32 NPC_SS_ARNATH_UNDEAD = 37491;
constexpr uint32 NPC_SS_BRANDON_UNDEAD = 37493;
constexpr uint32 NPC_SS_GRONDEL_UNDEAD = 37494;
constexpr uint32 NPC_SS_RUPERT_UNDEAD = 37495;

// icecrown_citadel.h CreatureIds: the Ymirjar of her gauntlet, what a Deathbringer raises and a 25-man
// Huntress's pet
constexpr uint32 NPC_SS_BATTLE_MAIDEN = 37132;
constexpr uint32 NPC_SS_DEATHBRINGER = 38125;
constexpr uint32 NPC_SS_FROSTBINDER = 37127;
constexpr uint32 NPC_SS_HUNTRESS = 37134;
constexpr uint32 NPC_SS_WARLORD = 37133;
constexpr uint32 NPC_SS_CORRUPTED_YMIRJAR = 38184;
constexpr uint32 NPC_SS_WARHAWK = 38154;

// icecrown_citadel.h DataTypes: Svalna's guid slot, then Arnath, Brandon, Grondel and Rupert from 31
constexpr uint32 SS_DATA_SVALNA_GUID = 9;
constexpr uint32 SS_DATA_CAPTAIN_ARNATH_GUID = 31;
constexpr uint8 SS_CAPTAINS = 4;

// No difficulty row: one id on every difficulty
constexpr uint32 SPELL_SS_IMPALING_SPEAR = 71443;
constexpr uint32 SPELL_SS_CAPTAIN_DIVINE_SHIELD = 71550;
constexpr uint32 SPELL_SS_DOMINATE_MIND = 14515;
constexpr uint32 SPELL_SS_SUMMON_UNDEAD = 69516;
constexpr uint32 SPELL_SS_WHIRLWIND = 41056;
constexpr uint32 SPELL_SS_FROZEN_ORB = 71274;
// Difficulty rows: remap at the call
constexpr uint32 SPELL_SS_ARNATH_FLASH_HEAL = 71782;
constexpr uint32 SPELL_SS_EMPOWERED_SHADOW_BOLT = 69528;

// A tank something of the encounter hits spends a button under this, and the class nodes keep theirs
// until the last resort
constexpr float SS_TANK_DEFENSIVE_PCT = 40.0f;
constexpr float SS_TANK_LAST_RESORT_PCT = 25.0f;

// Svalna or a captain she raised fights within reach of the bot. The captains rise and charge before she
// lands, so this covers the seconds before her boss state moves.
bool IsSsLive(PlayerbotAI* botAI);
Creature* GetSsSvalna(PlayerbotAI* botAI);
// Aether Shield is up from each Impaling Spear until a Hurl Spear breaks it. Publishes ss.shield.
bool IsSsShielded(PlayerbotAI* botAI, Unit* svalna);

// Tanks and DPS: a player's skull, a tank's own captain, the captains Arnath, Rupert, Grondel, Brandon
// (not under Divine Shield), then Svalna while unshielded. Notes ss.target.
Unit* GetSsTarget(PlayerbotAI* botAI);
// This tank's captain, held by somebody else, its taunt ready
Unit* GetSsCaptainToTaunt(PlayerbotAI* botAI);

// The Impaling Spear this bot was elected to pull out, its victim stunned under 10% health a second
Creature* GetSsSpearToPull(PlayerbotAI* botAI);
// The Infernal Spear this bot throws into the shield now, null while it holds none, is not the one elected
// to throw, or she is unshielded. Notes ss.hurl.
Item* GetSsSpearToThrow(PlayerbotAI* botAI);

// Arnath casting Dominate Mind (25-man) or Flash Heal, when this bot is the one elected to kick it, and
// the kick. Notes ss.interrupt.
Unit* GetSsInterrupt(PlayerbotAI* botAI, char const*& spell);

// The button this tank spends now, 0 for none: a captain hits it under SS_TANK_DEFENSIVE_PCT
uint32 GetSsTankDefensive(PlayerbotAI* botAI, char const* noteKind);
bool IsSsTankDefensiveHeld(PlayerbotAI* botAI);

// The scan, the spear, kick and captain duties, the shield note, the human tank call, the reset after a
// wipe
void TickSsEncounter(PlayerbotAI* botAI);

// The gauntlet: a Ymirjar of the hall, or what a Deathbringer raised, fights the bot's group
bool IsSsGauntletLive(PlayerbotAI* botAI);
// A player's skull, a tank's rescue of a melee add hitting a non-tank, then by role: ranged on
// Deathbringers, melee on Frostbinders, then the other caster, Huntresses, Battle-Maidens, Warlords and
// the summons, nearest the bot in a tier. Notes ss.gauntlet.target.
Unit* GetSsGauntletTarget(PlayerbotAI* botAI);
// A melee add on a non-tank this tank is the nearest to with its taunt ready. Notes ss.gauntlet.taunt.
Unit* GetSsGauntletTauntTarget(PlayerbotAI* botAI);
// A Deathbringer casting Summon Undead or Empowered Shadow Bolt this bot is elected to kick, and the
// kick. Notes ss.gauntlet.interrupt.
Unit* GetSsGauntletInterrupt(PlayerbotAI* botAI, char const*& spell);
// A Warlord's Whirlwind or a Frostbinder's Frozen Orb is about to hit the bot. Notes ss.gauntlet.dodge.
bool IsSsGauntletInDanger(PlayerbotAI* botAI);
bool FindSsGauntletDodgeSpot(PlayerbotAI* botAI, Position& spot);
// A melee bot's target stands inside one, so reaching it walks the bot back in
bool IsSsGauntletReachHeld(PlayerbotAI* botAI);
// The button this tank spends now, 0 for none: a Ymirjar hits it under SS_TANK_DEFENSIVE_PCT
uint32 GetSsGauntletTankDefensive(PlayerbotAI* botAI, char const* noteKind);
bool IsSsGauntletTankDefensiveHeld(PlayerbotAI* botAI);
// The trace, the scan, the kick duties, the human tank call, the reset once the hall goes quiet
void TickSsGauntletEncounter(PlayerbotAI* botAI);

#endif
