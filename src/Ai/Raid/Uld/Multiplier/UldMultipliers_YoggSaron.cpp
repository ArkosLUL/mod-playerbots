/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldMultipliers_YoggSaron.h"

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
#include "UldEncounter_YoggSaron.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "UldTriggers.h"
#include "VehicleActions.h"

#include <set>
#include <string>

using namespace EncounterHelpers;

float YoggSaronDpsTargetGuardMultiplier::GetValue(Action* action)
{
    // Cheap tests first: the read below is two 200 yd grid sweeps, and this runs for every action the
    // engine weighs.
    if (!action || botAI->IsTank(bot))
        return 1.0f;

    // "attack rti target" is deliberately left alone: bots no longer set marks here, but a mark a
    // player sets should still win. Nor is TankAssistAction zeroed - the resolver excludes tanks, so
    // that would strand them with nothing in its place.
    if (!dynamic_cast<DpsAssistAction*>(action))
        return 1.0f;

    // The resolver owns every non-tank's target for the whole encounter now that phase 1 has a kill
    // order of its own. This is literally the test YoggSaronSetDpsPriorityTrigger fires on: zeroing
    // the assist over a wider window than the resolver covers leaves a bot with no target source.
    YoggSaronTrigger yoggSaronTrigger(botAI);

    return yoggSaronTrigger.IsYoggSaronFight() ? 0.0f : 1.0f;
}

float YoggSaronDisplacementGuardMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    // Cheap gate first: the phase read below is a pair of 200 yd grid sweeps, and this runs for every
    // action the engine weighs. CastReachTargetSpellAction is the whole gap-closer family - Charge,
    // Intercept and both Feral Charges, with no other subclasses - and catching all of it matters,
    // because the Fury chain is charge then intercept then reach melee, so a partial veto only moves
    // the problem down the list. Blink and Disengage are plain CastSpellActions and need naming.
    bool const teleport =
        dynamic_cast<CastBlinkBackAction*>(action) || dynamic_cast<CastDisengageAction*>(action);
    bool const gapCloser =
        dynamic_cast<CastReachTargetSpellAction*>(action) || dynamic_cast<CastKillingSpreeAction*>(action);

    if (!teleport && !gapCloser)
        return 1.0f;

    uint32 const phase = YoggSaronPhase(botAI);
    if (!phase)
        return 1.0f;

    // reach melee survives either way, so melee still walk in on foot.
    return phase == 1 || teleport ? 0.0f : 1.0f;
}

bool YoggSaronAntiFearTotemGuardMultiplier::FearWindowActive() { return YoggSaronFearWindowActive(botAI); }
