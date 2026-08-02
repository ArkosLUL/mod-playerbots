/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "NaxxActions.h"

#include "Playerbots.h"
#include "RaidBossHelpers.h"

bool HorsemanAttractAlternativelyAction::Execute(Event event)
{
    if (!helper.UpdateBossAI())
    {
        return false;
    }
    helper.CalculatePosToGo(bot);
    auto [posX, posY] = helper.CurrentAttractPos();
    if (MoveTo(bot->GetMapId(), posX, posY, helper.posZ, false, false, false, false, MovementPriority::MOVEMENT_COMBAT))
    {
        return true;
    }
    Unit* attackTarget = helper.CurrentAttackTarget();
    if (context->GetValue<Unit*>("current target")->Get() != attackTarget)
    {
        return Attack(attackTarget);
    }
    return false;
}

std::pair<Player*, Unit*> FourhorsemanRedirectThreatAction::GetAssignment()
{
    int32 index = GetRedirecterIndex();
    if (index < 0)
    {
        return {nullptr, nullptr};
    }

    // Only the two melee horsemen are tanked - Zeliek and Blaumeux belong to the attractor rotation,
    // and a redirect there would only fight it. Same split as the kill order in
    // HorsemanAttactInOrderAction: the assist tank opens on the Baron, the main tank on the Thane.
    Unit* thane = AI_VALUE2(Unit*, "find target", "thane korth'azz");
    Unit* baron = AI_VALUE2(Unit*, "find target", "baron rivendare");
    if (!baron)
    {
        baron = AI_VALUE2(Unit*, "find target", "highlord mograine");
    }

    if (index % 2 == 1)
    {
        if (Player* assistTank = GetGroupAssistTank(botAI, bot, 0))
        {
            return {assistTank, baron};
        }
    }
    return {GetGroupMainTank(botAI, bot), thane};
}

Player* FourhorsemanRedirectThreatAction::GetRedirectTank()
{
    if (!helper.IsEncounterUp())
    {
        return nullptr;
    }
    return GetAssignment().first;
}

Unit* FourhorsemanRedirectThreatAction::GetThreatDumpTarget() { return GetAssignment().second; }

bool HorsemanAttactInOrderAction::Execute(Event event)
{
    if (!helper.UpdateBossAI())
    {
        return false;
    }
    Unit* target = nullptr;
    Unit* thane = AI_VALUE2(Unit*, "find target", "thane korth'azz");
    Unit* lady = AI_VALUE2(Unit*, "find target", "lady blaumeux");
    Unit* sir = AI_VALUE2(Unit*, "find target", "sir zeliek");
    Unit* fourth = AI_VALUE2(Unit*, "find target", "baron rivendare");
    if (!fourth)
    {
        fourth = AI_VALUE2(Unit*, "find target", "highlord mograine");
    }
    std::vector<Unit*> attack_order;
    if (botAI->IsAssistTank(bot))
    {
        attack_order = {fourth, thane, lady, sir};
    }
    else
    {
        attack_order = {thane, fourth, lady, sir};
    }
    for (Unit* t : attack_order)
    {
        if (t && t->IsAlive())
        {
            target = t;
            break;
        }
    }
    if (target)
    {
        if (context->GetValue<Unit*>("current target")->Get() == target && botAI->GetState() == BOT_STATE_COMBAT)
        {
            return false;
        }
        if (!bot->IsWithinLOSInMap(target))
        {
            return MoveNear(target, 22.0f, MovementPriority::MOVEMENT_COMBAT);
        }
        return Attack(target);
    }
    return false;
}