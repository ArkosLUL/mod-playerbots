/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCTriggers_BloodPrinces.h"

#include "Creature.h"
#include "ICCSeams.h"
#include "Playerbots.h"
#include "Position.h"
#include "RaidDefensive.h"
#include "RaidDirectTargeting.h"
#include "Spell.h"

bool IccBpcDodgeTrigger::IsActive() { return IsBpcInVortex(botAI); }

bool IccBpcTankDefensiveTrigger::IsActive() { return GetBpcTankDefensive(botAI, "bpc.defensive") != 0; }

bool IccBpcLanceDefensiveTrigger::IsActive()
{
    RaidPredictedHit hit;
    return GetBpcLanceHit(botAI, hit) && static_cast<bool>(GetRaidPersonalDefensive(botAI, ICC_DEFENSIVE_SCOPE, &hit));
}

bool IccBpcFlameDefensiveTrigger::IsActive() { return GetBpcFlameDefensive(botAI) != 0; }

bool IccBpcDispelTrigger::IsActive()
{
    char const* spell = nullptr;
    return GetBpcDispelTarget(botAI, spell) != nullptr;
}

bool IccBpcBrakeTrigger::IsActive()
{
    Creature* prince = GetBpcBrakeTarget(botAI);
    if (!prince)
        return false;

    Spell* autoRepeat = bot->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL);
    return bot->GetVictim() == prince || AI_VALUE(Unit*, "current target") == prince ||
           (autoRepeat && autoRepeat->m_targets.GetUnitTarget() == prince);
}

bool IccBpcBallTrigger::IsActive()
{
    Position spot;
    return GetBpcBallWalk(botAI, spot);
}

bool IccBpcPrinceTauntTrigger::IsActive() { return GetBpcPrinceToTaunt(botAI) != nullptr; }

bool IccBpcNucleusTrigger::IsActive()
{
    Position spot;
    return GetBpcNucleusToTaunt(botAI) || GetBpcNucleusWalk(botAI, spot);
}

bool IccBpcSoakTrigger::IsActive()
{
    Position spot;
    return GetBpcSoakWalk(botAI, spot);
}

bool IccBpcTargetTrigger::IsActive()
{
    Unit* pick = GetBpcTarget(botAI);
    if (pick && AI_VALUE(Unit*, "current target") != pick)
        return true;

    Unit* petPick = GetBpcPetTarget(botAI);
    return petPick && RaidPetOffPick(botAI, petPick);
}

bool IccBpcStandTrigger::IsActive()
{
    Position spot;
    return GetBpcStandWalk(botAI, spot);
}
