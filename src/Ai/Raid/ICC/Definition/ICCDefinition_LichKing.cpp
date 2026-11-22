/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "ICCActions_LichKing.h"
#include "ICCActions_LichKingLate.h"
#include "ICCEncounterGate.h"
#include "ICCHelpers_LichKing.h"
#include "ICCHelpers_LichKingLate.h"
#include "ICCMultipliers_LichKing.h"
#include "ICCSeams.h"
#include "ICCTriggers_LichKing.h"
#include "ICCTriggers_LichKingLate.h"
#include "RaidTankDefensive.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
bool IsLkAnyMovementOwned(PlayerbotAI* botAI) { return IsLkMovementOwned(botAI) || IsLkLateMovementOwned(botAI); }

void TickLichKing(PlayerbotAI* botAI)
{
    TickLkEncounter(botAI);
    TickLkLate(botAI);
}

void DefineLichKing(EncounterBuilder& e)
{
    e.Node<IccLichKingQuakeTrigger, IccLichKingQuakeAction>(ACTION_EMERGENCY + 6, EncounterRow::Mover);
    e.Node<IccLichKingShadowTrapTrigger, IccLichKingShadowTrapAction>(ACTION_EMERGENCY + 5, EncounterRow::Mover);
    e.Node<IccLichKingDefileTrigger, IccLichKingDefileAction>(ACTION_EMERGENCY + 4, EncounterRow::Mover);
    e.Node<IccLichKingVileSpiritsTrigger, IccLichKingVileSpiritsAction>(ACTION_EMERGENCY + 3, EncounterRow::Mover);
    e.Node<IccLichKingFrostmourneTrigger, IccLichKingFrostmourneAction>(ACTION_EMERGENCY + 2, EncounterRow::Mover);
    e.Node<IccLichKingTotemTrigger, IccLichKingTotemAction>(ACTION_RAID + 15);
    e.Node<IccLichKingTankDefensiveTrigger, IccLichKingCastTankDefensiveAction>(ACTION_RAID + 14);
    e.Node<IccLichKingTauntTrigger, IccLichKingTauntAction>(ACTION_RAID + 13);
    e.Node<IccLichKingSiphonTauntTrigger, IccLichKingSiphonTauntAction>(ACTION_RAID + 12);
    e.Node<IccLichKingPlagueCureTrigger, IccLichKingPlagueCureAction>(ACTION_RAID + 11);
    e.Node<IccLichKingPlagueWalkTrigger, IccLichKingPlagueWalkAction>(ACTION_RAID + 10, EncounterRow::Mover);
    e.Node<IccLichKingTranquilizeTrigger, IccLichKingTranquilizeAction>(ACTION_RAID + 9);
    e.Node<IccLichKingStunTrigger, IccLichKingStunAction>(ACTION_RAID + 8);
    e.Node<IccLichKingValkyrControlTrigger, IccLichKingValkyrControlAction>(ACTION_RAID + 7);
    e.Node<IccLichKingSilenceDispelTrigger, IccLichKingSilenceDispelAction>(ACTION_RAID + 6);
    e.Node<IccLichKingHarvestSoulTrigger, IccLichKingHarvestSoulAction>(ACTION_RAID + 5);
    e.Node<IccLichKingTargetTrigger, IccLichKingTargetAction>(ACTION_RAID + 4);
    e.Node<IccLichKingStackTrigger, IccLichKingStackAction>(ACTION_RAID + 3, EncounterRow::Mover);
    e.Node<IccLichKingStandTrigger, IccLichKingStandAction>(ACTION_RAID + 2, EncounterRow::Mover);
    e.Node<IccLichKingBrakeTrigger, IccLichKingBrakeStopAction>(ACTION_RAID + 1);

    e.LiveMultiplier<IccLichKingBrakeMultiplier>(Family::Spell | Family::Melee);
    e.LiveMultiplier<IccLichKingSplashMultiplier>(Family::Spell | Family::Melee);
    e.LiveMultiplier<IccLichKingCureHoldMultiplier>(Family::Spell);

    e.OwnTargeting("icc lich king targeting", Role::Any, &IccResolverPicks<&GetLkTarget>,
                   Family::DpsAssist | Family::TankAssist | Family::DpsAoe);
    e.Block("icc lich king target hold", Role::Any, &IsLkTargetHeld,
            Family::DpsAssist | Family::TankAssist | Family::DpsAoe | Family::AggressiveTarget |
                Family::AttackAnything | Family::AttackLeastHp | Family::AttackRti | Family::Follow |
                Family::MoveRandom | Family::PetAttack);
    e.OwnMovement("icc lich king movement", Role::Any, &IsLkMovementOwned, Family::Attack);
    e.OwnMovement("icc lich king late movement", Role::Any, &IsLkLateMovementOwned);
    e.OwnMovement("icc lich king late reach hold", Role::Any, &IsLkLateReachHeld, Family::Attack);
    e.Block("icc lich king spell movers", Role::Any, &IsLkAnyMovementOwned,
            Family::Charge | Family::Blink | Family::Disengage);
    e.Block("icc lich king avoid aoe", Role::Any, &IsLkLive, Family::AvoidAoe);
    e.Block("icc lich king taunts", Role::Tank, &IsLkLive, Family::Taunt);
    e.Block("icc lich king tank form", Role::Tank, &IsLkLive, 0, {"caster form"});
    e.Block("icc lich king tank defensive hold", Role::Tank, &IsLkTankDefensiveHeld, 0, HeldTankDefensiveNames());

    e.Tick(&TickLichKing);
}
}

EncounterDefinition const& IccLichKingDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_THE_LICH_KING, BossStateGate, "the-lich-king",
                                                &DefineLichKing);
    return definition;
}
