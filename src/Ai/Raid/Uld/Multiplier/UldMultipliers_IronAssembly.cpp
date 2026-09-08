/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldMultipliers_IronAssembly.h"

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
#include "UldEncounter_IronAssembly.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "UldTriggers.h"
#include "VehicleActions.h"

#include <set>
#include <string>

using namespace EncounterHelpers;

// Iron Assembly
float IronAssemblyDisableAutomaticTargetingMultiplier::GetValue(Action* action)
{
    if (botAI->GetState() != BOT_STATE_COMBAT)
        return 1.0f;

    if (!dynamic_cast<DpsAssistAction*>(action) && !dynamic_cast<TankAssistAction*>(action))
        return 1.0f;

    return IronAssemblyFormationActive(botAI) ? 0.0f : 1.0f;
}

float IronAssemblyMovementGuardMultiplier::GetValue(Action* action)
{
    if (!action || !IronAssemblyFormationActive(botAI))
        return 1.0f;

    if (!dynamic_cast<MovementAction*>(action))
        return 1.0f;

    // AttackAction derives from MovementAction, so a blanket zero would also kill targeting, and
    // ReachTargetAction is what walks a healer into range of someone the formation cannot reach.
    if (dynamic_cast<AttackAction*>(action) || dynamic_cast<ReachTargetAction*>(action))
        return 1.0f;

    static std::set<std::string> const encounterMovers = {
        "iron assembly overload action",
        "iron assembly lightning tendrils action",
        "iron assembly rune of death action",
        "iron assembly rune of power soak action",
        "iron assembly raid position action",
        "iron assembly tank assignment action"};

    if (encounterMovers.count(action->getName()))
        return 1.0f;

    // Only while this bot is actually committed to a hazard. Outside that window the generic movers
    // are what bring it back to the formation, and the position node has already yielded.
    return IronAssemblyMemberMustMove(botAI, bot) ? 0.0f : 1.0f;
}

float IronAssemblyChargeGuardMultiplier::GetValue(Action* action)
{
    // Cheap gate first: this only ever has an opinion about the gap-closers, and every one of them is
    // a CastReachTargetSpellAction - Charge, Intercept and both Feral Charges, with no other
    // subclasses.
    if (!dynamic_cast<CastReachTargetSpellAction*>(action))
        return 1.0f;

    if (!IronAssemblyFormationActive(botAI))
        return 1.0f;

    // The whole window, not only while the bot is still inside the circle. IronAssemblyMemberMustMove
    // goes false the moment it clears the clearance, and Charge and Intercept both reach 25 yd from
    // there - so gating on that hands the ability back at exactly the range that puts the bot back in
    // the blast, which is how the escape ends up arming the thing that undoes it.
    if (Unit* brundir = GetIronAssemblyMember(botAI, NPC_BRUNDIR))
        if (IronAssemblyOverloadActive(brundir) || IronAssemblyTendrilsActive(brundir))
            return 0.0f;

    return 1.0f;
}

float IronAssemblyHoldDpsCooldownsMultiplier::GetValue(Action* action)
{
    if (!IsIronAssemblyHardModeActive(botAI) || !IronAssemblyFormationActive(botAI))
        return 1.0f;

    if (!IsDpsCooldownAction(bot, action))
        return 1.0f;

    // Released the moment Steelbreaker is the last one standing. Under the normal order he dies first
    // and this would never release, which is why it is gated on the option rather than the phase.
    return IsSteelbreakerEmpowered(botAI) ? 1.0f : 0.0f;
}
