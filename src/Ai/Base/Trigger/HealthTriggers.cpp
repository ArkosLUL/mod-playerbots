/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "HealthTriggers.h"

#include "AoeHealValues.h"
#include "AttackersValue.h"
#include "Playerbots.h"

bool HealthInRangeTrigger::IsActive()
{
    return ValueInRangeTrigger::IsActive() && !AI_VALUE2(bool, "dead", GetTargetName());
}

float HealthInRangeTrigger::GetValue() { return AI_VALUE2(uint8, "health", GetTargetName()); }

bool PartyMemberDeadTrigger::IsActive() { return GetTarget(); }

bool CombatPartyMemberDeadTrigger::IsActive()
{
    if (!GetTarget())
        return false;

    // Conserve battle rez (Druid Rebirth) for boss fights unless the server opts out.
    if (sPlayerbotAIConfig.battleRezBossOnly && !AttackersValue::IsInBossFight(botAI))
        return false;

    return true;
}

bool DeadTrigger::IsActive() { return AI_VALUE2(bool, "dead", GetTargetName()); }

bool AoeHealTrigger::IsActive() { return AI_VALUE2(uint8, "aoe heal", type) >= count; }

bool HealerLowManaTrigger::IsActive()
{
    Unit* target = GetTarget();
    if (!target)
        return false;

    return target->GetPowerPct(POWER_MANA) < sPlayerbotAIConfig.lowMana;
}

bool AoeInGroupTrigger::IsActive()
{
    int32 member = CountHealableGroupMembers(bot);
    if (member < 3)
        return false;
    int threshold = member * 0.5;
    if (member <= 5)
        threshold = 3;
    else if (member <= 10)
        threshold = std::min(threshold, 4);
    else if (member <= 25)
        threshold = std::min(threshold, 6);
    else
        threshold = std::min(threshold, 8);

    return AI_VALUE2(uint8, "aoe heal", type) >= threshold;
}
