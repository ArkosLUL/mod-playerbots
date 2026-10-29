/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCTriggers_LichKing.h"

#include "Creature.h"
#include "Playerbots.h"
#include "Position.h"

bool IccLichKingQuakeTrigger::IsActive()
{
    Position spot;
    return GetLkQuakeWalk(botAI, spot);
}

bool IccLichKingShadowTrapTrigger::IsActive()
{
    Position spot;
    return GetLkTrapWalk(botAI, spot);
}

bool IccLichKingTankDefensiveTrigger::IsActive() { return GetLkTankDefensive(botAI, "lk.defensive") != 0; }

bool IccLichKingTauntTrigger::IsActive()
{
    uint32 aoeSpell = 0;
    return GetLkTauntTarget(botAI, aoeSpell) != nullptr;
}

bool IccLichKingPlagueCureTrigger::IsActive()
{
    uint32 spellId = 0;
    return GetLkCureTarget(botAI, spellId) != nullptr;
}

bool IccLichKingPlagueWalkTrigger::IsActive()
{
    Position spot;
    return GetLkPlagueWalk(botAI, spot);
}

bool IccLichKingTranquilizeTrigger::IsActive() { return GetLkTranquilizeTarget(botAI) != nullptr; }

bool IccLichKingStunTrigger::IsActive()
{
    uint32 spellId = 0;
    return GetLkStunTarget(botAI, spellId) != nullptr;
}

bool IccLichKingSilenceDispelTrigger::IsActive()
{
    uint32 spellId = 0;
    return GetLkSilenceDispelTarget(botAI, spellId) != nullptr;
}

bool IccLichKingBrakeTrigger::IsActive()
{
    Creature* boss = IsLkBraked(botAI) ? GetLkBoss(botAI) : nullptr;
    return boss && (bot->GetVictim() == boss || AI_VALUE(Unit*, "current target") == boss);
}

bool IccLichKingStandTrigger::IsActive()
{
    Position spot;
    return GetLkStandWalk(botAI, spot);
}
