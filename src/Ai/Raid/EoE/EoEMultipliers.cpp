/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "EoEMultipliers.h"
#include "ChooseTargetActions.h"
#include "DKActions.h"
#include "DruidActions.h"
#include "DruidBearActions.h"
#include "EoEActions.h"
#include "EoETriggers.h"
#include "FollowActions.h"
#include "GenericActions.h"
#include "GenericSpellActions.h"
#include "HunterActions.h"
#include "MageActions.h"
#include "MovementActions.h"
#include "PaladinActions.h"
#include "ReachTargetActions.h"
#include "ScriptedCreature.h"
#include "VehicleActions.h"
#include "WarriorActions.h"

float MalygosMultiplier::GetValue(Action* action)
{
    // getPhase caches per bot for half a second, which matters here: this runs once per action per
    // bot per tick.
    uint8 phase = MalygosTrigger::getPhase(bot);
    if (phase == 0) { return 1.0f; }

    if (phase == 1)
    {
        if (dynamic_cast<FollowAction*>(action))
        {
            return 0.0f;
        }

        // Malygos has a CombatReach of 20, so he parks about 21y short of his victim - just outside
        // what ReachMeleeAction considers "in melee". Left alone it walks the tank into the boss
        // every tick while MalygosPositionAction walks it back out, and the boss pivots between the
        // two, raking the Arcane Breath cone across the raid. The boss closes the gap himself, so
        // once the tank has aggro it simply holds the spot.
        Unit* boss = MalygosTrigger::getMalygos(bot);
        bool const isBossTank = botAI->IsMainTank(bot) || (boss && boss->GetVictim() == bot);
        if (boss && boss->GetVictim() == bot && dynamic_cast<ReachTargetAction*>(action))
        {
            return 0.0f;
        }

        // Malygos' CombatReach of 20 keeps "enemy too close for spell" active for anyone holding a
        // spot near him, and every class wires that trigger to an escape somewhere between 34 and 50
        // relevance - all of it above MalygosPositionAction. Left alone the bot steps out, gets
        // dragged back next tick and never finishes a cast.
        if (dynamic_cast<FleeAction*>(action) || dynamic_cast<RunAwayAction*>(action) ||
            dynamic_cast<CastBlinkBackAction*>(action) || dynamic_cast<CastDisengageAction*>(action))
        {
            return 0.0f;
        }

        if (botAI->IsDps(bot) && dynamic_cast<DpsAssistAction*>(action))
        {
            return 0.0f;
        }

        if (botAI->IsRangedDps(bot) && dynamic_cast<DropTargetAction*>(action))
        {
            return 0.0f;
        }

        // Nobody but the boss' current victim walks anywhere in P1 under their own steam.
        // Ranged were chasing Power Sparks back inside Malygos' minimum range; melee were walking
        // out to get behind him (set behind) or to spread (combat formation move) and being dragged
        // back to the stack next tick, which is the shuffling that shows up in game. The hold spots
        // already sit inside his 20y combat reach for melee and outside the minimum range for
        // ranged, and the DK grips sparks to the raid, so there is nothing left to walk to.
        // Note AttackAction derives from MovementAction, so the EoE actions have to be named or they
        // go with it. Closing on a heal target is the one exception, same as in P2.
        if (!isBossTank && !dynamic_cast<MalygosPositionAction*>(action) &&
            !dynamic_cast<MalygosTargetAction*>(action) && !dynamic_cast<KillPowerSparkAction*>(action) &&
            !dynamic_cast<ReachPartyMemberToHealAction*>(action) &&
            (dynamic_cast<MovementAction*>(action) || dynamic_cast<CastReachTargetSpellAction*>(action)))
        {
            return 0.0f;
        }

        if (!botAI->IsMainTank(bot) && dynamic_cast<TankAssistAction*>(action))
        {
            return 0.0f;
        }
    }
    else if (phase == 2)
    {
        if (botAI->IsDps(bot) && dynamic_cast<DpsAssistAction*>(action))
        {
            return 0.0f;
        }

        // Keep the generic flee from walking bots off the edge; MalygosPositionAction handles
        // pulling anyone who drifts too far back toward the centre.
        if (dynamic_cast<FleeAction*>(action))
        {
            return 0.0f;
        }

        // A disk rider's chase actions steer the disk, not the bot, and ReachCombatTo runs its
        // endpoint through UpdateAllowedPositionZ - which clamps to the platform floor, so the disk
        // dives 25y and MalygosRideDiskAction has to climb back up. Nothing but the EoE actions may
        // move a rider, plus a chatted "leave vehicle" as a manual override. Note AttackAction
        // derives from MovementAction, so the EoE actions have to be named or they go too.
        if (bot->GetVehicle() && !dynamic_cast<MalygosRideDiskAction*>(action) &&
            !dynamic_cast<MalygosTargetAction*>(action) && !dynamic_cast<LeaveVehicleAction*>(action) &&
            (dynamic_cast<MovementAction*>(action) || dynamic_cast<CastReachTargetSpellAction*>(action)))
        {
            return 0.0f;
        }

        // Ranged and healers hold their bubble and only hit what is already in reach, so nothing may
        // chase them out of shelter. Melee still have to reach the Nexus Lords on the ground, so they
        // stay free to move.
        // Closing on someone to heal them is the exception: the bubble round-robin spreads the raid
        // out and the disk riders leave the ground entirely, so a healer that can't walk to a
        // raider out of range simply never heals them.
        if (!bot->GetVehicle() && (botAI->IsRanged(bot) || botAI->IsHeal(bot)) &&
            !dynamic_cast<ReachPartyMemberToHealAction*>(action) &&
            (dynamic_cast<ReachTargetAction*>(action) || dynamic_cast<FollowAction*>(action)))
        {
            return 0.0f;
        }

        if (dynamic_cast<TankAssistAction*>(action))
        {
            Unit* target = action->GetTarget();
            if (target && target->GetEntry() == NPC_SCION_OF_ETERNITY)
            {
                return 0.0f;
            }
        }
    }
    else if (phase == 3)
    {
        // Suppresses FollowAction as well as attack-driven chase movement, but leaves the
        // drake flight and the (non-MovementAction) avoid actions free to run.
        if (dynamic_cast<MovementAction*>(action) && !dynamic_cast<EoEFlyDrakeAction*>(action))
        {
            return 0.0f;
        }

        // The follow formation would fly the drake straight back into the Static Field the avoid
        // action just cleared - the master's drake is as likely to be parked in one as anybody.
        if (dynamic_cast<EoEFlyDrakeAction*>(action) && IsStaticFieldNear(bot))
        {
            return 0.0f;
        }
    }
    else if (phase == 4)
    {
        // Phase transition: Malygos is untargetable and the raid isn't mounted yet. Hold the
        // gather at centre and stay off the default strategy so nobody chases into the void.
        if (dynamic_cast<FollowAction*>(action))
        {
            return 0.0f;
        }

        if (dynamic_cast<DpsAssistAction*>(action) || dynamic_cast<TankAssistAction*>(action))
        {
            return 0.0f;
        }

        if (dynamic_cast<MovementAction*>(action) && !dynamic_cast<MalygosPositionAction*>(action))
        {
            return 0.0f;
        }
    }

    return 1.0f;
}
