/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCMultipliers_Valithria.h"
#include "ChooseTargetActions.h"
#include "FollowActions.h"
#include "ICCActions_Valithria.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "PriestActions.h"

float IccValithriaDreamCloudMultiplier::GetValue(Action* action)
{
    // Cached per ms: asked for every action the bot weighs, and a bot never ticks twice in one ms.
    uint32 const now = getMSTime();
    if (now != _checkedMs)
    {
        _checkedMs = now;
        _bossPresent = bot->FindNearestCreature(NPC_VALITHRIA_DREAMWALKER, 100.0f) != nullptr;
        _cloudBuff = botAI->GetAura("Twisted Nightmares", bot) || botAI->GetAura("Emerald Vigor", bot);
        _chasedByZombie = false;
        if (_bossPresent && !PlayerbotAI::IsTank(bot))
        {
            std::list<Creature*> zombies;
            bot->GetCreatureListWithEntryInGrid(zombies, NPC_BLISTERING_ZOMBIE, 100.0f);
            for (Creature* z : zombies)
            {
                if (z && z->IsAlive() && z->GetVictim() == bot)
                {
                    _chasedByZombie = true;
                    break;
                }
            }
        }
    }

    if (!_bossPresent && !bot->HasAura(SPELL_DREAM_STATE))
        return 1.0f;

    if (dynamic_cast<FollowAction*>(action) || dynamic_cast<CombatFormationMoveAction*>(action))
        return 0.0f;

    // Zombie victim: only the kite action runs. Blocks combat/movement so bot
    // doesn't try to attack/cast/move toward marks while being chased.
    if (_chasedByZombie && !PlayerbotAI::IsTank(bot))
    {
        if (dynamic_cast<IccValithriaZombieKiteAction*>(action))
            return 1.0f;
        return 0.0f;
    }

    if (PlayerbotAI::IsTank(bot))
    {
        if (dynamic_cast<AttackRtiTargetAction*>(action))
            return 0.0f;
    }
    else
    {
        // Non-tanks must strictly follow RTI marks. Block generic assist actions
        // so bots never attack unmarked adds; AttackRtiTargetAction drives them to
        // skull/cross targets set by HandleMarkingLogic.
        if (dynamic_cast<TankAssistAction*>(action))
            return 0.0f;

        // Melee bots must not engage Blistering Zombies (one-shot melee swing).
        // Only ranged DPS handle them. If RTI/current target is a zombie, block
        // attack actions so melee falls through to other priorities.
        if (!PlayerbotAI::IsRangedDps(bot) && !PlayerbotAI::IsHeal(bot))
        {
            Unit* victim = bot->GetVictim();
            bool victimIsZombie = victim && victim->GetEntry() == NPC_BLISTERING_ZOMBIE;

            bool rtiIsZombie = false;
            if (Group* group = bot->GetGroup())
            {
                ObjectGuid rtiGuid = group->GetTargetIcon(RtiTargetValue::skullIndex);
                if (!rtiGuid.IsEmpty())
                {
                    Unit* rtiUnit = ObjectAccessor::GetUnit(*bot, rtiGuid);
                    if (rtiUnit && rtiUnit->GetEntry() == NPC_BLISTERING_ZOMBIE)
                        rtiIsZombie = true;
                }
            }

            if (victimIsZombie || rtiIsZombie)
            {
                if (dynamic_cast<AttackRtiTargetAction*>(action) ||
                    dynamic_cast<DpsAssistAction*>(action))
                    return 0.0f;
            }
        }
    }

    if (PlayerbotAI::IsHeal(bot) && _cloudBuff)
        if (dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<AttackRtiTargetAction*>(action))
            return 0.0f;

    if (bot->HasAura(SPELL_DREAM_STATE) && !bot->HealthBelowPct(50))
    {
        if (dynamic_cast<IccValithriaDreamCloudAction*>(action))
            return 1.0f; // Allow Dream Cloud action
        else
            return 0.0f; // Cancel all other actions when we need to handle Dream Cloud
    }

    return 1.0f;

}
