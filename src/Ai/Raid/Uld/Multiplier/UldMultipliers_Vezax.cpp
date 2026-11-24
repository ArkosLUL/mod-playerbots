/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldMultipliers_Vezax.h"

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
#include "ThreatManager.h"
#include "Timer.h"
#include "UldActions.h"
#include "UldEncounter_Vezax.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "UldTriggers.h"
#include "WarlockActions.h"
#include "VehicleActions.h"

#include <limits>
#include <string>

using namespace EncounterHelpers;

namespace
{
bool IsVezaxDamageAction(Action* action)
{
    return dynamic_cast<MeleeAction*>(action) ||
           (dynamic_cast<CastSpellAction*>(action) && !dynamic_cast<CastBuffSpellAction*>(action) &&
            !dynamic_cast<CastHealingSpellAction*>(action));
}
}

float VezaxSuppressLifeTapMultiplier::GetValue(Action* action)
{
    if (!action || !dynamic_cast<CastLifeTapAction*>(action))
        return 1.0f;

    return VezaxEncounterActive(botAI) ? 0.0f : 1.0f;
}

float VezaxHoldCastOutsideFieldMultiplier::GetValue(Action* action)
{
    if (!action || PlayerbotAI::IsHeal(bot) || !PlayerbotAI::IsRanged(bot) || PlayerbotAI::IsMainTank(bot))
        return 1.0f;

    if (!IsVezaxDamageAction(action))
        return 1.0f;

    if (!VezaxEncounterActive(botAI))
        return 1.0f;

    if (bot->HasAura(SPELL_VEZAX_SHADOW_CRASH_FIELD))
        return 1.0f;

    return VezaxTargetingAnimus(botAI) ? 1.0f : 0.0f;
}

float VezaxThreatMultiplier::GetValue(Action* action)
{
    if (!action || PlayerbotAI::IsTank(bot) || PlayerbotAI::IsHeal(bot) || !IsVezaxDamageAction(action))
        return 1.0f;

    Unit* boss = AI_VALUE(Unit*, "current target");
    if (!boss || !boss->IsAlive() ||
        (boss->GetEntry() != NPC_VEZAX && boss->GetEntry() != NPC_VEZAX_SARONITE_ANIMUS))
        return 1.0f;

    ThreatManager& threat = boss->GetThreatMgr();

    Player* holder = boss->GetVictim() ? boss->GetVictim()->ToPlayer() : nullptr;
    if (!holder || !PlayerbotAI::IsTank(holder))
    {
        holder = GetGroupMainTank(bot);
        if (!holder || holder == bot)
            return 1.0f;

        if (boss->GetEntry() == NPC_VEZAX && !threat.IsThreatenedBy(holder))
            return 1.0f;
    }

    float const own = threat.GetThreat(bot);
    return own > 0.0f && own >= ULDUAR_VEZAX_THREAT_SHARE * threat.GetThreat(holder) ? 0.0f : 1.0f;
}

float VezaxTricksOnTankMultiplier::GetValue(Action* action)
{
    if (!action || !dynamic_cast<CastTricksOfTheTradeAction*>(action) || !VezaxEncounterActive(botAI))
        return 1.0f;

    Unit* target = AI_VALUE(Unit*, "tricks of the trade target");
    Player* player = target ? target->ToPlayer() : nullptr;
    return player && PlayerbotAI::IsTank(player) ? 1.0f : 0.0f;
}

float VezaxBurstForAnimusMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    std::string const& name = burstNames.Get(action, [this, action]
    {
        std::string const actionName = action->getName();
        bool const held = IsBurstCooldownAction(actionName) && actionName != "bloodlust" && actionName != "heroism" &&
                          !IsManaReturnCooldown(bot, actionName);
        return held ? actionName : std::string();
    });

    if (name.empty() || !PlayerbotAI::IsDps(bot))
        return 1.0f;

    VezaxBurstDecision const decision = VezaxBurstForAnimus(botAI, name);
    if (decision.call == VezaxBurstCall::None)
        return 1.0f;

    NoteDecision(name, decision);
    return decision.call == VezaxBurstCall::Hold || decision.call == VezaxBurstCall::Wait ? 0.0f : 1.0f;
}

void VezaxBurstForAnimusMultiplier::NoteDecision(std::string const& name, VezaxBurstDecision const& decision)
{
    if (decision.pullMs != notedPull)
    {
        noted.clear();
        notedPull = decision.pullMs;
    }

    auto const [it, fresh] = noted.try_emplace(name, decision.call);
    if (!fresh && it->second == decision.call)
        return;

    it->second = decision.call;
    if (!RaidObs::Active())
        return;

    char const* call = "none";
    switch (decision.call)
    {
        case VezaxBurstCall::Use:
            call = "use";
            break;
        case VezaxBurstCall::Hold:
            call = "hold";
            break;
        case VezaxBurstCall::Wait:
            call = "wait";
            break;
        case VezaxBurstCall::Animus:
            call = "animus";
            break;
        case VezaxBurstCall::None:
            break;
    }

    std::string const cooldown = decision.cooldownMs == std::numeric_limits<uint32>::max()
                                     ? "fight"
                                     : std::to_string(decision.cooldownMs / IN_MILLISECONDS);
    RaidObs::Note(bot, "vezax.burst", name + ' ' + call + ' ' + cooldown);
}

float VezaxDamageHoldMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    auto cached = reaches.find(action);
    if (cached == reaches.end())
    {
        Reach reach = Reach::None;
        CastSpellAction* spell = dynamic_cast<CastSpellAction*>(action);
        if (spell && !dynamic_cast<CastHealingSpellAction*>(action) && !dynamic_cast<CastBuffSpellAction*>(action))
        {
            if (spell->getThreatType() == Action::ActionThreatType::Aoe)
                reach = Reach::Area;
            else if (spell->GetTargetName() == "current target")
                reach = Reach::CurrentTarget;
        }

        cached = reaches.emplace(action, reach).first;
    }

    if (cached->second == Reach::None || !VezaxDamageHeld(botAI))
        return 1.0f;

    if (cached->second == Reach::Area)
        return 0.0f;

    Unit* target = AI_VALUE(Unit*, "current target");
    return target && target->GetEntry() == NPC_VEZAX ? 0.0f : 1.0f;
}
