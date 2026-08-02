/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "NaxxActions.h"

#include "Playerbots.h"

bool GothikChooseTargetAction::isUseful() { return helper.UpdateBossAI(); }

bool GothikChooseTargetAction::Execute(Event /*event*/)
{
    if (!helper.UpdateBossAI())
    {
        return false;
    }

    Unit* target = nullptr;

    // Boss first once he is standing on our side: the wave table is done by then, leftovers get
    // cleaved, and it lines up with the burst window NaxxBurstWindowMultiplier opens.
    if (helper.IsBossAttackable())
    {
        target = helper.GetBoss();
    }
    else
    {
        target = helper.GetBestAdd();
    }

    if (!target)
    {
        return false;
    }
    if (AI_VALUE(Unit*, "current target") == target)
    {
        return false;
    }
    return Attack(target);
}

bool GothikStayOnLivingSideAction::isUseful()
{
    return helper.UpdateBossAI() && !GothikBossHelper::IsLiveSide(bot);
}

bool GothikStayOnLivingSideAction::Execute(Event /*event*/)
{
    if (MoveTo(NAXX_MAP_ID, GothikBossHelper::LivingHoldX, GothikBossHelper::LivingHoldY,
               GothikBossHelper::ArenaFloorZ, false, false, false, false, MovementPriority::MOVEMENT_COMBAT))
    {
        return true;
    }

    return MoveInside(NAXX_MAP_ID, GothikBossHelper::LivingHoldX, GothikBossHelper::LivingHoldY,
                      GothikBossHelper::ArenaFloorZ, 3.0f, MovementPriority::MOVEMENT_COMBAT);
}
