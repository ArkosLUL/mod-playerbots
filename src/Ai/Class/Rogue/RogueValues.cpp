/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "RogueValues.h"

#include <ctime>

#include "Group.h"
#include "Playerbots.h"
#include "ThreatManager.h"

bool TricksOfTheTradeTargetValue::TankNeedsRedirect(Unit* mainTank)
{
    // Only maintained while the 'wait for attack' strategy runs, so 0 means "no opener info" rather
    // than "combat just started".
    time_t combatStartTime = AI_VALUE(time_t, "combat start time");
    if (combatStartTime && uint32(time(nullptr) - combatStartTime) <= OPENER_SECONDS)
        return true;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return false;

    ThreatManager& mgr = target->GetThreatMgr();
    float tankThreat = mgr.GetThreat(mainTank);
    return tankThreat == 0.0f || mgr.GetThreat(bot) > tankThreat * 0.5f;
}

Unit* TricksOfTheTradeTargetValue::Calculate()
{
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    // Tricks reaches 20 yd, and CanCastSpell lets SPELL_FAILED_OUT_OF_RANGE through as castable, so
    // without the same range gate the fallback below uses, a tank standing further out than that
    // eats the action every tick on a cast that can never land.
    Unit* mainTank = AI_VALUE(Unit*, "main tank");
    if (mainTank && mainTank != bot && bot->GetDistance(mainTank) <= REDIRECT_RANGE &&
        TankNeedsRedirect(mainTank))
        return mainTank;

    Player* best = nullptr;
    float bestAttackPower = 0.0f;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || !member->IsInWorld() ||
            member->GetMapId() != bot->GetMapId())
        {
            continue;
        }

        if (!PlayerbotAI::IsMelee(member) || !PlayerbotAI::IsDps(member))
            continue;

        if (bot->GetDistance(member) > REDIRECT_RANGE)
            continue;

        float attackPower = member->GetTotalAttackPowerValue(BASE_ATTACK);
        if (!best || attackPower > bestAttackPower)
        {
            best = member;
            bestAttackPower = attackPower;
        }
    }

    return best;
}
