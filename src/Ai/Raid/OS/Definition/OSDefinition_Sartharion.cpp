/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "OSDefinitions.h"

#include "OSActions.h"
#include "OSHelpers.h"
#include "OSMultipliers.h"
#include "OSTriggers.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "RaidTankDefensive.h"
#include "Strategy.h"
#include "Timer.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

using namespace OsHelpers;

namespace
{
constexpr RaidEncounterRules::FamilyMask FORMATION_MOVERS =
    Family::CombatFormationMove | Family::TankFace | Family::SetBehind | Family::Follow;
constexpr RaidEncounterRules::FamilyMask GENERIC_MOVERS =
    FORMATION_MOVERS | Family::Reach | Family::ReachHeal | Family::Charge | Family::RearFlank;
constexpr RaidEncounterRules::FamilyMask WANDER_MOVERS = Family::MoveRandom | Family::RunAway | Family::Flee;

bool EncounterActive(PlayerbotAI* botAI) { return SartharionSnapshotFor(botAI).encounterActive; }
bool DodgeLive(PlayerbotAI* botAI) { return SartharionSnapshotFor(botAI).dodgeLive; }
bool OffTankInFight(PlayerbotAI* botAI) { return EncounterActive(botAI) && IsOffTank(botAI->GetBot()); }
bool InRealmWaitingOutWave(PlayerbotAI* botAI) { return TwilightRealmWaveWait(botAI->GetBot()); }

bool MainTankOnBoss(PlayerbotAI* botAI)
{
    SartharionSnapshot const& state = SartharionSnapshotFor(botAI);
    return state.encounterActive && state.boss &&
           (botAI->GetBot()->IsWithinMeleeRange(state.boss) || !MainTankDragDone(state.boss));
}

enum class Mechanic : uint8
{
    None,
    Fissure,
    Tsunami,
    OffPlatform
};

// The same three predicates the triggers run, in precedence order. Sharing them is what keeps the
// rules below from suppressing every mover for a mechanic whose own action would never fire. They
// sweep for tsunamis and fissures, so the answer is kept for the rest of this bot's tick.
Mechanic LiveMechanic(PlayerbotAI* botAI)
{
    thread_local PlayerbotAI* cachedFor = nullptr;
    thread_local uint32 cachedAtMs = 0;
    thread_local Mechanic cached = Mechanic::None;

    uint32 const now = getMSTime();
    if (cachedFor == botAI && cachedAtMs == now && cachedAtMs)
        return cached;

    cachedFor = botAI;
    cachedAtMs = now;

    Player* bot = botAI->GetBot();
    if (NeedsPlatformReturn(botAI, bot))
        cached = Mechanic::OffPlatform;
    else if (NeedsTsunamiDodge(bot))
        cached = Mechanic::Tsunami;
    else if (NeedsFissureDodge(bot))
        cached = Mechanic::Fissure;
    else
        cached = Mechanic::None;

    return cached;
}

bool OffPlatformLive(PlayerbotAI* botAI) { return LiveMechanic(botAI) == Mechanic::OffPlatform; }
bool TsunamiLive(PlayerbotAI* botAI) { return LiveMechanic(botAI) == Mechanic::Tsunami; }
bool FissureLive(PlayerbotAI* botAI) { return LiveMechanic(botAI) == Mechanic::Fissure; }

void TickSartharion(PlayerbotAI* botAI) { TickSartharionObs(botAI->GetBot()); }

void DefineSartharion(EncounterBuilder& e)
{
    // Movement precedence, highest first. Left implicit these tie and the off-tank and melee
    // oscillate between the drake pile and the safe corridor.
    //
    // Getting back onto the arena sits above both dodges: off the platform nothing can hit the bot
    // and the bot can do nothing, so there is no mechanic left worth reacting to.
    //
    // The fissure dodge wins outright: smallest move, tightest fuse. The corridor dodge outranks
    // every hold, so the pile is abandoned mid-fight when a wave goes out - it is Y-only and keeps X,
    // so the west-to-east order re-forms on the new corridor. "rear flank" sits below both on
    // purpose: eating one Shadow Breath is survivable, standing in a tsunami is not. It also sits
    // below the two nodes that own the cases it gets wrong - Sartharion's rear is a cone of its own,
    // and a drake's is free ground its 90-120 degree band never reaches. It keeps the adds.
    e.Node<OsOffPlatformTrigger, OsReturnToPlatformAction>(ACTION_EMERGENCY + 2);
    e.Node<OsTwilightFissureTrigger, OsAvoidTwilightFissureAction>(ACTION_EMERGENCY + 1);
    e.Node<OsTsunamiCorridorTrigger, OsTsunamiCorridorAction>(ACTION_EMERGENCY);

    // Top of the RAID band, so a hold that keeps returning true cannot starve either of them, and
    // still under every dodge: a bear tank standing in a tsunami is no better off for being in form,
    // and neither is one under Survival Instincts. The cooldown goes first because it only fires at
    // all in the window the tank is dying in, and it costs one tick.
    e.Node<OsMainTankCooldownTrigger, OsMainTankCooldownAction>(ACTION_RAID + 7);
    e.Node<OsTankShapeshiftTrigger, OsTankShapeshiftAction>(ACTION_RAID + 6);
    e.Node<OsDrakeLandingTrigger, OsDrakeLandingPositionAction>(ACTION_RAID + 5);
    e.Node<OsOffTankHoldTrigger, OsOffTankHoldAction>(ACTION_RAID + 4);
    e.Node<OsTranquilizeTrigger, OsTranquilizeEnrageAction>(ACTION_RAID + 3);
    e.Node<OsRedirectThreatTrigger, OsRedirectThreatAction>(ACTION_RAID + 2);
    e.Node<OsMainTankHoldTrigger, OsMainTankHoldAction>(ACTION_RAID + 1);
    e.Node<OsRaidHoldTrigger, OsRaidHoldAction>(ACTION_RAID + 1);
    e.Node<TwilightPortalEnterTrigger, EnterTwilightPortalAction>(ACTION_RAID + 1);
    e.Node<OsDrakeRearTrigger, OsDrakeRearAction>(ACTION_MOVE + 6);
    e.Node<OsSartharionFlankTrigger, OsSartharionFlankAction>(ACTION_MOVE + 5);
    e.Node<SartharionMeleePositioningTrigger>("rear flank", ACTION_MOVE + 4);

    // Every other rule is off in the Twilight Realm, where the boss cannot be resolved in phase 16.
    // This one still has to hold there. With the adds dead the bot is idle and stacked, and the hold
    // releases the tick on its own duplicate-move guard, so the collision step and the idle wander
    // would walk it back into a lane before the shift is stripped.
    e.Block("sartharion", Role::Any, InRealmWaitingOutWave,
            GENERIC_MOVERS | WANDER_MOVERS | Family::MoveOutOfCollision | Family::MoveOutOfEnemyContact);

    // Idle wander and panic flight. Nothing in this fight wants either, and they are what turns a bot
    // that has drifted off the arena into a bot halfway across the zone.
    e.Block("sartharion", Role::Any, EncounterActive, WANDER_MOVERS);

    e.OwnTargeting("sartharion", Role::Dps, EncounterActive, Family::DpsAssist);
    e.Multiplier<SartharionMainTankTargetMultiplier>(Family::TankAssist | Family::Taunt);
    e.Multiplier<SartharionOffTankBossMultiplier>(Family::Taunt | Family::TankAssist | Family::AggressiveTarget |
                                                  Family::AttackAnything);

    // The OS node owns Tricks and Misdirection end to end - main tank on the pull, off-tank once the
    // adds are up, main tank again after. The generic node only ever knows the main tank, so it stays
    // off the whole fight.
    e.Block("sartharion", Role::Any, EncounterActive, 0,
            {"tricks of the trade on main tank", "misdirection on main tank"});

    // The class nodes hang these off a bare health trigger, which in this fight spends a 5-minute
    // Shield Wall on the first Flame Breath and leaves nothing for the stretch that kills him.
    // "os main tank cooldown" owns the order instead, and it can only own it if nothing else casts them.
    e.Block("sartharion", Role::MainTank, EncounterActive, 0, HeldTankDefensiveNames());

    // Scoped to a live dodge only. Held permanently this is the freeze bug: with the generic movers
    // off, a silently-failing MoveTo strands the bot for the rest of the fight. Outside the dodge
    // window melee and the off-tank chase drakes normally.
    e.Block("sartharion", Role::Any, DodgeLive, GENERIC_MOVERS);

    // Nothing generic may move the off-tank in this fight. "os offtank hold" and "os drake landing
    // position" produce every position he needs and both clamp; he taunts drakes, Lava Blazes and
    // whelps alike from wherever he is standing. Left open against a drake this is a reach action
    // walking him onto Vesperon's landing coord, which is past the east edge of the platform.
    e.Block("sartharion", Role::Any, OffTankInFight, GENERIC_MOVERS);

    // Once the tank is on the boss, nothing else may move him. "os main tank hold" releases the tick
    // as soon as it is inside its tolerance, the reach and formation movers then nudge him out of it,
    // and the hold drags him back next tick - that is the oscillation. Gated on melee range rather
    // than on the encounter so the pull still closes the gap normally, and on the drag as well: the
    // corner is 20.83yd from where the boss settles, so nothing is in melee range while the tank is
    // standing out his three seconds there.
    e.Block("sartharion", Role::MainTank, MainTankOnBoss, GENERIC_MOVERS);

    // Formation spacing and party follow have no business overriding a scripted hold. The reach
    // actions stay live on purpose: "os raid hold" owns the line, but a drake parked at the east end
    // of the pile still has to be closed on.
    e.Block("sartharion", Role::Ranged | Role::Heal, EncounterActive, FORMATION_MOVERS);

    e.Multiplier<SartharionRearFlankMultiplier>(Family::RearFlank);

    // A druid tank out of bear form is wearing cloth. Out of combat - which the off-tank is between
    // drakes - every druid buff, heal and revive node pulls "caster form" as a prerequisite, and that
    // action is a bare RemoveShapeshift. Tanks only: a cat-spec druid needs caster form for Rebirth.
    e.Block("sartharion", Role::Tank, EncounterActive, 0, {"caster form"});

    // Same window, and it costs the form even when it fails: Mount() calls RemoveShapeshift before it
    // casts anything, and mounting inside the instance never succeeds.
    e.Block("sartharion", Role::Any, EncounterActive, 0, {"check mount state"});

    // Both are unclamped 20yd displacements away from the current target, with no idea the platform
    // ends. Every destination this strategy issues goes through ClampDestination; these two do not,
    // and there is nowhere here that 20yd of blind travel is survivable.
    e.Block("sartharion", Role::Any, EncounterActive, Family::Blink | Family::Disengage);

    // Three more that clamp nothing, and "avoid aoe" outranks every hold at ACTION_EMERGENCY. What it
    // flees is the fissure and the tsunami, which are the two things this strategy already dodges with
    // clamped, corridor-aware destinations. The collision step is a random bearing taken whenever a
    // parked bot is stacked with another, which here is the whole raid by design, and backing out of
    // enemy contact is what "os drake rear" and the holds do properly.
    e.Block("sartharion", Role::Any, EncounterActive,
            Family::AvoidAoe | Family::MoveOutOfCollision | Family::MoveOutOfEnemyContact);

    e.Multiplier<SartharionBossReachMultiplier>(GENERIC_MOVERS);

    // Settles which of the three emergency dodges owns the tick. All three issue at MOVEMENT_FORCED so
    // they can preempt a hold's movement lock, but that ladder cannot rank them against each other -
    // IsWaitingForLastMove compares with a strict >, so FORCED never beats FORCED. So each mechanic
    // passes its own action and zeroes every other mover while it is live.
    //
    // Off the platform beats a tsunami beats a fissure. Standing off the platform is the only one of
    // the three that does not fix itself - the bot is in lava and no hold will walk it back - a tsunami
    // is lethal on contact, and a Void Blast is survivable.
    e.Exclusive("os mechanic priority", Role::Any, OffPlatformLive, 0, {OsReturnToPlatformAction::Name});
    e.Exclusive("os mechanic priority", Role::Any, TsunamiLive, 0, {OsTsunamiCorridorAction::Name});
    e.Exclusive("os mechanic priority", Role::Any, FissureLive, 0, {OsAvoidTwilightFissureAction::Name});

    e.Multiplier<SartharionBurstWindowMultiplier>(Family::AnyAction);

    e.Tick(TickSartharion);
}
}  // namespace

EncounterDefinition const& OsSartharionDefinition()
{
    static EncounterDefinition const definition(OS_BOSS_SARTHARION, BossStateGate, "sartharion", &DefineSartharion);
    return definition;
}
