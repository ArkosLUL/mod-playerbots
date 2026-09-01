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

// Thorim
//
// The nodes that pick what to attack, as opposed to the ones that act on what is already picked. They
// all derive from AttackAction, so a guard that zeroes AttackAction wholesale also zeroes the only
// thing that could correct a bad target, and a bot holding one is stuck on it for the rest of the
// pull. Thorim's balcony sits above the arena box, which is exactly how that happened.
static bool ThorimIsTargetSelectionAction(Action* action)
{
    return dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<DpsAoeAction*>(action) ||
           dynamic_cast<TankAssistAction*>(action) || dynamic_cast<AggressiveTargetAction*>(action) ||
           dynamic_cast<AttackAnythingAction*>(action) || dynamic_cast<AttackLeastHpTargetAction*>(action);
}

float ThorimRunicBarrierMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    // Only the swing and the walk that sets it up. Casts, heals and hazard dodges are untouched: a
    // blanket damage stop would throw away DPS the shield was never going to punish, and the gauntlet
    // is on a 2:45 timer in hard mode.
    if (!dynamic_cast<MeleeAction*>(action) && !dynamic_cast<ReachTargetAction*>(action))
        return 1.0f;

    if (!ThorimBarrierBailLatched(botAI, bot))
        return 1.0f;

    // Gated on the current target, so a switch onto anything else releases this for free.
    Unit* colossus = GetThorimRunicColossus(botAI);
    return colossus && AI_VALUE(Unit*, "current target") == colossus ? 0.0f : 1.0f;
}

float ThorimArenaLeashMultiplier::GetValue(Action* action)
{
    if (!action || !dynamic_cast<MovementAction*>(action))
        return 1.0f;

    // Hazard dodges stay live. Everything else, the chase included, is what this exists to stop.
    if (dynamic_cast<AvoidAoeAction*>(action))
        return 1.0f;

    // Picking a target is not walking anywhere. A bot outside the leash still has to be able to
    // choose one while the leash action carries it back, or it arrives with nothing to do.
    if (ThorimIsTargetSelectionAction(action))
        return 1.0f;

    static std::set<std::string> const encounterMovers = {"thorim arena leash action",
                                                          "thorim arena positioning action",
                                                          "thorim charged orb action",
                                                          "thorim sif blizzard action",
                                                          "thorim sif frost nova action"};

    if (encounterMovers.count(action->getName()))
        return 1.0f;

    return ThorimArenaLeashBreached(botAI, bot) ? 0.0f : 1.0f;
}

float ThorimDisableAutomaticTargetingMultiplier::GetValue(Action* action)
{
    if (!action || botAI->GetState() != BOT_STATE_COMBAT)
        return 1.0f;

    // TankAssistAction is deliberately not in scope, even though ThorimIsTargetSelectionAction covers
    // it: the encounter has no tank-targeting node to put in its place, and a tank steered onto the
    // ranged pick is worse than a tank on whatever is currently hitting the raid.
    if (!dynamic_cast<DpsAssistAction*>(action) && !dynamic_cast<DpsAoeAction*>(action) &&
        !dynamic_cast<AggressiveTargetAction*>(action) && !dynamic_cast<AttackAnythingAction*>(action) &&
        !dynamic_cast<AttackLeastHpTargetAction*>(action))
        return 1.0f;

    // The two roles the picker does not steer. A healer's target drives its wand and its offensive
    // dispels, and the trigger leaves it alone for that reason, so taking the generic picker away too
    // would leave it with nothing to hold.
    if (botAI->IsHeal(bot) || botAI->IsTank(bot))
        return 1.0f;

    // Only while the encounter actually has an answer. GetThorimDpsTarget returns nothing whenever no
    // tier has a candidate inside the arena box, and zeroing every picker in that state strands the
    // bot with no target source at all.
    return ThorimHasDpsTarget(botAI, bot) ? 0.0f : 1.0f;
}

float ThorimArenaTargetGuardMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    if (!dynamic_cast<AttackAction*>(action) && !dynamic_cast<ReachTargetAction*>(action))
        return 1.0f;

    // The escape hatch. Zeroing these too is a deadlock: the guard fires because the target is wrong,
    // and the action it kills is the one that would pick a different one.
    if (ThorimIsTargetSelectionAction(action))
        return 1.0f;

    if (!ThorimSplitActive(botAI) || GetThorimSquad(botAI, bot) != ThorimSquad::Arena)
        return 1.0f;

    Unit* target = AI_VALUE(Unit*, "current target");
    return target && !ThorimInArenaBox(target) ? 0.0f : 1.0f;
}

float ThorimArenaAnchorGuardMultiplier::GetValue(Action* action)
{
    if (!action || !dynamic_cast<MovementAction*>(action))
        return 1.0f;

    // Unlike the leash, this one exempts the chase: an add can land 24 yd from the centre, which puts
    // it up to 38 yd from an outer ring slot, and a ranged bot that cannot step into range is silent.
    if (dynamic_cast<AttackAction*>(action) || dynamic_cast<ReachTargetAction*>(action) ||
        dynamic_cast<AvoidAoeAction*>(action))
        return 1.0f;

    static std::set<std::string> const encounterMovers = {"thorim arena positioning action",
                                                          "thorim arena leash action",
                                                          "thorim charged orb action",
                                                          "thorim sif blizzard action",
                                                          "thorim sif frost nova action"};

    if (encounterMovers.count(action->getName()))
        return 1.0f;

    return ThorimArenaAnchorSettled(botAI, bot) ? 0.0f : 1.0f;
}

float ThorimMovementGuardMultiplier::GetValue(Action* action)
{
    if (!action || !dynamic_cast<MovementAction*>(action))
        return 1.0f;

    // AttackAction derives from MovementAction, so a blanket zero would also kill targeting, and a
    // bot that cannot clear a Rune Detonation is worse off than one standing slightly out of place.
    if (dynamic_cast<AttackAction*>(action) || dynamic_cast<ReachTargetAction*>(action) ||
        dynamic_cast<AvoidAoeAction*>(action))
        return 1.0f;

    static std::set<std::string> const encounterMovers = {"thorim phase 2 positioning action",
                                                          "thorim lightning charge action",
                                                          "thorim sif blizzard action",
                                                          "thorim sif frost nova action"};

    if (encounterMovers.count(action->getName()))
        return 1.0f;

    return ThorimMeleeRingSettled(botAI, bot) ? 0.0f : 1.0f;
}
