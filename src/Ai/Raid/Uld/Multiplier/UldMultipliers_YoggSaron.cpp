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

#include <algorithm>
#include <cmath>
#include <set>
#include <string>

using namespace EncounterHelpers;

float YoggSaronDpsTargetGuardMultiplier::GetValue(Action* action)
{
    // Cheap tests first: the read below is two 200 yd grid sweeps, and this runs for every action the
    // engine weighs.
    //
    // "attack rti target" is deliberately left alone: bots no longer set marks here, but a mark a
    // player sets should still win. The resolver excludes tanks, so TankAssistAction only stands down
    // where guardian control gives a tank its target instead.
    if (dynamic_cast<TankAssistAction*>(action))
        return TankAssistGuard();

    if (!dynamic_cast<DpsAssistAction*>(action) || botAI->IsTank(bot))
        return 1.0f;

    // The resolver owns every non-tank's target for the whole encounter now that phase 1 has a kill
    // order of its own. This is literally the test YoggSaronSetDpsPriorityTrigger fires on: zeroing
    // the assist over a wider window than the resolver covers leaves a bot with no target source.
    return YoggSaronEncounterActive(botAI) ? 0.0f : 1.0f;
}

float YoggSaronDpsTargetGuardMultiplier::TankAssistGuard()
{
    // Its picker ranks by "not attacking me", then nearest by GetDistance, which takes off Yogg's 30 yd
    // combat reach, so Yogg always reads as nearest. It held one bot tank on him for all of phase 3 with
    // no Guardian ever targeted. Guardian control hands out a Guardian while one can still be held; 70 yd
    // covers the whole spawn ring, 38-48 yd round Yogg, from the melee spot 18.5 yd behind him.
    constexpr float guardianSearchRadius = 70.0f;

    if (!botAI->IsTank(bot) || !YoggSaronHoldableGuardianWithin(bot, guardianSearchRadius))
        return 1.0f;

    return YoggSaronInPhase3(botAI) ? 0.0f : 1.0f;
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

float YoggSaronMovementGuardMultiplier::FleeGuard()
{
    // Ranged and healers only. The station owns their feet in phase 1 and stepping off it is what
    // hands the second orbit a stacked back line to sweep; melee and the tank keep it, because the
    // leash already owns them and no melee bot fled at all in the pull that measured this.
    if (!PlayerbotAI::IsRanged(bot) && !PlayerbotAI::IsHeal(bot))
        return 1.0f;

    return YoggSaronInPhase1(botAI) ? 0.0f : 1.0f;
}

bool YoggSaronMovementGuardMultiplier::MeleeReachIsWrong(Action* action)
{
    // Only the melee reach pays the phase read, so every other reach keeps the cheap ring arithmetic
    // below ahead of it.
    if (action->getName() != "reach melee" || !YoggSaronInPhase1(botAI))
        return false;

    // Walking at a Guardian that is not on the stack is walking out of the cloud-free circle, and the
    // innermost orbit collected a Guardian on almost every one of its 24 s laps that way. The tank
    // taunts it in instead - but only if there is a tank alive to do it, or this strands the melee
    // half of the raid with nothing to close on.
    Unit* target = AI_VALUE(Unit*, "current target");

    return target && !YoggSaronGuardianOnTheStack(target) && YoggSaronBotTankAlive(botAI);
}

float YoggSaronMovementGuardMultiplier::SetBehindGuard()
{
    // Arithmetic first. Below the platform there is no body, and the phase read is two 200 yd sweeps.
    if (bot->GetPositionZ() < ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT)
        return 1.0f;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return 1.0f;

    // Both spots SetBehindTargetAction::Execute picks from: 108 degrees either side of the target's
    // facing, at the bot's current distance from it. Guardians are tanked 18.5 yd from the middle and are
    // several yards across, so one pull's melee aimed 67 of 631 of these inside the ring and were thrown
    // 52 times walking them. Guardians have no frontal attack, so not getting behind costs parries only.
    float const distance =
        std::max(bot->GetExactDist(target), bot->GetMeleeRange(target) / 2.0f) - bot->GetCombatReach();
    Position const& middle = ULDUAR_YOGG_SARON_MIDDLE;

    bool intoRing = false;
    for (float const side : {1.0f, -1.0f})
    {
        float const angle = Position::NormalizeOrientation(target->GetOrientation() +
                                                           side * 3.0f * static_cast<float>(M_PI) / 5.0f);
        float const x = target->GetPositionX() + std::cos(angle) * distance;
        float const y = target->GetPositionY() + std::sin(angle) * distance;

        intoRing = intoRing || middle.GetExactDist2d(x, y) < ULDUAR_YOGG_SARON_BODY_KNOCKBACK_CLEAR_RADIUS ||
                   !YoggSaronRouteClearOfBody(bot, x, y);
    }

    if (!intoRing)
        return 1.0f;

    uint32 const phase = YoggSaronPhase(botAI);
    return phase == 2 || phase == 3 ? 0.0f : 1.0f;
}

float YoggSaronMovementGuardMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    if (dynamic_cast<FleeAction*>(action))
        return FleeGuard();

    if (dynamic_cast<SetBehindTargetAction*>(action))
        return SetBehindGuard();

    if (!dynamic_cast<ReachTargetAction*>(action))
        return 1.0f;

    // The heal reach is what keeps a healer in range of somebody no station can see, and it never walks
    // anyone into the body. Leave it alone.
    if (action->getName() == "reach party member to heal")
        return 1.0f;

    // Both windows are within a few yards of the middle of the boss platform, and the phase read below
    // is a pair of 200 yd grid sweeps, so the arithmetic goes first. The brain room middle is 2.3 yd
    // from the platform middle in 2d, so without this the ring test below would veto the reach of every
    // bot standing on the Brain, 93 yd underneath it.
    if (bot->GetPositionZ() < ULDUAR_YOGG_SARON_BOSS_ROOM_AXIS_Z_PATHING_ISSUE_DETECT)
        return 1.0f;

    if (MeleeReachIsWrong(action))
        return 0.0f;

    float const fromMiddle =
        bot->GetDistance2d(ULDUAR_YOGG_SARON_MIDDLE.GetPositionX(), ULDUAR_YOGG_SARON_MIDDLE.GetPositionY());
    if (fromMiddle >= ULDUAR_YOGG_SARON_BODY_KNOCKBACK_CLEAR_RADIUS)
        return 1.0f;

    uint32 const phase = YoggSaronPhase(botAI);

    // Exactly the radius the phase 2 spacing trigger fires at, so reach stands down only while that
    // node owns the bot and is free again the moment it has been walked clear. A wider band here would
    // leave a ring where neither node moves anybody. The spacing node runs in phase 3 too.
    if ((phase == 2 || phase == 3) && fromMiddle < ULDUAR_YOGG_SARON_BODY_KNOCKBACK_RADIUS)
        return 0.0f;

    if (phase != 1 && phase != 2)
        return 1.0f;

    // The walk out of the ring, which spans the phase boundary: the window is phase 1 and the hold that
    // outlives it is phase 2.
    return YoggSaronHandoverState(botAI).clearing ? 0.0f : 1.0f;
}

float YoggSaronPhase1AoeHoldMultiplier::GetValue(Action* action)
{
    if (!action || action->getThreatType() != Action::ActionThreatType::Aoe)
        return 1.0f;

    if (dynamic_cast<CastHealingSpellAction*>(action))
        return 1.0f;

    // Brain level first: no grid sweep for the bots under the floor.
    if (YoggSaronOnBrainLevel(bot) || !YoggSaronInPhase1(botAI))
        return 1.0f;

    return YoggSaronPhase1AoeHold(botAI) ? 0.0f : 1.0f;
}

bool YoggSaronAntiFearTotemGuardMultiplier::FearWindowActive() { return YoggSaronFearWindowActive(botAI); }
