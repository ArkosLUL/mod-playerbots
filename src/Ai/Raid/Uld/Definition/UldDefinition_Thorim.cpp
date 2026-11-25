/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldDefinitions.h"

#include "BossAuraActions.h"
#include "BossAuraTriggers.h"
#include "BossResistanceMultipliers.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Strategy.h"
#include "UldActions_Thorim.h"
#include "UldEncounterGate.h"
#include "UldEncounter_Thorim.h"
#include "UldMultipliers_Thorim.h"
#include "UldTriggers_Thorim.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
constexpr RaidEncounterRules::FamilyMask PICKERS = Family::DpsAssist | Family::DpsAoe | Family::TankAssist |
                                                   Family::AggressiveTarget | Family::AttackAnything |
                                                   Family::AttackLeastHp;

constexpr RaidEncounterRules::FamilyMask DPS_PICKERS =
    Family::DpsAssist | Family::DpsAoe | Family::AggressiveTarget | Family::AttackAnything | Family::AttackLeastHp;

bool ThorimLeashBreached(PlayerbotAI* botAI) { return ThorimArenaLeashBreached(botAI, botAI->GetBot()); }

bool ThorimArenaSettled(PlayerbotAI* botAI) { return ThorimArenaAnchorSettled(botAI, botAI->GetBot()); }

bool ThorimRingSettled(PlayerbotAI* botAI) { return ThorimMeleeRingSettled(botAI, botAI->GetBot()); }

bool ThorimRingMelee(PlayerbotAI* botAI) { return ThorimRingMeleeOnFloor(botAI, botAI->GetBot()); }

bool ThorimRingStepped(PlayerbotAI* botAI) { return ThorimRingSteppedOut(botAI, botAI->GetBot()); }

bool ThorimPickingFor(PlayerbotAI* botAI)
{
    return botAI->GetState() == BOT_STATE_COMBAT && !PlayerbotAI::IsHeal(botAI->GetBot());
}

bool ThorimTankPicked(PlayerbotAI* botAI) { return ThorimPickingFor(botAI) && ThorimPhase2Active(botAI); }

bool ThorimDpsPicked(PlayerbotAI* botAI)
{
    return ThorimPickingFor(botAI) && (ThorimHasDpsTarget(botAI, botAI->GetBot()) || ThorimSplitActive(botAI));
}

bool ThorimPhase2DpsPicked(PlayerbotAI* botAI) { return ThorimPhase2Active(botAI) && ThorimDpsPicked(botAI); }

bool ThorimTauntWouldSteal(PlayerbotAI* botAI)
{
    if (!ThorimPhase2Active(botAI))
        return false;

    Unit* boss = GetThorim(botAI);
    if (!boss || botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get() != boss)
        return false;

    Player* bot = botAI->GetBot();
    Unit* victim = boss->GetVictim();
    if (victim == bot)
        return false;

    Player* holder = victim ? victim->ToPlayer() : nullptr;
    if (holder && PlayerbotAI::IsTank(holder))
        return true;

    return !ThorimAtTankSpots(bot) && !IsThorimAnchorTank(bot);
}

bool ThorimBalconyWalk(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!ThorimOnBalconySide(bot) || GetThorimSquad(botAI, bot) != ThorimSquad::Gauntlet)
        return false;

    ThorimBalconyAdvanceTrigger balconyAdvance(botAI);
    return balconyAdvance.IsActive();
}

bool ThorimFrostAuraHeld(PlayerbotAI* botAI) { return IsThorimFrostAuraPaladin(botAI->GetBot()); }

bool ThorimTankAnchored(PlayerbotAI* botAI) { return ThorimTankOnAnchor(botAI, botAI->GetBot()); }

bool ThorimLeverWait(PlayerbotAI* botAI)
{
    ObjectGuid lever;
    return ThorimLeverWaiting(botAI, botAI->GetBot(), lever);
}

bool ThorimGiantHeld(PlayerbotAI* botAI) { return ThorimGauntletGiantHeld(botAI, botAI->GetBot()) != nullptr; }

bool ThorimCorridorFlee(PlayerbotAI* botAI) { return ThorimCorridorFleeHeld(botAI, botAI->GetBot()); }

bool ThorimGauntletOnClock(PlayerbotAI* botAI) { return ThorimGauntletSquadOnClock(botAI, botAI->GetBot()); }

void TickThorim(PlayerbotAI* botAI)
{
    ThorimTickBarrierBail(botAI, botAI->GetBot());
    ThorimTickHumanTankCalls(botAI);
    ThorimTickAnchorTank(botAI);
    ThorimTickFrostAuraPaladin(botAI);
    ThorimTickChainLightning(botAI);
}

void DefineThorim(EncounterBuilder& e)
{
    e.Node(
        "thorim nature resistance trigger",
        [](PlayerbotAI* botAI) -> Trigger* { return new BossNatureResistanceTrigger(botAI, "thorim"); },
        "thorim nature resistance action",
        [](PlayerbotAI* botAI) -> Action* { return new BossNatureResistanceAction(botAI, "thorim"); }, ACTION_RAID);
    e.Node<ThorimUnbalancingStrikeSwapTrigger, ThorimUnbalancingStrikeSwapAction>(ACTION_RAID + 2);
    e.Node<ThorimTankPickupTrigger, ThorimTankPickupAction>(ACTION_RAID + 2);
    e.Node<ThorimDpsPriorityTrigger, ThorimDpsPriorityAction>(ACTION_RAID);

    e.Node<ThorimFrostAuraTrigger, ThorimFrostAuraAction>(ACTION_RAID);

    e.Node<ThorimPetLeashTrigger, ThorimPetLeashAction>(ACTION_RAID);

    e.Node<ThorimGauntletPositioningTrigger, ThorimGauntletPositioningAction>(ACTION_RAID);
    e.Node<ThorimGauntletLeverTrigger, ThorimGauntletLeverAction>(ACTION_RAID + 1);
    e.Node<ThorimGauntletLeadTrigger, ThorimGauntletLeadAction>(ACTION_RAID + 1);

    e.Node<ThorimBalconyAdvanceTrigger, ThorimBalconyAdvanceAction>(ACTION_RAID + 2);

    e.Node<ThorimArenaPositioningTrigger, ThorimArenaPositioningAction>(ACTION_RAID);
    e.Node<ThorimFallFromFloorTrigger, ThorimFallFromFloorAction>(ACTION_RAID + 1);
    e.Node<ThorimPhase2PositioningTrigger, ThorimPhase2PositioningAction>(ACTION_RAID);
    e.Node<ThorimSifBlizzardTrigger, ThorimSifBlizzardAction>(ACTION_RAID + 3);
    e.Node<ThorimUnreachableTrigger, ThorimUnreachableStepAction>(ACTION_RAID + 3);

    e.Node<ThorimChargedOrbTrigger, ThorimChargedOrbAction>(ACTION_RAID + 4);

    e.Node<ThorimPersonalDefensiveTrigger, ThorimCastPersonalDefensiveAction>(ACTION_RAID + 4);

    e.Node<ThorimRunicSmashTrigger, ThorimRunicSmashAction>(ACTION_RAID + 3);
    e.Node<ThorimRunicBarrierBailTrigger, ThorimRunicBarrierBailAction>(ACTION_RAID + 2);

    e.Node<ThorimArenaLeashTrigger, ThorimArenaLeashAction>(ACTION_RAID + 5);

    e.Node<ThorimResetEncounterStateTrigger, ThorimResetEncounterStateAction>(ACTION_RAID);

    e.Multiplier<ThorimRunicBarrierMultiplier>(Family::Melee | Family::Reach | Family::ReachHeal);

    e.OwnTargeting("thorim disable automatic targeting", Role::Tank, ThorimTankPicked,
                   DPS_PICKERS | Family::TankAssist);
    e.OwnTargeting("thorim disable automatic targeting", Role::NonTank, ThorimDpsPicked, DPS_PICKERS);
    e.OwnTargeting("thorim disable automatic targeting", Role::NonTank, ThorimPhase2DpsPicked, Family::TankAssist);

    e.OwnMovement("thorim movement guard", Role::Any, ThorimRingSettled,
                  Family::Attack | Family::Reach | Family::ReachHeal,
                  {ThorimPhase2PositioningAction::Name, ThorimBalconyAdvanceAction::Name,
                   ThorimSifBlizzardAction::Name, ThorimUnreachableStepAction::Name});
    e.Block("thorim ring aoe hold", Role::Any, ThorimRingMelee, Family::AvoidAoe);
    e.Block("thorim ring blizzard step", Role::Any, ThorimRingStepped, Family::Reach | Family::Charge);

    e.OwnMovement("thorim tank anchor guard", Role::Tank, ThorimTankAnchored,
                  Family::Attack | Family::Reach | Family::ReachHeal,
                  {ThorimPhase2PositioningAction::Name, ThorimBalconyAdvanceAction::Name,
                   ThorimUnreachableStepAction::Name});

    e.Exclusive("thorim arena leash", Role::Any, ThorimLeashBreached, Family::AvoidAoe | PICKERS,
                {ThorimArenaLeashAction::Name, ThorimArenaPositioningAction::Name, ThorimChargedOrbAction::Name,
                 ThorimSifBlizzardAction::Name, ThorimDpsPriorityAction::Name});

    e.Multiplier<ThorimArenaTargetGuardMultiplier>(Family::Attack | Family::Reach | Family::ReachHeal);

    e.OwnMovement("thorim arena anchor guard", Role::Any, ThorimArenaSettled,
                  Family::Attack | Family::Reach | Family::ReachHeal | Family::AvoidAoe,
                  {ThorimArenaPositioningAction::Name, ThorimArenaLeashAction::Name, ThorimChargedOrbAction::Name,
                   ThorimSifBlizzardAction::Name});

    e.OwnMovement("thorim balcony guard", Role::Any, ThorimBalconyWalk,
                  Family::Attack | Family::Reach | Family::ReachHeal | Family::AvoidAoe,
                  {ThorimBalconyAdvanceAction::Name});
    e.Multiplier<ThorimBalconyGuardMultiplier>(Family::Attack | Family::Reach | Family::ReachHeal);

    e.OwnMovement("thorim lever wait", Role::Any, ThorimLeverWait, Family::Attack | Family::AvoidAoe,
                  {ThorimGauntletLeverAction::Name});
    e.Block("thorim lever wait", Role::Any, ThorimLeverWait,
            Family::TankAssist | Family::Taunt | Family::Charge | DPS_PICKERS);

    e.OwnMovement("thorim giant hold", Role::Any, ThorimGiantHeld, Family::Attack | Family::AvoidAoe,
                  {ThorimGauntletLeadAction::Name});
    e.Block("thorim giant hold", Role::Any, ThorimGiantHeld, Family::TankAssist | Family::Charge | DPS_PICKERS);

    e.Block("thorim corridor flee hold", Role::Any, ThorimCorridorFlee, Family::Flee);
    e.Block("thorim revive pet hold", Role::Any, ThorimGauntletOnClock, 0, {"revive pet"});

    e.Block("thorim taunt guard", Role::Any, ThorimTauntWouldSteal, Family::Taunt);

    e.Multiplier<BossNatureAspectHoldMultiplier>(Family::Spell, "thorim");
    e.Multiplier<ThorimChainWaitCastHoldMultiplier>(Family::Spell);

    e.Block("thorim frost aura hold", Role::Any, ThorimFrostAuraHeld, 0,
            {"devotion aura", "retribution aura", "concentration aura", "crusader aura", "sanctity aura",
             "shadow resistance aura", "fire resistance aura"});

    e.Tick(TickThorim);
}
}

EncounterDefinition const& UldThorimDefinition()
{
    static EncounterDefinition const definition(ULD_BOSS_THORIM, BossStateGate, "thorim", &DefineThorim);
    return definition;
}
