/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldMultipliers_Algalon.h"

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
#include "UldEncounter_Algalon.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "UldTriggers.h"
#include "VehicleActions.h"

#include <set>
#include <string>

using namespace EncounterHelpers;

// Algalon the Observer
//
// Big Bang is unavoidable raid-wide damage that immunity does not stop, so whoever is holding it off
// this cast has to still have the cooldown when it lands. Spending it on the low-mana or
// critical-health nodes thirty seconds earlier is what turns a survivable cast into a reset: with
// nobody left standing, CheckTargets finds no targets and Algalon ascends and evades.
float AlgalonSoakCooldownReserveMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    if (!dynamic_cast<CastDispersionAction*>(action) && action->getName() != "guardian spirit")
        return 1.0f;

    if (!AlgalonEncounterActive(botAI))
        return 1.0f;

    if (GetAlgalonBigBangSoaker(botAI) != bot)
        return 1.0f;

    // During the cast itself the soak action must be free to spend it.
    return AlgalonBigBangCasting(botAI) ? 1.0f : 0.0f;
}

float AlgalonCollapsingStarAoeMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<DpsAoeAction*>(action) || !AlgalonEncounterActive(botAI))
        return 1.0f;

    // One star alive is the state the pacing is trying to reach, so splash is harmless there. Phase 2
    // has no stars at all, which leaves Dark Matter cleave untouched.
    return AlgalonAliveStarCount(botAI) >= 2 ? 0.0f : 1.0f;
}

float AlgalonTargetGuardMultiplier::GetValue(Action* action)
{
    if (!action || !AlgalonEncounterActive(botAI))
        return 1.0f;

    static std::set<std::string> const encounterOwned = {
        "algalon constellation taunt action", "algalon constellation kite action",
        "algalon dark matter tank action",    "algalon collapsing star focus action",
        "algalon dark matter mark action"};

    if (encounterOwned.count(action->getName()))
        return 1.0f;

    Unit* currentTarget = AI_VALUE(Unit*, "current target");

    // A Living Constellation carries 20x base health and cannot be killed inside the six minute
    // enrage; the kite is the only way one ever leaves. Whoever is holding it still hits it, since
    // that threat is what keeps it following.
    if (currentTarget && currentTarget->GetEntry() == PB_NPC_LIVING_CONSTELLATION &&
        currentTarget->GetVictim() != bot)
    {
        return 0.0f;
    }

    // No hole standing with a Big Bang closing in. A skull mark on the star is only advice, and the
    // raid has to actually stop hitting the boss for the star to die in time.
    if (currentTarget && currentTarget == GetAlgalon(botAI) && AlgalonNeedsShelterUrgently(botAI) &&
        !botAI->IsTank(bot))
    {
        return 0.0f;
    }

    return 1.0f;
}

float AlgalonControlMovementMultiplier::GetValue(Action* action)
{
    if (!action || !AlgalonEncounterActive(botAI))
        return 1.0f;

    // Only the roles the formation actually places. Melee and the off-tank hold the boss, so both
    // keep every generic mover.
    if (!AlgalonTakesRingSlot(bot) && !botAI->IsMainTank(bot))
        return 1.0f;

    if (!dynamic_cast<MovementAction*>(action))
        return 1.0f;

    // AttackAction derives from MovementAction, so a blanket zero would also kill targeting;
    // ReachTargetAction is what walks a healer into range of someone the rings cannot reach.
    if (dynamic_cast<AttackAction*>(action) || dynamic_cast<ReachTargetAction*>(action))
        return 1.0f;

    static std::set<std::string> const encounterMovers = {
        "algalon raid position action", "algalon big bang hide action", "algalon cosmic smash action",
        "algalon leave black hole action", "algalon constellation kite action"};

    return encounterMovers.count(action->getName()) ? 1.0f : 0.0f;
}
