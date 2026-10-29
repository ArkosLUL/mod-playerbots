/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCTriggers_SindragosaAdds.h"

#include "Creature.h"
#include "Playerbots.h"
#include "Position.h"

bool IccSgDrakeDodgeTrigger::IsActive() { return IsSgDrakeInHazard(botAI); }

bool IccSgDrakeTankDefensiveTrigger::IsActive() { return GetSgDrakeTankDefensive(botAI, "sg.drake.defensive") != 0; }

bool IccSgDrakeAntiFearTrigger::FearWindowActive() { return IsSgSpinestalkerFearWindow(botAI); }

bool IccSgDrakeBrakeTrigger::IsActive()
{
    Creature* drake = GetSgDrakeBrakeTarget(botAI);
    return drake && (bot->GetVictim() == drake || AI_VALUE(Unit*, "current target") == drake);
}

bool IccSgDrakeStandTrigger::IsActive()
{
    Position spot;
    return GetSgDrakeStandWalk(botAI, spot);
}

bool IccSgGauntletDodgeTrigger::IsActive() { return IsSgGauntletInBlade(botAI); }

bool IccSgGauntletTauntTrigger::IsActive() { return GetSgGauntletTauntTarget(botAI) != nullptr; }
