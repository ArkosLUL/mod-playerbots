/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "EoEDefinitions.h"

#include "EoEActions.h"
#include "EoEData.h"
#include "EoEEncounter_Malygos.h"
#include "EoEMultipliers.h"
#include "EoETriggers.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
bool Phase1(PlayerbotAI* botAI) { return GetMalygosPhase(botAI->GetBot()) == MalygosPhase::P1; }
bool Phase2(PlayerbotAI* botAI) { return GetMalygosPhase(botAI->GetBot()) == MalygosPhase::P2; }
bool Phase3(PlayerbotAI* botAI) { return GetMalygosPhase(botAI->GetBot()) == MalygosPhase::P3; }
bool PhaseTransition(PlayerbotAI* botAI) { return GetMalygosPhase(botAI->GetBot()) == MalygosPhase::Transition; }

bool Phase1TankedAndNotMainTank(PlayerbotAI* botAI)
{
    return Phase1(botAI) && !GetMalygosAggro(botAI).mainTank && GetMalygosAggro(botAI).victimIsTank;
}

bool Phase1NotBossTank(PlayerbotAI* botAI)
{
    return Phase1(botAI) && !GetMalygosAggro(botAI).mainTank && !GetMalygosAggro(botAI).victim;
}

bool Phase1BossVictim(PlayerbotAI* botAI) { return Phase1(botAI) && GetMalygosAggro(botAI).victim; }
bool Phase1NotMainTank(PlayerbotAI* botAI) { return Phase1(botAI) && !GetMalygosAggro(botAI).mainTank; }
bool Phase2Riding(PlayerbotAI* botAI) { return Phase2(botAI) && botAI->GetBot()->GetVehicle(); }
bool Phase2OnFoot(PlayerbotAI* botAI) { return Phase2(botAI) && !botAI->GetBot()->GetVehicle(); }

bool MalygosGateOpen(PlayerbotAI* botAI, uint32 encounterId)
{
    if (BossStateGateOpen(botAI, encounterId))
        return true;

    Unit* drake = botAI->GetBot()->GetVehicleBase();
    return drake && drake->GetEntry() == NPC_WYRMREST_SKYTALON;
}

constexpr EncounterGate MalygosGate{&MalygosGateOpen, &BossStateGateLive};

void DefineMalygos(EncounterBuilder& e)
{
    e.Node<MalygosTrigger, MalygosPositionAction>(ACTION_MOVE);
    e.Node<MalygosTrigger, MalygosTargetAction>(ACTION_RAID + 1);
    e.Node<MalygosTrigger, MalygosSpellstealAction>(ACTION_RAID + 2);

    e.Node<PowerSparkTrigger, PullPowerSparkAction>(ACTION_RAID + 3);
    e.Node<PowerSparkTrigger, KillPowerSparkAction>(ACTION_RAID + 2);

    e.Node<MalygosBubbleTrigger, MalygosSeekBubbleAction>(ACTION_EMERGENCY + 2);
    e.Node<SurgeOfPowerTrigger, AvoidSurgeOfPowerAction>(ACTION_EMERGENCY);

    e.Node<MalygosFreeDiskTrigger, MalygosBoardDiskAction>(ACTION_RAID + 4);
    e.Node<MalygosOnDiskTrigger, MalygosRideDiskAction>(ACTION_RAID + 4);

    e.Node<MalygosDrakeFlightTrigger, EoEFlyDrakeAction>(ACTION_EMERGENCY);
    e.Node<MalygosDrakeFlightTrigger, EoEDrakeAttackAction>(ACTION_NORMAL + 5);
    e.Node<DrakeSurgeTrigger, DrakeSurgeShieldAction>(ACTION_EMERGENCY + 5);

    e.Block("malygos", Role::Any, Phase1TankedAndNotMainTank, Family::Taunt);

    e.Block("malygos", Role::Any, Phase1, Family::Flee | Family::RunAway | Family::Blink | Family::Disengage);
    e.Block("malygos", Role::Any, Phase1, Family::Follow);

    e.Block("malygos", Role::Dps, Phase1, Family::DropTarget);

    e.Block("malygos", Role::Any, Phase1BossVictim, Family::Reach | Family::ReachHeal);

    e.OwnTargeting("malygos", Role::Dps, Phase1, Family::DpsAssist);

    e.OwnMovement("malygos", Role::Any, Phase1NotBossTank, Family::ReachHeal,
                  {MalygosPositionAction::Name, MalygosTargetAction::Name, KillPowerSparkAction::Name});
    e.Block("malygos", Role::Any, Phase1NotBossTank, Family::Charge);

    e.Block("malygos", Role::Any, Phase1NotMainTank, Family::TankAssist);

    e.Block("malygos", Role::Any, Phase2Riding, Family::Charge);

    e.OwnTargeting("malygos", Role::Dps, Phase2, Family::DpsAssist);

    e.Block("malygos", Role::Any, Phase2, Family::Flee);

    e.Exclusive("malygos", Role::Any, Phase2Riding, 0,
                {MalygosRideDiskAction::Name, MalygosTargetAction::Name, "leave vehicle"});

    e.Block("malygos", Role::Ranged | Role::Heal, Phase2OnFoot, Family::Reach | Family::Follow);

    e.Multiplier<MalygosScionTankAssistMultiplier>(Family::TankAssist);

    e.Exclusive("malygos", Role::Any, Phase3, 0, {EoEFlyDrakeAction::Name});

    e.Exclusive("malygos", Role::Any, PhaseTransition, 0, {MalygosPositionAction::Name});
}
}

EncounterDefinition const& EoEMalygosDefinition()
{
    static EncounterDefinition const definition(EOE_DATA_MALYGOS, MalygosGate, "malygos", &DefineMalygos);
    return definition;
}
