/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "EoEMultipliers.h"
#include "ChooseTargetActions.h"
#include "EoEActions.h"
#include "EoEData.h"
#include "EoEEncounter_Malygos.h"
#include "FollowActions.h"
#include "GenericSpellActions.h"
#include "HunterActions.h"
#include "MageActions.h"
#include "MovementActions.h"
#include "ReachTargetActions.h"
#include "Timer.h"
#include "VehicleActions.h"

#include <string>

namespace
{
// A bot's role does not change mid-fight, and re-deriving it per action was the most
// expensive thing this strategy did.
constexpr uint32 EOE_SNAPSHOT_CACHE_MS = 500;

// Every taunt the tank specs wire up, radius ones included. Matched by name because the six of them
// live in four class headers the EoE strategy has no other reason to pull in.
bool IsTauntAction(std::string const& name)
{
    return name == "taunt" || name == "hand of reckoning" || name == "dark command" ||
           name == "growl" || name == "challenging shout" || name == "challenging roar";
}
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

    Unit* boss = GetMalygos(bot);
    Unit* victim = boss ? boss->GetVictim() : nullptr;
    isBossVictim = victim == bot;
    isBossTank = isMainTank || isBossVictim;

    Player* victimPlayer = victim ? victim->ToPlayer() : nullptr;
    bossVictimIsTank = victimPlayer && botAI->IsTank(victimPlayer);
}

float MalygosMultiplier::GetValue(Action* action)
{
    uint8 const phase = GetMalygosPhase(bot);
    if (phase == 0)
    {
        return 1.0f;
    }

    RefreshSnapshot();

    // Everything suppressed here is either a MovementAction or a CastSpellAction, and the two
    // families are disjoint - so one cast each way beats walking a list of thirteen.
    MovementAction* move = dynamic_cast<MovementAction*>(action);
    CastSpellAction* cast = move ? nullptr : dynamic_cast<CastSpellAction*>(action);

    if (phase == 1)
    {
        if (cast)
        {
            // Arcane Breath is a frontal cone on whoever Malygos is hitting, so an off-tank taunting
            // from the melee stack turns him inward and sweeps it through the raid. Gated on a tank
            // already holding him: with nobody on him the taunt is the rescue, so it has to survive.
            if (!isMainTank && bossVictimIsTank && IsTauntAction(cast->getName()))
            {
                return 0.0f;
            }

            // "enemy too close for spell" stays active next to a 20-reach boss, and every class wires it
            // to an escape at 34-50 relevance, above MalygosPositionAction.
            if (dynamic_cast<CastBlinkBackAction*>(cast) || dynamic_cast<CastDisengageAction*>(cast))
            {
                return 0.0f;
            }

            // Closing on a heal target is the one exception to the movement lockout below.
            if (!isBossTank && dynamic_cast<CastReachTargetSpellAction*>(cast))
            {
                return 0.0f;
            }

            return 1.0f;
        }

        if (!move)
        {
            // Any dps may be holding a Power Spark rather than the boss.
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

        // He parks ~21y short of his victim, just outside what ReachMeleeAction calls melee, so this
        // would walk the tank in every tick while the position action walks him back out.
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
        // AttackAction derives from MovementAction, so the EoE actions have to be named or they go
        // with it. Closing on a heal target is the one exception, same as in P2.
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
            // A chase that casts on arrival steers the disk exactly like a chase that walks.
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

        // Keep the generic flee from walking bots off the edge; MalygosPositionAction recentres.
        if (dynamic_cast<FleeAction*>(move))
        {
            return 0.0f;
        }

        // A rider's chase actions steer the disk, and ReachCombatTo clamps its endpoint to the
        // platform floor, so the disk dives. AttackAction derives from MovementAction, so the EoE
        // actions have to be named or they go too.
        if (onVehicle && !dynamic_cast<MalygosRideDiskAction*>(move) &&
            !dynamic_cast<MalygosTargetAction*>(move) && !dynamic_cast<LeaveVehicleAction*>(move))
        {
            return 0.0f;
        }

        // Nothing may chase ranged or healers out of shelter; melee still walk to the Nexus Lords.
        // Closing on someone to heal them is the exception, or a raider out of range never gets one.
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
        // EoEFlyDrakeAction is the only thing allowed to steer a Skytalon. The drake rotation and the
        // surge shield are plain Actions, so they are untouched.
        if (move && !dynamic_cast<EoEFlyDrakeAction*>(move))
        {
            return 0.0f;
        }
    }
    else if (phase == 4)
    {
        // Phase transition: hold the gather at centre and stay off the default strategy.
        if (move && !dynamic_cast<MalygosPositionAction*>(move))
        {
            return 0.0f;
        }
    }

    return 1.0f;
}
