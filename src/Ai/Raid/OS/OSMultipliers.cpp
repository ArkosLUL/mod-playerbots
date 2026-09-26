/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "OSMultipliers.h"
#include "Action.h"
#include "BurstCooldowns.h"
#include "OSHelpers.h"
#include "Playerbots.h"

using namespace OsHelpers;

float SartharionMainTankTargetMultiplier::GetValue(Action* action)
{
    SartharionSnapshot const& state = SartharionSnapshotFor(botAI);
    if (!state.encounterActive || !botAI->IsMainTank(bot))
        return 1.0f;

    Unit* target = action->GetTarget();
    return target && target != state.boss ? 0.0f : 1.0f;
}

float SartharionOffTankBossMultiplier::GetValue(Action* action)
{
    SartharionSnapshot const& state = SartharionSnapshotFor(botAI);
    if (!state.encounterActive || !botAI->IsAssistTank(bot))
        return 1.0f;

    Unit* target = action->GetTarget();
    return target && target == state.boss ? 0.0f : 1.0f;
}

float SartharionRearFlankMultiplier::GetValue(Action* action)
{
    SartharionSnapshot const& state = SartharionSnapshotFor(botAI);
    if (!state.encounterActive)
        return 1.0f;

    Unit* target = action->GetTarget();
    return target && (target == state.boss || IsDrakeEntry(target->GetEntry())) ? 0.0f : 1.0f;
}

float SartharionBossReachMultiplier::GetValue(Action* action)
{
    SartharionSnapshot const& state = SartharionSnapshotFor(botAI);
    if (!state.encounterActive || !state.boss)
        return 1.0f;

    if (!botAI->IsRanged(bot) && !botAI->IsHeal(bot) && !IsOffTank(bot))
        return 1.0f;

    return action->GetTarget() == state.boss ? 0.0f : 1.0f;
}

float SartharionBurstWindowMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    std::string const name = action->getName();
    if (!IsBurstCooldownAction(name) || IsManaReturnCooldown(bot, name))
        return 1.0f;

    uint32 const now = getMSTime();
    if (now != cachedAtMs || !cachedAtMs)
    {
        cachedAtMs = now;
        cachedValue = EvaluateWindow();
    }
    return cachedValue;
}

float SartharionBurstWindowMultiplier::EvaluateWindow()
{
    Unit* boss = GetSartharion(bot);
    if (!boss || !boss->IsInCombat())
        return 1.0f;

    // Hard interlock, not a tiebreaker: Gift of Twilight is a full-school damage immunity, so a
    // Bloodlust fired under it is thrown away entirely.
    if (SartharionDamageImmune(bot))
        return 0.0f;

    if (boss->HasAura(SpellId::SartharionBerserk) || boss->GetHealthPct() <= SARTHARION_ENRAGE_PCT)
        return 1.0f;

    return SartharionBurstWindowOpen(bot) ? 1.0f : 0.0f;
}
