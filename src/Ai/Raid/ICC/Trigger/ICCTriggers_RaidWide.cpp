/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCTriggers_RaidWide.h"

#include "Playerbots.h"
#include "RaidDirectTargeting.h"

bool IccDirectTargetTrigger::IsActive()
{
    Unit* pick = _resolve ? _resolve(botAI) : nullptr;
    return pick && (AI_VALUE(Unit*, "current target") != pick || RaidPetOffPick(botAI, pick));
}

bool IccOpenerSwingTrigger::IsActive()
{
    return !IccOpenerBosses(botAI).empty() && RaidOpenerSwingTrigger::IsActive();
}

bool IccResistAuraTrigger::IsActive() { return IccResistAuraWanted(botAI) && RaidResistAuraTrigger::IsActive(); }
