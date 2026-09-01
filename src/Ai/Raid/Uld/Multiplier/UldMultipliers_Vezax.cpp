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
#include "Timer.h"
#include "UldActions.h"
#include "UldEncounter_Vezax.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "UldTriggers.h"
#include "VehicleActions.h"

#include <set>
#include <string>

using namespace EncounterHelpers;

float VezaxControlMovementMultiplier::GetValue(Action* action)
{
    if (!action || !VezaxFormationActive(botAI))
        return 1.0f;

    // Only the roles the formation actually places. Melee ride the boss and keep every generic mover.
    // The main tank is not one of them any more: it holds the anchor slot, and FollowAction would
    // drag it off the spawn point that every other radius here is measured from.
    if (!botAI->IsRanged(bot) && !botAI->IsMainTank(bot))
        return 1.0f;

    if (!dynamic_cast<MovementAction*>(action))
        return 1.0f;

    // AttackAction derives from MovementAction, so a blanket zero would also kill targeting;
    // ReachTargetAction is what keeps the tank in melee and walks a healer into range of someone the
    // formation cannot reach.
    if (dynamic_cast<AttackAction*>(action) || dynamic_cast<ReachTargetAction*>(action))
        return 1.0f;

    static std::set<std::string> const encounterMovers = {
        "vezax raid position action",       "vezax mark of the faceless action",
        "vezax vapor puddle clear action",  "vezax shadow crash dodge action",
        "vezax shadow crash soak action",   "vezax vapor soak action"};

    return encounterMovers.count(action->getName()) ? 1.0f : 0.0f;
}
