/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCMultipliers_BloodPrinces.h"
#include "AttackAction.h"
#include "ChooseTargetActions.h"
#include "DKActions.h"
#include "DruidActions.h"
#include "FollowActions.h"
#include "HunterActions.h"
#include "ICCActions_BloodPrinces.h"
#include "MageActions.h"
#include "MovementActions.h"
#include "PaladinActions.h"
#include "PlayerbotAI.h"
#include "PriestActions.h"
#include "ReachTargetActions.h"
#include "RogueActions.h"
#include "ShamanActions.h"
#include "WarriorActions.h"

float IccBpcAssistMultiplier::GetValue(Action* action)
{
    Unit* keleseth = AI_VALUE2(Unit*, "find target", "prince keleseth");
    if (!keleseth)
        return 1.0f;

    if (keleseth && (dynamic_cast<DpsAoeAction*>(action) || dynamic_cast<CastHurricaneAction*>(action) ||
        dynamic_cast<CastVolleyAction*>(action) || dynamic_cast<CastBlizzardAction*>(action) ||
        dynamic_cast<CastStarfallAction*>(action) || dynamic_cast<FanOfKnivesAction*>(action) ||
        dynamic_cast<CastWhirlwindAction*>(action) || dynamic_cast<CastMindSearAction*>(action) ||
        dynamic_cast<CastMagmaTotemAction*>(action) || dynamic_cast<CastFlamestrikeAction*>(action) ||
        dynamic_cast<CastExplosiveTrapAction*>(action) || dynamic_cast<CastExplosiveShotBaseAction*>(action) ||
        dynamic_cast<CastArmyOfTheDeadAction*>(action)))
        return 0.0f;

    if (PlayerbotAI::IsTank(bot) &&
        (dynamic_cast<AttackRtiTargetAction*>(action) || dynamic_cast<TankAssistAction*>(action)))
    {
        if (Group* group = bot->GetGroup())
        {
            for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
            {
                Player* member = itr->GetSource();
                if (member && member->IsAlive() && PlayerbotAI::IsAssistTank(member))
                    return 0.0f;
            }
        }
    }

    Aura* aura = botAI->GetAura("Shadow Prison", bot, false, true);

    // Bomb-assigned bot: block target switching and non-bomb BPC actions, allow combat rotation. Ahead of the
    // Shadow Prison and flame checks below, which it overrides. The assignment only decides these actions.
    bool const bombKeeps = dynamic_cast<IccBpcKineticBombAction*>(action) || dynamic_cast<AvoidAoeAction*>(action);
    bool const bombBlocks =
        !bombKeeps &&
        (dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<TankAssistAction*>(action) ||
         dynamic_cast<AttackRtiTargetAction*>(action) || dynamic_cast<IccBpcEmpoweredVortexAction*>(action) ||
         dynamic_cast<IccBpcBallOfFlameAction*>(action) || dynamic_cast<CombatFormationMoveAction*>(action) ||
         dynamic_cast<FollowAction*>(action));
    if ((bombKeeps || bombBlocks) && !(aura && aura->GetStackAmount() > 18) && IccBpcAssignedKineticBomb(botAI))
        return bombKeeps ? 1.0f : 0.0f;

    // Shadow prison movement block (non-bomb bots use normal 12 stack limit).
    // AttackAction derives from MovementAction, so it must be excluded here or
    // bots stop attacking entirely (not just repositioning) once stacked high.
    // ReachTargetAction (and ReachMeleeAction, which derives from it) is what
    // actually walks a ranged/caster bot into range of a new target - Attack()
    // alone does not move them - so it must be excluded too or they can never
    // close distance on a newly marked target while stacked.
    if (aura && aura->GetStackAmount() > (PlayerbotAI::IsTank(bot) ? 18 : 12) &&
        dynamic_cast<MovementAction*>(action) && !dynamic_cast<AttackAction*>(action) &&
        !dynamic_cast<ReachTargetAction*>(action))
        return 0.0f;

    Unit* valanar = AI_VALUE2(Unit*, "find target", "prince valanar");
    if (!valanar)
        return 1.0f;

    if (valanar && valanar->HasUnitState(UNIT_STATE_CASTING) &&
        (valanar->FindCurrentSpellBySpellId(SPELL_EMPOWERED_SHOCK_VORTEX1) ||
         valanar->FindCurrentSpellBySpellId(SPELL_EMPOWERED_SHOCK_VORTEX2) ||
         valanar->FindCurrentSpellBySpellId(SPELL_EMPOWERED_SHOCK_VORTEX3) ||
         valanar->FindCurrentSpellBySpellId(SPELL_EMPOWERED_SHOCK_VORTEX4)))
    {
        if (dynamic_cast<AvoidAoeAction*>(action) || dynamic_cast<IccBpcEmpoweredVortexAction*>(action))
            return 1.0f;
        else
            return 0.0f;
    }

    // Cached per ms: two grid searches, asked for every action the bot weighs. A bot never ticks twice in one ms.
    uint32 const now = getMSTime();
    if (now != _flamesCheckedMs)
    {
        _flamesCheckedMs = now;
        Unit* flame1 = bot->FindNearestCreature(NPC_BALL_OF_FLAME, 100.0f);
        Unit* flame2 = bot->FindNearestCreature(NPC_BALL_OF_INFERNO_FLAME, 100.0f);
        _infernoFlameUp = flame2 != nullptr;
        _chasedByFlame = (flame1 && flame1->GetVictim() == bot) || (flame2 && flame2->GetVictim() == bot);
    }

    if (_infernoFlameUp)
    {
        if (dynamic_cast<AvoidAoeAction*>(action) || dynamic_cast<IccBpcKineticBombAction*>(action))
            return 0.0f;

        if (dynamic_cast<IccBpcBallOfFlameAction*>(action))
            return 1.0f;
    }

    if (_chasedByFlame)
    {
        if (dynamic_cast<IccBpcBallOfFlameAction*>(action))
            return 1.0f;
        else
            return 0.0f;
    }

    // For assist tank during BPC fight
    if (PlayerbotAI::IsAssistTank(bot) && !(aura && aura->GetStackAmount() > 18))
    {
        // Allow BPC-specific actions
        if (dynamic_cast<IccBpcKelesethTankAction*>(action))
            return 1.0f;

        // Disable normal assist behavior
        if (dynamic_cast<TankAssistAction*>(action) ||
            dynamic_cast<FleeAction*>(action) ||
            dynamic_cast<CastConsecrationAction*>(action))
            return 0.0f;
    }

    return 1.0f;
}
