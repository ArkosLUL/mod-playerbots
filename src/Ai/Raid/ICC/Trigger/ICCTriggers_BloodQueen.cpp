/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCTriggers_BloodQueen.h"

#include "Creature.h"
#include "ICCSeams.h"
#include "Playerbots.h"
#include "Position.h"
#include "RaidDefensive.h"

bool IccBqlDodgeTrigger::IsActive() { return IsBqlInShadow(botAI); }

bool IccBqlBiteTrigger::IsActive() { return GetBqlBiteTarget(botAI) != nullptr; }

bool IccBqlShadowsTrigger::IsActive()
{
    Position spot;
    return GetBqlShadowsWalk(botAI, spot);
}

bool IccBqlPactTrigger::IsActive()
{
    Position spot;
    return GetBqlPactWalk(botAI, spot);
}

bool IccBqlWhirlDefensiveTrigger::IsActive()
{
    RaidPredictedHit hit;
    return !IsBqlBiting(botAI) && GetBqlWhirlHit(botAI, hit) &&
           static_cast<bool>(GetRaidPersonalDefensive(botAI, ICC_DEFENSIVE_SCOPE, &hit));
}

bool IccBqlWhirlCoverTrigger::IsActive() { return GetBqlWhirlCover(botAI) != RaidCover::None; }

bool IccBqlTankDefensiveTrigger::IsActive() { return GetBqlTankDefensive(botAI, "bql.defensive") != 0; }

bool IccBqlAntiFearTrigger::IsActive() { return IsBqlSelfWardDue(botAI) || RaidAntiFearTrigger::IsActive(); }

bool IccBqlAntiFearTrigger::FearWindowActive() { return IsBqlFearWindow(botAI); }

bool IccBqlMassDispelTrigger::IsActive() { return GetBqlMassDispelTarget(botAI) != nullptr; }

bool IccBqlFrenzyCcTrigger::IsActive()
{
    uint32 spellId = 0;
    return GetBqlFrenzyCc(botAI, spellId) != nullptr;
}

bool IccBqlTauntTrigger::IsActive() { return GetBqlTauntTarget(botAI) != nullptr; }

bool IccBqlBrakeTrigger::IsActive()
{
    Creature* queen = IsBqlBraked(botAI) ? GetBqlQueen(botAI) : nullptr;
    return queen && (bot->GetVictim() == queen || AI_VALUE(Unit*, "current target") == queen);
}

bool IccBqlStandTrigger::IsActive()
{
    Position spot;
    return GetBqlStandWalk(botAI, spot);
}
