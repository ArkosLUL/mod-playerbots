/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCHELPERS_SVALNA_H
#define PLAYERBOTS_ICCHELPERS_SVALNA_H

#include "Define.h"

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

// icecrown_citadel.h DataTypes: Svalna's guid slot, then Arnath, Brandon, Grondel and Rupert from 31
constexpr uint32 SS_DATA_SVALNA_GUID = 9;
constexpr uint32 SS_DATA_CAPTAIN_ARNATH_GUID = 31;
constexpr uint8 SS_CAPTAINS = 4;

// No difficulty row: one id on every difficulty
constexpr uint32 SPELL_SS_IMPALING_SPEAR = 71443;
constexpr uint32 SPELL_SS_CAPTAIN_DIVINE_SHIELD = 71550;

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

// The scan, the spear and captain duties, the shield note, the reset after a wipe
void TickSsEncounter(PlayerbotAI* botAI);

#endif
