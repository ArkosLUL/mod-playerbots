/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldMultipliers_Thorim.h"

#include "AttackAction.h"
#include "BurstCooldowns.h"
#include "ChooseTargetActions.h"
#include "EncounterHelpers.h"
#include "FollowActions.h"
#include "GenericSpellActions.h"
#include "HunterActions.h"
#include "MageActions.h"
#include "MovementActions.h"
#include "PaladinActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "PriestActions.h"
#include "ReachTargetActions.h"
#include "RogueActions.h"
#include "SharedDefines.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "UldActions.h"
#include "UldEncounter_Thorim.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "UldTriggers.h"
#include "VehicleActions.h"

#include <set>
#include <string>

using namespace EncounterHelpers;

static bool ThorimIsTargetSelectionAction(Action* action)
{
    return dynamic_cast<ThorimDpsPriorityAction*>(action) || dynamic_cast<DpsAssistAction*>(action) ||
           dynamic_cast<DpsAoeAction*>(action) || dynamic_cast<TankAssistAction*>(action) ||
           dynamic_cast<AggressiveTargetAction*>(action) || dynamic_cast<AttackAnythingAction*>(action) ||
           dynamic_cast<AttackLeastHpTargetAction*>(action);
}

float ThorimRunicBarrierMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    if (!dynamic_cast<MeleeAction*>(action) && !dynamic_cast<ReachTargetAction*>(action))
        return 1.0f;

    if (!ThorimBarrierBailLatched(botAI, bot))
        return 1.0f;

    Unit* colossus = GetThorimRunicColossus(botAI);
    return colossus && AI_VALUE(Unit*, "current target") == colossus ? 0.0f : 1.0f;
}

float ThorimArenaTargetGuardMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    if (!dynamic_cast<AttackAction*>(action) && !dynamic_cast<ReachTargetAction*>(action))
        return 1.0f;

    if (ThorimIsTargetSelectionAction(action))
        return 1.0f;

    if (!ThorimSplitActive(botAI) || GetThorimSquad(botAI, bot) != ThorimSquad::Arena)
        return 1.0f;

    Unit* target = AI_VALUE(Unit*, "current target");
    return target && !ThorimInArenaBox(target) ? 0.0f : 1.0f;
}

float ThorimBalconyGuardMultiplier::GetValue(Action* action)
{
    if (!action || (!dynamic_cast<AttackAction*>(action) && !dynamic_cast<ReachTargetAction*>(action)))
        return 1.0f;

    if (!ThorimOnBalconySide(bot) || GetThorimSquad(botAI, bot) != ThorimSquad::Gauntlet)
        return 1.0f;

    if (ThorimIsTargetSelectionAction(action))
        return 1.0f;

    if (dynamic_cast<ReachTargetAction*>(action) && ThorimPhase2Active(botAI))
        return 0.0f;

    if (!ThorimSplitActive(botAI))
        return 1.0f;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return 1.0f;

    Unit* boss = GetThorim(botAI);
    if (target == boss)
        return ThorimBalconyHitAllowed(botAI, bot) ? 1.0f : 0.0f;

    return target->GetEntry() == NPC_SIF ? 0.0f : 1.0f;
}

float ThorimChainWaitCastHoldMultiplier::GetValue(Action* action)
{
    if (!action || !ThorimCampWaitingOnChain(botAI, bot))
        return 1.0f;

    auto cached = spellIds.find(action);
    if (cached == spellIds.end())
    {
        CastSpellAction* spellAction = dynamic_cast<CastSpellAction*>(action);
        uint32 const spellId =
            spellAction ? context->GetValue<uint32>("spell id", spellAction->getSpell())->Get() : 0;
        cached = spellIds.emplace(action, spellId).first;
    }

    SpellInfo const* info = cached->second ? sSpellMgr->GetSpellInfo(cached->second) : nullptr;
    if (!info)
        return 1.0f;

    return info->IsChanneled() || info->CalcCastTime(bot) > 0 ? 0.0f : 1.0f;
}
