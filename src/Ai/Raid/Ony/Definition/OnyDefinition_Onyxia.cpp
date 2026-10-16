/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "OnyDefinitions.h"

#include "OnyActions.h"
#include "OnyHelpers.h"
#include "OnyMultipliers.h"
#include "OnyTriggers.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

using namespace OnyHelpers;

namespace
{
constexpr RaidEncounterRules::FamilyMask FORMATION_MOVERS =
    Family::CombatFormationMove | Family::TankFace | Family::SetBehind | Family::Follow;
constexpr RaidEncounterRules::FamilyMask GENERIC_MOVERS =
    FORMATION_MOVERS | Family::Reach | Family::ReachHeal | Family::Charge | Family::RearFlank;
constexpr RaidEncounterRules::FamilyMask CLOSERS = Family::Reach | Family::Charge | Family::SetBehind | Family::RearFlank;
constexpr RaidEncounterRules::FamilyMask WANDER_MOVERS = Family::MoveRandom | Family::RunAway | Family::Flee |
                                                         Family::MoveOutOfCollision | Family::MoveOutOfEnemyContact;

bool Engaged(PlayerbotAI* botAI) { return OnyxiaSnapshotFor(botAI).phase != OnyxiaPhase::None; }
bool DodgeLive(PlayerbotAI* botAI) { return DodgeReasons(botAI) != 0; }
bool GroundLayout(PlayerbotAI* botAI) { return IsGroundLayout(OnyxiaSnapshotFor(botAI).phase); }
bool AirLayout(PlayerbotAI* botAI) { return IsAirborne(OnyxiaSnapshotFor(botAI).phase); }
bool BreathLaneLive(PlayerbotAI* botAI) { return BreathLive(botAI->GetBot()); }

bool MeleeDpsOnGroundedOnyxia(PlayerbotAI* botAI)
{
    return IsGrounded(OnyxiaSnapshotFor(botAI).phase) && !PlayerbotAI::IsTank(botAI->GetBot());
}

bool ClosingOnAirborneOnyxia(PlayerbotAI* botAI)
{
    OnyxiaSnapshot const& snap = OnyxiaSnapshotFor(botAI);
    if (!snap.boss || !(IsAirborne(snap.phase) || snap.phase == OnyxiaPhase::Landing))
        return false;

    return botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get() == snap.boss;
}

void DefineOnyxia(EncounterBuilder& e)
{
    // The dodge outranks everything: eggs, a breath lane, Blast Nova and a Fireball splash each cost
    // far more than any hold is worth. The tank's cooldown sits above his own walk, so a hold that keeps
    // returning true cannot starve it.
    e.Node<OnyxiaDodgeTrigger, OnyxiaDodgeAction>(ACTION_EMERGENCY + 6, EncounterRow::Mover);
    e.Node<OnyxiaMainTankCooldownTrigger, OnyxiaMainTankCooldownAction>(ACTION_RAID + 7);
    e.Node<OnyxiaMainTankTrigger, OnyxiaMainTankAction>(ACTION_RAID + 6, EncounterRow::Mover);
    e.Node<OnyxiaAntiFearTrigger, OnyxiaAntiFearAction>(ACTION_RAID + 5);
    e.Node<OnyxiaRedirectThreatTrigger, OnyxiaRedirectThreatAction>(ACTION_RAID + 4);
    e.Node<OnyxiaChooseTargetTrigger, OnyxiaChooseTargetAction>(ACTION_RAID + 3);
    e.Node<OnyxiaGroundSlotTrigger, OnyxiaGroundSlotAction>(ACTION_RAID + 2, EncounterRow::Mover);
    e.Node<OnyxiaAirSpotTrigger, OnyxiaAirSpotAction>(ACTION_RAID + 2, EncounterRow::Mover);
    e.Node<OnyxiaRearFlankTrigger, OnyxiaRearFlankAction>(ACTION_MOVE + 5, EncounterRow::Mover);

    // Every dodge issues at MOVEMENT_FORCED and FORCED never beats FORCED, so while one is live
    // nothing else may move the bot, attacks included.
    e.Exclusive("onyxia", Role::Any, DodgeLive, 0, {OnyxiaDodgeAction::Name});

    // The main tank's own walk and hold are the only things that move him while he has her, waits
    // for her landing, or is on his way back to the wall. Generic reach and facing would pull him off
    // the spot, and the wall is what stops Wing Buffet's knockback.
    e.Block("onyxia", Role::Tank, MainTankOwnsMovement, GENERIC_MOVERS | WANDER_MOVERS | Family::AvoidAoe);

    // Ranged and healers stand in their slots on the ground and in their cells in the air. Reach stays
    // on the ground, where she is in range of every slot; in the air it walks them under her hover point
    // and into the next breath lane.
    e.OwnMovement("onyxia", Role::Ranged | Role::Heal, GroundLayout,
                  Family::Attack | Family::Reach | Family::ReachHeal);
    e.OwnMovement("onyxia", Role::Ranged | Role::Heal, AirLayout, Family::Attack | Family::ReachHeal);

    // Melee on her own the flank band through "onyxia rear flank". The formation movers and the
    // generic arc, which redraws a random angle every tick, would walk them back out of it.
    e.Block("onyxia", Role::Melee, MeleeDpsOnGroundedOnyxia, FORMATION_MOVERS, {"rear flank"});

    // Chasing her while she is in the air, or on her way down, only drags melee under the hover point.
    e.Block("onyxia", Role::Any, ClosingOnAirborneOnyxia, CLOSERS);

    e.Multiplier<OnyxiaEggGuardMultiplier>(CLOSERS | Family::ReachHeal);

    // Closing on anything while a breath is crossing the room walks the bot straight through it.
    e.Block("onyxia", Role::Any, BreathLaneLive, CLOSERS);

    e.Block("onyxia", Role::Any, Engaged, WANDER_MOVERS | Family::Follow);

    // Unchecked displacement: 20yd of blind travel ends in the egg pit as often as not.
    e.Block("onyxia", Role::Any, Engaged, Family::Blink | Family::Disengage);

    // "onyxia redirect threat" spends them on the main tank as she lands, not on the pull. The plain
    // Tricks picks its own target, which can be a dps warrior: that is who takes her off the tank.
    e.Block("onyxia", Role::Any, Engaged, 0,
            {"misdirection on main tank", "tricks of the trade", "tricks of the trade on main tank"});

    e.OwnTargeting("onyxia", Role::Dps, Engaged,
                   Family::DpsAssist | Family::AggressiveTarget | Family::AttackAnything);
    e.OwnTargeting("onyxia", Role::Tank, Engaged,
                   Family::TankAssist | Family::AggressiveTarget | Family::AttackAnything);

    e.Multiplier<OnyxiaTauntGuardMultiplier>(Family::Taunt);
    e.Multiplier<OnyxiaThreatHoldMultiplier>(Family::Spell | Family::Melee);
    e.Multiplier<OnyxiaBurstWindowMultiplier>(Family::AnyAction);
    e.Multiplier<OnyxiaAntiFearTotemGuardMultiplier>(Family::Spell);

    e.Tick(TickOnyxia);
}
}  // namespace

EncounterDefinition const& OnyxiaDefinition()
{
    static EncounterDefinition const definition(ONYXIA_BOSS_ONYXIA, BossStateGate, "onyxia", &DefineOnyxia);
    return definition;
}
