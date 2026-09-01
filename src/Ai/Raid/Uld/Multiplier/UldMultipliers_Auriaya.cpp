/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldMultipliers_Auriaya.h"

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
#include "UldEncounter_Auriaya.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "UldTriggers.h"
#include "VehicleActions.h"

#include <set>
#include <string>

using namespace EncounterHelpers;

float AuriayaMovementGuardMultiplier::GetValue(Action* action)
{
    // Engaged, not merely present: these stand-downs hand generic behaviour to encounter nodes that
    // none of them run before the pull, so on sight alone they would leave bots rooted with no target.
    if (!action || !IsAuriayaEngaged(botAI))
        return 1.0f;

    // auriaya set dps priority action owns every non-tank's target, so the generic picker stands down
    // rather than pulling bots back onto whatever is nearest. "attack rti target" is deliberately left
    // alone: bots no longer set marks here, but a mark the player sets should still win.
    if (!botAI->IsTank(bot) && dynamic_cast<DpsAssistAction*>(action))
        return 0.0f;

    // Only the main tank and the ranged half of the raid stand on a spot. The off-tank chases Sanctum
    // Sentries and melee ride the boss, so both keep every generic mover - melee in particular still
    // need SetBehindTargetAction.
    if (!botAI->IsMainTank(bot) && !botAI->IsRanged(bot))
        return 1.0f;

    if (!dynamic_cast<MovementAction*>(action))
        return 1.0f;

    // AttackAction derives from MovementAction, so a blanket zero here would also kill targeting;
    // ReachTargetAction is what walks a healer into range of someone the anchor cannot reach.
    if (dynamic_cast<AttackAction*>(action) || dynamic_cast<ReachTargetAction*>(action))
        return 1.0f;

    static std::set<std::string> const encounterMovers = {
        "auriaya raid position action", "auriaya seeping essence action", "auriaya fall from floor action"};

    return encounterMovers.count(action->getName()) ? 1.0f : 0.0f;
}

// Both of these run behind the base class's shaman and totem-action checks, so the encounter lookup
// only happens for the handful of actions that could take the earth slot.
bool AuriayaAntiFearTotemGuardMultiplier::FearWindowActive() { return AuriayaFearWindowActive(botAI); }
