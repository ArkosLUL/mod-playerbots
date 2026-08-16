/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "OSMultipliers.h"
#include "BurstCooldowns.h"
#include "CheckMountStateAction.h"
#include "ChooseTargetActions.h"
#include "DKActions.h"
#include "DruidActions.h"
#include "DruidBearActions.h"
#include "DruidShapeshiftActions.h"
#include "FollowActions.h"
#include "GenericActions.h"
#include "GenericSpellActions.h"
#include "HunterActions.h"
#include "MageActions.h"
#include "MovementActions.h"
#include "OSActions.h"
#include "OSHelpers.h"
#include "OSTriggers.h"
#include "PaladinActions.h"
#include "ReachTargetActions.h"
#include "RogueActions.h"
#include "ScriptedCreature.h"
#include "WarriorActions.h"

using namespace OsHelpers;

namespace
{

bool IsGenericMover(Action* action)
{
    return dynamic_cast<ReachTargetAction*>(action) || dynamic_cast<CastReachTargetSpellAction*>(action) ||
           dynamic_cast<CombatFormationMoveAction*>(action) || dynamic_cast<RearFlankAction*>(action) ||
           dynamic_cast<FollowAction*>(action);
}

// Idle wander and panic flight. Nothing in this fight wants either, and they are what turns a bot
// that has drifted off the arena into a bot halfway across the zone.
bool IsWanderMover(Action* action)
{
    return dynamic_cast<MoveRandomAction*>(action) || dynamic_cast<RunAwayAction*>(action) ||
           dynamic_cast<FleeAction*>(action);
}

// Formation spacing and party follow have no business overriding a scripted hold.
bool IsFormationMover(Action* action)
{
    return dynamic_cast<CombatFormationMoveAction*>(action) || dynamic_cast<FollowAction*>(action);
}

// Everything that can move a bot on its own. The realm rule below has no snapshot to lean on, so it names
// the lot rather than the in-combat subset.
bool IsAnyMover(Action* action)
{
    return IsGenericMover(action) || IsWanderMover(action) ||
           dynamic_cast<MoveOutOfCollisionAction*>(action) ||
           dynamic_cast<MoveOutOfEnemyContactAction*>(action);
}

}

SartharionMultiplier::TickState const& SartharionMultiplier::Snapshot()
{
    uint32 const now = getMSTime();
    if (now == cachedAtMs && cachedAtMs)
        return cached;

    cachedAtMs = now;
    cached = TickState();
    cached.encounterActive = SartharionEncounterActive(bot);
    if (!cached.encounterActive)
        return cached;

    cached.boss = GetSartharion(bot);
    // requireSelectable off, same as the trigger: the fissure carries that flag for its whole life.
    cached.dodgeLive = ClassifyTsunamiWave(bot) != TsunamiWave::None ||
                       FindUnitByEntries(bot, { NpcId::TwilightFissure, NpcId::TwilightFissureH },
                                         FISSURE_CLEAR_RADIUS, false) != nullptr;
    return cached;
}

float SartharionMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    // Ahead of the snapshot, which is empty in the Twilight Realm: the boss is unresolvable in phase 16,
    // so every rule below is skipped there. This one still has to hold. With the adds dead the bot is idle
    // and stacked, and the hold releases the tick on its own duplicate-move guard, so the collision step
    // and the idle wander would walk it back into a lane before the shift is stripped.
    if (TwilightRealmWaveWait(bot) && IsAnyMover(action))
        return 0.0f;

    TickState const& state = Snapshot();
    if (!state.encounterActive)
        return 1.0f;

    Unit* boss = state.boss;
    Unit* target = action->GetTarget();

    if (IsWanderMover(action))
        return 0.0f;

    if (botAI->IsDps(bot) && dynamic_cast<DpsAssistAction*>(action))
        return 0.0f;

    if (botAI->IsMainTank(bot) && target && target != boss &&
        (dynamic_cast<TankAssistAction*>(action) || dynamic_cast<CastTauntAction*>(action) ||
         dynamic_cast<CastDarkCommandAction*>(action) || dynamic_cast<CastHandOfReckoningAction*>(action) ||
         dynamic_cast<CastGrowlAction*>(action)))
    {
        return 0.0f;
    }

    // Sartharion belongs to the main tank alone: two tanks trading aggro spin him through the raid.
    // So the off-tank is locked off him for the whole fight, not merely off the taunts. The concrete
    // target-picking classes are named one by one on purpose - OsOffTankHoldAction is itself an
    // AttackAction and GetTarget() reports the bot's current target, so casting to the base would
    // zero the one action that can switch the off-tank away and strand it on the boss.
    if (botAI->IsAssistTank(bot) && target && target == boss &&
        (dynamic_cast<CastTauntAction*>(action) || dynamic_cast<CastDarkCommandAction*>(action) ||
         dynamic_cast<CastHandOfReckoningAction*>(action) || dynamic_cast<CastGrowlAction*>(action) ||
         dynamic_cast<TankAssistAction*>(action) || dynamic_cast<AggressiveTargetAction*>(action) ||
         dynamic_cast<AttackAnythingAction*>(action)))
    {
        return 0.0f;
    }

    // The OS node owns Tricks and Misdirection end to end - main tank on the pull, off-tank once the
    // adds are up, main tank again after. The generic node only ever knows the main tank, so it stays
    // off the whole fight. Casting on the concrete actions, never on the shared BuffOnMainTankAction
    // base, which also carries Beacon, Earth Shield, Thorns and Lifebloom.
    if (dynamic_cast<CastTricksOfTheTradeOnMainTankAction*>(action) ||
        dynamic_cast<CastMisdirectionOnMainTankAction*>(action))
    {
        return 0.0f;
    }

    // The class nodes hang these off a bare health trigger, which in this fight spends a 5-minute
    // Shield Wall on the first Flame Breath and leaves nothing for the stretch that kills him.
    // "os main tank cooldown" owns the order instead, and it can only own it if nothing else casts them.
    if (botAI->IsMainTank(bot) && IsHeldTankDefensive(action->getName()))
        return 0.0f;

    // Scoped to a live dodge only. Held permanently this is the freeze bug: with the generic movers
    // off, a silently-failing MoveTo strands the bot for the rest of the fight. Outside the dodge
    // window melee and the off-tank chase drakes normally.
    if (state.dodgeLive && IsGenericMover(action))
        return 0.0f;

    // Nothing generic may move the off-tank in this fight. "os offtank hold" and "os drake landing
    // position" produce every position he needs and both clamp; he taunts drakes, Lava Blazes and
    // whelps alike from wherever he is standing. Left open against a drake this is a reach action
    // walking him onto Vesperon's landing coord, which is past the east edge of the platform.
    if (IsOffTank(bot) && IsGenericMover(action))
        return 0.0f;

    // Once the tank is on the boss, nothing else may move him. "os main tank hold" releases the tick
    // as soon as it is inside its tolerance, the reach and formation movers then nudge him out of it,
    // and the hold drags him back next tick - that is the oscillation. Gated on melee range rather
    // than on the encounter so the pull still closes the gap normally, and on the drag as well: the
    // corner is 20.83yd from where the boss settles, so nothing is in melee range while the tank is
    // standing out his three seconds there.
    if (botAI->IsMainTank(bot) && boss && (bot->IsWithinMeleeRange(boss) || !MainTankDragDone(boss)) &&
        IsGenericMover(action))
    {
        return 0.0f;
    }

    // Same fight, ranged edition. The reach actions stay live on purpose: "os raid hold" owns the
    // line, but a drake parked at the east end of the pile still has to be closed on.
    if ((botAI->IsRanged(bot) || botAI->IsHeal(bot)) && IsFormationMover(action))
        return 0.0f;

    // The generic melee arc is wrong against both: Sartharion's rear is Tail Lash, and a drake's rear
    // is free ground the 90-120 degree band stops short of. "os sartharion flank" and "os drake rear"
    // replace it. It has to be zeroed rather than left to lose on priority - the two above release the
    // tick once the bot is in position, and this would then walk it back out. Lava Blazes and whelps
    // keep it.
    if (target && (target == boss || IsDrakeEntry(target->GetEntry())) &&
        dynamic_cast<RearFlankAction*>(action))
    {
        return 0.0f;
    }

    // A druid tank out of bear form is wearing cloth. Out of combat - which the off-tank is between
    // drakes - every druid buff, heal and revive node pulls "caster form" as a prerequisite, and that
    // action is a bare RemoveShapeshift. Tanks only: a cat-spec druid needs caster form for Rebirth.
    if (botAI->IsTank(bot) && dynamic_cast<CastCasterFormAction*>(action))
        return 0.0f;

    // Same window, and it costs the form even when it fails: Mount() calls RemoveShapeshift before it
    // casts anything, and mounting inside the instance never succeeds.
    if (dynamic_cast<CheckMountStateAction*>(action))
        return 0.0f;

    // Both are unclamped 20yd displacements away from the current target, with no idea the platform
    // ends. Every destination this strategy issues goes through ClampDestination; these two do not,
    // and there is nowhere here that 20yd of blind travel is survivable.
    if (dynamic_cast<CastBlinkBackAction*>(action) || dynamic_cast<CastDisengageAction*>(action))
        return 0.0f;

    // Three more that clamp nothing, and "avoid aoe" outranks every hold at ACTION_EMERGENCY. What it
    // flees is the fissure and the tsunami, which are the two things this strategy already dodges with
    // clamped, corridor-aware destinations. The collision step is a random bearing taken whenever a
    // parked bot is stacked with another, which here is the whole raid by design, and backing out of
    // enemy contact is what "os drake rear" and the holds do properly.
    if (dynamic_cast<AvoidAoeAction*>(action) || dynamic_cast<MoveOutOfCollisionAction*>(action) ||
        dynamic_cast<MoveOutOfEnemyContactAction*>(action))
    {
        return 0.0f;
    }

    // Sartharion is the one target these three never walk to. He parks 36.8yd from the raid's home hold
    // and 50.5yd from its left one, both outside their 28.5yd spell range, and closing that gap means
    // walking in behind him into a 30yd Tail Lash. The off-tank is on the list for a different reason:
    // he is locked off the boss for the whole fight, and the pull leaves the boss in his "current
    // target" before this strategy is live, which is enough for a reach mover to carry him into melee.
    if ((botAI->IsRanged(bot) || botAI->IsHeal(bot) || IsOffTank(bot)) && boss && target == boss &&
        IsGenericMover(action))
    {
        return 0.0f;
    }

    return 1.0f;
}

float SartharionBurstWindowMultiplier::GetValue(Action* action)
{
    if (!action || !IsBurstCooldownAction(action->getName()))
        return 1.0f;

    uint32 const now = getMSTime();
    if (now != cachedAtMs || !cachedAtMs)
    {
        cachedAtMs = now;
        cachedValue = EvaluateWindow();
    }
    return cachedValue;
}

float SartharionBurstWindowMultiplier::EvaluateWindow()
{
    Unit* boss = GetSartharion(bot);
    if (!boss || !boss->IsInCombat())
        return 1.0f;

    // Hard interlock, not a tiebreaker: Gift of Twilight is a full-school damage immunity, so a
    // Bloodlust fired under it is thrown away entirely.
    if (SartharionDamageImmune(bot))
        return 0.0f;

    if (boss->HasAura(SpellId::SartharionBerserk) || boss->GetHealthPct() <= SARTHARION_ENRAGE_PCT)
        return 1.0f;

    return SartharionBurstWindowOpen(bot) ? 1.0f : 0.0f;
}
