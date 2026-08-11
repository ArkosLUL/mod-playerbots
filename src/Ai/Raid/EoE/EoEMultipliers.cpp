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
#include "Timer.h"
#include "VehicleActions.h"
#include "WarriorActions.h"

namespace
{
// A bot's role does not change mid-fight, and re-deriving it per action was the most expensive
// thing this strategy did.
constexpr uint32 EOE_SNAPSHOT_CACHE_MS = 500;
}

void MalygosMultiplier::RefreshSnapshot()
{
    uint32 const now = getMSTime();
    if (snapshotAtMs && getMSTimeDiff(snapshotAtMs, now) < EOE_SNAPSHOT_CACHE_MS)
    {
        return;
    }
    snapshotAtMs = now;

    isMainTank = botAI->IsMainTank(bot);
    isDps = botAI->IsDps(bot);
    isRanged = botAI->IsRanged(bot);
    isHeal = botAI->IsHeal(bot);

    Unit* boss = MalygosTrigger::getMalygos(bot);
    isBossVictim = boss && boss->GetVictim() == bot;
    isBossTank = isMainTank || isBossVictim;
}

float MalygosMultiplier::GetValue(Action* action)
{
    uint8 const phase = MalygosTrigger::getPhase(bot);
    if (phase == 0) { return 1.0f; }

    RefreshSnapshot();

    // Everything this multiplier suppresses is either a MovementAction or a CastSpellAction, and
    // the two families are disjoint. Resolving which one an action belongs to up front means a
    // plain rotation cast pays two dynamic_casts instead of walking the whole list of thirteen.
    MovementAction* move = dynamic_cast<MovementAction*>(action);
    CastSpellAction* cast = move ? nullptr : dynamic_cast<CastSpellAction*>(action);

    if (phase == 1)
    {
        if (cast)
        {
            // Malygos' CombatReach of 20 keeps "enemy too close for spell" active for anyone holding
            // a spot near him, and every class wires that trigger to an escape somewhere between 34
            // and 50 relevance - all of it above MalygosPositionAction. Left alone the bot steps out,
            // gets dragged back next tick and never finishes a cast.
            if (dynamic_cast<CastBlinkBackAction*>(cast) || dynamic_cast<CastDisengageAction*>(cast))
            {
                return 0.0f;
            }

            // Closing on a heal target is the one exception to the movement lockout below, same as
            // in P2, so the spell half of that lockout only bites for non-tanks.
            if (!isBossTank && dynamic_cast<CastReachTargetSpellAction*>(cast))
            {
                return 0.0f;
            }

            return 1.0f;
        }

        if (!move)
        {
            // Any dps may be holding a Power Spark rather than the boss; dropping it hands the spark
            // its walk to Malygos back.
            if (isDps && dynamic_cast<DropTargetAction*>(action))
            {
                return 0.0f;
            }

            return 1.0f;
        }

        if (dynamic_cast<FollowAction*>(move))
        {
            return 0.0f;
        }

        // Malygos has a CombatReach of 20, so he parks about 21y short of his victim - just outside
        // what ReachMeleeAction considers "in melee". Left alone it walks the tank into the boss
        // every tick while MalygosPositionAction walks it back out, and the boss pivots between the
        // two, raking the Arcane Breath cone across the raid. The boss closes the gap himself, so
        // once the tank has aggro it simply holds the spot.
        if (isBossVictim && dynamic_cast<ReachTargetAction*>(move))
        {
            return 0.0f;
        }

        if (dynamic_cast<FleeAction*>(move) || dynamic_cast<RunAwayAction*>(move))
        {
            return 0.0f;
        }

        if (isDps && dynamic_cast<DpsAssistAction*>(move))
        {
            return 0.0f;
        }

        // Nobody but the boss' current victim walks anywhere in P1 under their own steam.
        // Ranged were chasing Power Sparks back inside Malygos' minimum range; melee were walking
        // out to get behind him (set behind) or to spread (combat formation move) and being dragged
        // back to the stack next tick, which is the shuffling that shows up in game. The hold spots
        // already sit inside his 20y combat reach for melee and outside the inflated minimum range
        // for hunters, and the DK grips sparks to the raid, so there is nothing left to walk to.
        // Note AttackAction derives from MovementAction, so the EoE actions have to be named or they
        // go with it. Closing on a heal target is the one exception, same as in P2.
        if (!isBossTank && !dynamic_cast<MalygosPositionAction*>(move) &&
            !dynamic_cast<MalygosTargetAction*>(move) && !dynamic_cast<KillPowerSparkAction*>(move) &&
            !dynamic_cast<ReachPartyMemberToHealAction*>(move))
        {
            return 0.0f;
        }

        if (!isMainTank && dynamic_cast<TankAssistAction*>(move))
        {
            return 0.0f;
        }
    }
    else if (phase == 2)
    {
        bool const onVehicle = bot->GetVehicle() != nullptr;

        if (cast)
        {
            // Same two lockouts as the movement ones below - a chase that casts on arrival steers
            // the disk exactly like a chase that walks.
            if (!dynamic_cast<CastReachTargetSpellAction*>(cast))
            {
                return 1.0f;
            }

            if (onVehicle)
            {
                return 0.0f;
            }

            return 1.0f;
        }

        if (!move)
        {
            return 1.0f;
        }

        if (isDps && dynamic_cast<DpsAssistAction*>(move))
        {
            return 0.0f;
        }

        // Keep the generic flee from walking bots off the edge; MalygosPositionAction handles
        // pulling anyone who drifts too far back toward the centre.
        if (dynamic_cast<FleeAction*>(move))
        {
            return 0.0f;
        }

        // A disk rider's chase actions steer the disk, not the bot, and ReachCombatTo runs its
        // endpoint through UpdateAllowedPositionZ - which clamps to the platform floor, so the disk
        // dives 25y and MalygosRideDiskAction has to climb back up. Nothing but the EoE actions may
        // move a rider, plus a chatted "leave vehicle" as a manual override. Note AttackAction
        // derives from MovementAction, so the EoE actions have to be named or they go too.
        if (onVehicle && !dynamic_cast<MalygosRideDiskAction*>(move) &&
            !dynamic_cast<MalygosTargetAction*>(move) && !dynamic_cast<LeaveVehicleAction*>(move))
        {
            return 0.0f;
        }

        // Ranged and healers hold their bubble and only hit what is already in reach, so nothing may
        // chase them out of shelter. Melee still have to reach the Nexus Lords on the ground, so they
        // stay free to move.
        // Closing on someone to heal them is the exception: the bubble round-robin spreads the raid
        // out and the disk riders leave the ground entirely, so a healer that can't walk to a
        // raider out of range simply never heals them.
        if (!onVehicle && (isRanged || isHeal) && !dynamic_cast<ReachPartyMemberToHealAction*>(move) &&
            (dynamic_cast<ReachTargetAction*>(move) || dynamic_cast<FollowAction*>(move)))
        {
            return 0.0f;
        }

        if (dynamic_cast<TankAssistAction*>(move))
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
        // EoEFlyDrakeAction is the only thing allowed to steer a Skytalon; everything else that
        // moves - FollowAction, attack-driven chase - would fight it for the same MotionMaster.
        // The drake rotation and the surge shield are plain Actions, so they are untouched.
        if (move && !dynamic_cast<EoEFlyDrakeAction*>(move))
        {
            return 0.0f;
        }
    }
    else if (phase == 4)
    {
        // Phase transition: Malygos is untargetable and the raid isn't mounted yet. Hold the
        // gather at centre and stay off the default strategy so nobody chases into the void.
        if (move && !dynamic_cast<MalygosPositionAction*>(move))
        {
            return 0.0f;
        }
    }

    return 1.0f;
}
