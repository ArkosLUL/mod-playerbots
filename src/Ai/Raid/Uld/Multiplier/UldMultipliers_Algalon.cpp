/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldMultipliers_Algalon.h"

#include "AttackAction.h"
#include "GenericSpellActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidEncounter.h"
#include "UldEncounter_Algalon.h"

namespace Family = RaidEncounterRules::Family;

float AlgalonTargetGuardMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    auto cached = damagesCurrentTarget.find(action);
    if (cached == damagesCurrentTarget.end())
    {
        bool damages = dynamic_cast<MeleeAction*>(action) || (ClassifyAction(action) & Family::PetAttack);
        if (!damages)
        {
            CastSpellAction* spell = dynamic_cast<CastSpellAction*>(action);
            damages = spell && !dynamic_cast<CastHealingSpellAction*>(action) &&
                      spell->GetTargetName() == "current target";
        }

        cached = damagesCurrentTarget.emplace(action, damages).first;
    }

    if (!cached->second)
        return 1.0f;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return 1.0f;

    switch (target->GetEntry())
    {
        case PB_NPC_COLLAPSING_STAR:
        case PB_NPC_LIVING_CONSTELLATION:
        case PB_NPC_UNLEASHED_DARK_MATTER:
            return AlgalonMayDamage(bot, target) ? 1.0f : 0.0f;
        default:
            return 1.0f;
    }
}

float AlgalonStarAoeMultiplier::GetValue(Action* action)
{
    if (!action || action->getThreatType() != Action::ActionThreatType::Aoe)
        return 1.0f;

    // Healing AoE reports the same threat type and is never held.
    if (dynamic_cast<CastHealingSpellAction*>(action))
        return 1.0f;

    return AlgalonAliveStarCount(botAI) >= 2 && AlgalonEngaged(botAI) ? 0.0f : 1.0f;
}
