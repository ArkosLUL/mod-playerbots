/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCTriggers_Dogs.h"

#include "Creature.h"
#include "Playerbots.h"
#include "RaidDefensive.h"

bool IccDogsTankDefensiveTrigger::IsActive() { return GetDogsTankDefensive(botAI, "dogs.defensive") != 0; }

bool IccDogsDecimateCoverTrigger::IsActive() { return GetDogsDecimateCover(botAI) != RaidCover::None; }

bool IccDogsSwapTrigger::IsActive() { return IsDogsSwapTaunter(botAI); }

bool IccDogsYieldTrigger::IsActive()
{
    Creature* dog = GetDogsBoss(botAI);
    return dog && IsDogsYielding(botAI) && (bot->GetVictim() == dog || AI_VALUE(Unit*, "current target") == dog);
}
