/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCMultipliers_Rotface.h"
#include "ChooseTargetActions.h"
#include "DKActions.h"
#include "DruidBearActions.h"
#include "GenericSpellActions.h"
#include "ICCActions_Rotface.h"
#include "ICCScripts.h"
#include "MageActions.h"
#include "MovementActions.h"
#include "PaladinActions.h"
#include "PlayerbotAI.h"
#include "PriestActions.h"
#include "ShamanActions.h"
#include "WarriorActions.h"

float IccRotfaceMultiplier::GetValue(Action* action)
{
    Unit* boss1 = AI_VALUE2(Unit*, "find target", "rotface");
    if (!boss1)
        return 1.0f;

    {
        uint32 const now = getMSTime();
        IcecrownHelpers::IccInstanceState& st = IcecrownHelpers::IccState(bot->GetMap()->GetInstanceId());
        auto const& waitMap = st.rotfaceVileGasWaitUntil;
        auto it = waitMap.find(bot->GetGUID());
        bool const inWait = it != waitMap.end() && now < it->second;
        bool const isVictim =
            st.rotfaceVileGas.victimGuid == bot->GetGUID() &&
            getMSTimeDiff(st.rotfaceVileGas.castTime, now) < 8000;

        if (isVictim || inWait || botAI->HasAura("Vile Gas", bot))
        {
            if (dynamic_cast<IccRotfaceAvoidVileGasAction*>(action))
                return 1.0f;
            if (dynamic_cast<MovementAction*>(action))
                return 0.0f;
        }
    }

    if (botAI->HasAura("Vile Gas", bot))
        return 0.0f;

    if (PlayerbotAI::IsTank(bot) && dynamic_cast<TankAssistAction*>(action))
        return 0.0f;

    if (dynamic_cast<CombatFormationMoveAction*>(action) || dynamic_cast<AvoidAoeAction*>(action))
        return 0.0f;

    if (dynamic_cast<FleeAction*>(action) && !(bot->getClass() == CLASS_HUNTER))
        return 0.0f;

    if (dynamic_cast<CastBlinkBackAction*>(action) || dynamic_cast<CastArmyOfTheDeadAction*>(action))
        return 0.0f;

    if (PlayerbotAI::IsAssistTank(bot) &&
        (dynamic_cast<AttackRtiTargetAction*>(action) || dynamic_cast<TankAssistAction*>(action) ||
         dynamic_cast<CastTauntAction*>(action) || dynamic_cast<CastDarkCommandAction*>(action) ||
         dynamic_cast<CastHandOfReckoningAction*>(action) || dynamic_cast<CastGrowlAction*>(action)))
        return 0.0f;

    if (PlayerbotAI::IsAssistTank(bot) && boss1 && bot->GetVictim() == boss1)
    {
        bot->AttackStop();
        bot->SetTarget(ObjectGuid::Empty);
        return 0.0f;
    }

    // Never cure/dispel Mutated Infection — it must expire naturally to spawn a small ooze
    if (dynamic_cast<CurePartyMemberAction*>(action) || dynamic_cast<CastCleanseDiseaseAction*>(action) ||
        dynamic_cast<CastCleanseDiseaseOnPartyAction*>(action) ||
        dynamic_cast<CastCleanseSpiritCurseOnPartyAction*>(action) || dynamic_cast<CastCleanseSpiritAction*>(action))
        return 0.0f;

    {
        Creature* bigOoze = bot->FindNearestCreature(NPC_BIG_OOZE, 100.0f);
        bool castingNow = bigOoze && bigOoze->IsAlive() &&
            bigOoze->HasUnitState(UNIT_STATE_CASTING) && bigOoze->FindCurrentSpellBySpellId(SPELL_UNSTABLE_OOZE_EXPLOSION);

        if (castingNow && (dynamic_cast<MovementAction*>(action) || dynamic_cast<IccRotfaceGroupPositionAction*>(action)) &&
            !dynamic_cast<IccRotfaceMoveAwayFromExplosionAction*>(action))
            return 0.0f;
    }

    return 1.0f;
}
