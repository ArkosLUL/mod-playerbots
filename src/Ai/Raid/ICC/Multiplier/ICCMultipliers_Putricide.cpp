/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCMultipliers_Putricide.h"
#include "DKActions.h"
#include "DruidBearActions.h"
#include "FollowActions.h"
#include "HunterActions.h"
#include "ICCActions_Putricide.h"
#include "MageActions.h"
#include "MovementActions.h"
#include "PaladinActions.h"
#include "PlayerbotAI.h"
#include "PriestActions.h"
#include "WarriorActions.h"

float IccAddsPutricideMultiplier::GetValue(Action* action)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "professor putricide");
    if (!boss)
        return 1.0f;
    Unit* boss1 = AI_VALUE2(Unit*, "find target", "rotface");
    if (boss1)
        return 1.0f;
    Unit* boss2 = AI_VALUE2(Unit*, "find target", "festergut");
    if (boss2)
        return 1.0f;

    if (Unit* vehBase = bot->GetVehicleBase())
    {
        uint32 e = vehBase->GetEntry();
        if (e == NPC_MUTATED_ABOMINATION_10 || e == NPC_MUTATED_ABOMINATION_25)
        {
            if (dynamic_cast<IccPutricideAbominationAction*>(action))
                return 1.0f;
            return 0.0f;
        }
    }

    bool hasGaseousBloat = botAI->HasAura("Gaseous Bloat", bot);
    bool hasUnboundPlague = botAI->HasAura("Unbound Plague", bot);

    if (PlayerbotAI::IsTank(bot) &&
        bot->GetMotionMaster()->GetCurrentMovementGeneratorType() == FOLLOW_MOTION_TYPE)
    {
        if (dynamic_cast<FollowAction*>(action) ||
            dynamic_cast<IccPutricideAvoidMalleableGooAction*>(action))
            return 1.0f;
        return 0.0f;
    }

    if (!(bot->getClass() == CLASS_HUNTER) && dynamic_cast<FleeAction*>(action))
        return 0.0f;

    if (dynamic_cast<CombatFormationMoveAction*>(action))
        return 0.0f;

    if (dynamic_cast<CastDisengageAction*>(action))
        return 0.0f;

    if (dynamic_cast<CastBlinkBackAction*>(action))
        return 0.0f;

    if (PlayerbotAI::IsTank(bot))
    {
        auto GetPlagueStacks = [&](Unit* unit) -> uint32
        {
            if (!unit)
                return 0;
            Aura* a = botAI->GetAura("mutated plague", unit, false, true);
            return a ? a->GetStackAmount() : 0;
        };

        uint32 const myStacks = GetPlagueStacks(bot);

        // Another tank has fewer stacks — they should be tanking instead of us.
        // Block generic taunts (so we don't fight for aggro), keep movement, and
        // cancel the rest of the rotation (IccPutricideMutatedPlagueAction owns
        // the AttackStop/taunt handoff).
        bool anotherTankHasFewer = false;
        if (Group* group = bot->GetGroup())
        {
            for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
            {
                Player* member = itr->GetSource();
                if (!member || member == bot || !member->IsAlive() || !member->IsInWorld())
                    continue;
                if (!PlayerbotAI::IsTank(member))
                    continue;

                if (GetPlagueStacks(member) < myStacks)
                {
                    anotherTankHasFewer = true;
                    break;
                }
            }
        }

        if (anotherTankHasFewer)
        {
            if (dynamic_cast<CastTauntAction*>(action) ||
                dynamic_cast<CastDarkCommandAction*>(action) ||
                dynamic_cast<CastHandOfReckoningAction*>(action) ||
                dynamic_cast<CastGrowlAction*>(action))
                return 0.0f;

            if (dynamic_cast<MovementAction*>(action))
                return 1.0f;

            if (dynamic_cast<IccPutricideMutatedPlagueAction*>(action))
                return 1.0f;

            return 0.0f;
        }
    }

    if (hasGaseousBloat)
    {
        if (dynamic_cast<IccPutricideGasCloudAction*>(action))
            return 1.0f;

        if (dynamic_cast<IccPutricideGrowingOozePuddleAction*>(action))
            return 1.0f;

        if (PlayerbotAI::IsHeal(bot))
            return 1.0f;
        else
            return 0.0f; // Cancel all other actions when we need to handle Gaseous Bloat
    }

    if (hasUnboundPlague && IccPutricidePhase(bot, boss) != 3)
    {
        if (dynamic_cast<IccPutricideAvoidMalleableGooAction*>(action))
            return 1.0f;
        else
            return 0.0f; // Cancel all other actions when we need to handle Unbound Plague
    }

    if (dynamic_cast<IccPutricideVolatileOozeAction*>(action))
    {
        if (dynamic_cast<IccPutricideAvoidMalleableGooAction*>(action))
            return 1.0f;
        if (dynamic_cast<IccPutricideGrowingOozePuddleAction*>(action) && !PlayerbotAI::IsMainTank(bot))
            return 0.0f;
    }

    return 1.0f;
}
