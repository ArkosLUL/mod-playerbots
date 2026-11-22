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
    e.Node<OsOffPlatformTrigger, OsReturnToPlatformAction>(ACTION_EMERGENCY + 2);
    e.Node<OsTwilightFissureTrigger, OsAvoidTwilightFissureAction>(ACTION_EMERGENCY + 1);
    e.Node<OsTsunamiCorridorTrigger, OsTsunamiCorridorAction>(ACTION_EMERGENCY);

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

    e.Block("sartharion", Role::Any, InRealmWaitingOutWave,
            GENERIC_MOVERS | WANDER_MOVERS | Family::MoveOutOfCollision | Family::MoveOutOfEnemyContact);

    e.Block("sartharion", Role::Any, EncounterActive, WANDER_MOVERS);

    e.OwnTargeting("sartharion", Role::Dps, EncounterActive, Family::DpsAssist);
    e.Multiplier<SartharionMainTankTargetMultiplier>(Family::TankAssist | Family::Taunt);
    e.Multiplier<SartharionOffTankBossMultiplier>(Family::Taunt | Family::TankAssist | Family::AggressiveTarget |
                                                  Family::AttackAnything);

    e.Block("sartharion", Role::Any, EncounterActive, 0,
            {"tricks of the trade on main tank", "misdirection on main tank"});

    e.Block("sartharion", Role::MainTank, EncounterActive, 0, HeldTankDefensiveNames());

    e.Block("sartharion", Role::Any, DodgeLive, GENERIC_MOVERS);

    e.Block("sartharion", Role::Any, OffTankInFight, GENERIC_MOVERS);

    e.Block("sartharion", Role::MainTank, MainTankOnBoss, GENERIC_MOVERS);

    e.Block("sartharion", Role::Ranged | Role::Heal, EncounterActive, FORMATION_MOVERS);

    e.Multiplier<SartharionRearFlankMultiplier>(Family::RearFlank);

    e.Block("sartharion", Role::Tank, EncounterActive, 0, {"caster form"});

    e.Block("sartharion", Role::Any, EncounterActive, 0, {"check mount state"});

    e.Block("sartharion", Role::Any, EncounterActive, Family::Blink | Family::Disengage);

    e.Block("sartharion", Role::Any, EncounterActive,
            Family::AvoidAoe | Family::MoveOutOfCollision | Family::MoveOutOfEnemyContact);

    e.Multiplier<SartharionBossReachMultiplier>(GENERIC_MOVERS);

    e.Exclusive("os mechanic priority", Role::Any, OffPlatformLive, 0, {OsReturnToPlatformAction::Name});
    e.Exclusive("os mechanic priority", Role::Any, TsunamiLive, 0, {OsTsunamiCorridorAction::Name});
    e.Exclusive("os mechanic priority", Role::Any, FissureLive, 0, {OsAvoidTwilightFissureAction::Name});

    e.Multiplier<SartharionBurstWindowMultiplier>(Family::AnyAction);

    e.Tick(TickSartharion);
}
}

EncounterDefinition const& OsSartharionDefinition()
{
    static EncounterDefinition const definition(OS_BOSS_SARTHARION, BossStateGate, "sartharion", &DefineSartharion);
    return definition;
}
