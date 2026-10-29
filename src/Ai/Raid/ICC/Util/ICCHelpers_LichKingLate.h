/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCHELPERS_LICHKINGLATE_H
#define PLAYERBOTS_ICCHELPERS_LICHKINGLATE_H

#include <string>

#include "Define.h"
#include "Position.h"

class PlayerbotAI;
class Unit;

// Phases two and three and Frostmourne: the scan, the Defile runs, the stacks, the Val'kyr, Vile Spirit and
// Frostmourne duties, the reset after a wipe. Runs after TickLkEncounter.
void TickLkLate(PlayerbotAI* botAI);

// This stem's pick for the bot, with its rule, or null: a Val'kyr, a siphoner, a Vile Spirit, the Spirit
// Warden or a Wicked Spirit. In combat and attackable whenever it answers.
Unit* GetLkLateTarget(PlayerbotAI* botAI, char const*& rule);
// A tank or DPS bot inside Frostmourne this stem hands nothing
bool IsLkLateTargetHeld(PlayerbotAI* botAI);
// A walk of this stem, a parked stack spot, or Frostmourne owns the bot's feet
bool IsLkLateMovementOwned(PlayerbotAI* botAI);

// Each walk answers true with its spot while it is the bot's current walk and the bot has not parked
bool GetLkDefileWalk(PlayerbotAI* botAI, Position& spot);
bool GetLkVileWalk(PlayerbotAI* botAI, Position& spot);
bool GetLkFrostmourneWalk(PlayerbotAI* botAI, Position& spot);
bool GetLkStackWalk(PlayerbotAI* botAI, Position& spot);
// The Defile target's own run, which spends Nitro Boosts
bool IsLkDefileRun(PlayerbotAI* botAI);
// The Frostmourne leader is stepping off a bomb, so the stack follows at once
bool IsLkFrostmourneUrgent(PlayerbotAI* botAI);

// The immunity or tank button the Vile Spirit soaker casts before it walks into a spirit, 0 for none
uint32 GetLkSoakGuard(PlayerbotAI* botAI);
// The trap hunter stands on its spot with Frost Trap ready
bool IsLkTrapDue(PlayerbotAI* botAI);
void NoteLkTrapDropped(PlayerbotAI* botAI);

// The siphoning Val'kyr the free tank taunts now, or null
Unit* GetLkSiphonTauntTarget(PlayerbotAI* botAI);
void NoteLkSiphonTaunt(PlayerbotAI* botAI, Unit* valkyr);

// The carrying Val'kyr this bot stuns or slows now, with the spell, or null
Unit* GetLkValkyrControl(PlayerbotAI* botAI, uint32& spellId, bool& stun);
void NoteLkValkyrControl(PlayerbotAI* botAI, Unit* valkyr, bool stun);

enum class LkHarvestDuty : uint8
{
    None,
    Warden,
    Kick,
    Heal,
    Dispel,
};

// A normal Harvest Soul victim's duty inside Frostmourne. Kick and Dispel come with the spell to cast,
// Heal with Terenas.
LkHarvestDuty GetLkHarvestDuty(PlayerbotAI* botAI, Unit*& target, std::string& spell);

#endif
