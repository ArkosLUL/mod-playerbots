/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "OnyMultipliers.h"

#include "Action.h"
#include "BurstCooldowns.h"
#include "EncounterHelpers.h"
#include "GenericSpellActions.h"
#include "OnyHelpers.h"
#include "Playerbots.h"

using namespace OnyHelpers;

namespace
{
// Area threat from further out than this cannot reach her.
constexpr float AREA_THREAT_REACH = 30.0f;
}  // namespace

float OnyxiaThreatHoldMultiplier::GetValue(Action* action)
{
    if (!action || dynamic_cast<CastHealingSpellAction*>(action) || !ThreatHoldLatch(bot))
        return 1.0f;

    Unit* boss = OnyxiaSnapshotFor(botAI).boss;
    if (!boss)
        return 1.0f;

    Unit* target = action->GetTarget();
    if (target == boss || (!target && AI_VALUE(Unit*, "current target") == boss))
        return 0.0f;

    if (EncounterHelpers::IsAoeThreatAction(bot, action) && bot->GetExactDist2d(boss) <= AREA_THREAT_REACH)
        return 0.0f;

    return 1.0f;
}

float OnyxiaTauntGuardMultiplier::GetValue(Action* action)
{
    Unit* boss = OnyxiaSnapshotFor(botAI).boss;
    if (!action || !boss)
        return 1.0f;

    Player* mainTank = EffectiveMainTank(bot, boss);
    if (!mainTank || mainTank == bot)
        return 1.0f;

    // An area taunt has no target of its own and takes her as readily as anything else in reach.
    Unit* target = action->GetTarget();
    if (target == boss || (!target && bot->GetExactDist2d(boss) <= AREA_THREAT_REACH))
        return 0.0f;

    return 1.0f;
}

float OnyxiaBurstWindowMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    std::string const name = action->getName();
    if (!IsBurstCooldownAction(name) || IsManaReturnCooldown(bot, name))
        return 1.0f;

    bool const lust = name == "bloodlust" || name == "heroism";
    return BurstAllowed(botAI, lust) ? 1.0f : 0.0f;
}

float OnyxiaEggGuardMultiplier::GetValue(Action* action)
{
    Unit* target = action ? action->GetTarget() : nullptr;
    if (!target || target == bot)
        return 1.0f;

    return NearLiveEgg2d(bot, target->GetPositionX(), target->GetPositionY(), ONYXIA_WHELP_EGG_EXCLUSION) ? 0.0f
                                                                                                          : 1.0f;
}

bool OnyxiaAntiFearTotemGuardMultiplier::FearWindowActive() { return OnyHelpers::FearWindowActive(botAI); }
