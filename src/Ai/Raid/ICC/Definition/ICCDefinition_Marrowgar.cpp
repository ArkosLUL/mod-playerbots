/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "ICCActions_Marrowgar.h"
#include "ICCEncounterGate.h"
#include "ICCHelpers_Marrowgar.h"
#include "ICCSeams.h"
#include "ICCTriggers_Marrowgar.h"
#include "RaidDefensive.h"
#include "RaidTankDefensive.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
void DefineMarrowgar(EncounterBuilder& e)
{
    e.Node<IccLmColdflameTrigger, IccLmDodgeColdflameAction>(ACTION_EMERGENCY + 6, EncounterRow::Mover);
    e.Node<IccLmStormCoverTrigger, IccLmCastStormCoverAction>(ACTION_RAID + 7);
    e.Node<IccLmTankDefensiveTrigger, IccLmCastTankDefensiveAction>(ACTION_RAID + 6);
    // Above the stands: a bot takes a new spike before it walks back to its spot
    e.Node<IccLmTargetTrigger, IccLmAttackTargetAction>(ACTION_RAID + 5);
    e.Node<IccLmStandTrigger, IccLmMoveToStandAction>(ACTION_RAID + 4, EncounterRow::Mover);

    e.OwnTargeting("icc lm targeting", Role::Any, &IccResolverPicks<&GetLmTarget>);
    e.OwnMovement("icc lm stand hold", Role::Any, &IccLmHoldsNormalStand,
                  Family::Attack | Family::Reach | Family::ReachHeal);
    // Nobody chases him through a storm: reach would walk the stack and the anchors into his charges
    e.OwnMovement("icc lm storm hold", Role::Any, &IccLmHoldsStormStand, Family::Attack | Family::ReachHeal,
                  {IccLmMoveToStandAction::Name, IccLmDodgeColdflameAction::Name, "reach party member to resurrect"});
    e.Block("icc lm spell movers", Role::Any, &IccLmHoldsStand, Family::Charge | Family::Blink | Family::Disengage);
    // avoid aoe sees the Coldflame patches too and flees them 5 yd at most, against the dodge
    e.Block("icc lm avoid aoe", Role::Any, &IsLmLive, Family::AvoidAoe);

    // The tank buttons wait for the Bone Slice window, the raid cooldowns for the storm window
    e.Block("icc lm tank defensive hold", Role::Tank, &IsLmTankDefensiveHeld, 0, HeldTankDefensiveNames());
    e.Block("icc lm storm cover hold", Role::Any, &IsLmStormCoverHeld, 0, RaidCoverClassNodes());

    e.Tick(&TickLmEncounter);
}
}  // namespace

EncounterDefinition const& IccMarrowgarDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_LORD_MARROWGAR, BossStateGate, "lord-marrowgar",
                                                &DefineMarrowgar);
    return definition;
}
