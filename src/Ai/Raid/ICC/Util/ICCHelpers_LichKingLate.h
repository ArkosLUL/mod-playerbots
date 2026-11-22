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

void TickLkLate(PlayerbotAI* botAI);

Unit* GetLkLateTarget(PlayerbotAI* botAI, char const*& rule);
bool IsLkLateTargetHeld(PlayerbotAI* botAI);
bool IsLkLateMovementOwned(PlayerbotAI* botAI);
bool IsLkLateReachHeld(PlayerbotAI* botAI);

bool GetLkDefileWalk(PlayerbotAI* botAI, Position& spot);
bool GetLkVileWalk(PlayerbotAI* botAI, Position& spot);
bool GetLkFrostmourneWalk(PlayerbotAI* botAI, Position& spot);
bool GetLkStackWalk(PlayerbotAI* botAI, Position& spot);
bool IsLkDefileRun(PlayerbotAI* botAI);
bool IsLkFrostmourneUrgent(PlayerbotAI* botAI);

uint32 GetLkSoakGuard(PlayerbotAI* botAI);
bool IsLkTrapDue(PlayerbotAI* botAI);
void NoteLkTrapDropped(PlayerbotAI* botAI);

Unit* GetLkSiphonTauntTarget(PlayerbotAI* botAI);
void NoteLkSiphonTaunt(PlayerbotAI* botAI, Unit* valkyr);

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

LkHarvestDuty GetLkHarvestDuty(PlayerbotAI* botAI, Unit*& target, std::string& spell);

#endif
