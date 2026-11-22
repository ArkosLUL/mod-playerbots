/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldDefinitions.h"

#include "Player.h"
#include "PlayerbotAI.h"
#include "RaidTankDefensive.h"
#include "Strategy.h"
#include "UldActions_IronAssembly.h"
#include "UldEncounterGate.h"
#include "UldEncounter_IronAssembly.h"
#include "UldMultipliers_IronAssembly.h"
#include "UldScripts.h"
#include "UldTriggers_IronAssembly.h"

#include <string>
#include <vector>

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
bool IronAssemblyOwnsTargets(PlayerbotAI* botAI)
{
    return botAI->GetState() == BOT_STATE_COMBAT && IronAssemblyFormationActive(botAI);
}

bool IronAssemblyHazardMove(PlayerbotAI* botAI)
{
    return IronAssemblyFormationActive(botAI) && IronAssemblyMemberMustMove(botAI, botAI->GetBot());
}

bool IronAssemblyBrundirHazard(PlayerbotAI* botAI)
{
    if (!IronAssemblyFormationActive(botAI))
        return false;

    Unit* brundir = GetIronAssemblyMember(botAI, NPC_BRUNDIR);
    return brundir && (IronAssemblyOverloadActive(brundir) || IronAssemblyTendrilsActive(brundir));
}

bool IronAssemblyMeltdownCarrierAway(PlayerbotAI* botAI)
{
    IronAssemblyMeltdownRole const role = IronAssemblyMeltdownRoleFor(botAI, botAI->GetBot());
    return role == IronAssemblyMeltdownRole::Run || role == IronAssemblyMeltdownRole::Parked;
}

bool IronAssemblyTankPinned(PlayerbotAI* botAI)
{
    return IronAssemblyFormationActive(botAI) && IronAssemblyAssignedBoss(botAI, botAI->GetBot());
}

bool IronAssemblyTankDefensiveHeld(PlayerbotAI* botAI)
{
    if (!IronAssemblyFormationActive(botAI))
        return false;

    IronAssemblyTargets targets;
    GatherIronAssemblyTargets(botAI, targets);
    if (!targets.steelbreaker)
        return false;

    Player* bot = botAI->GetBot();
    if (bot->GetHealthPct() >= ULDUAR_IRON_ASSEMBLY_TANK_DEFENSIVE_LAST_RESORT_PCT)
        return true;

    for (Unit* member : {targets.steelbreaker, targets.molgeim, targets.brundir})
    {
        if (member && member->GetVictim() == bot)
            return false;
    }

    return true;
}

std::vector<std::string> IronAssemblyHeldTankDefensiveNames()
{
    std::vector<std::string> names = HeldTankDefensiveNames();
    names.insert(names.end(), {"divine shield", "lay on hands"});
    return names;
}

void DefineIronAssembly(EncounterBuilder& e)
{
    e.Node<IronAssemblyResetEncounterStateTrigger, IronAssemblyResetEncounterStateAction>(ACTION_EMERGENCY + 10);
    e.Node<IronAssemblyOverloadTrigger, IronAssemblyOverloadAction>(ACTION_EMERGENCY + 6, EncounterRow::Mover);
    e.Node<IronAssemblyLightningTendrilsTrigger, IronAssemblyLightningTendrilsAction>(ACTION_EMERGENCY + 6,
                                                                                     EncounterRow::Mover);
    e.Node<IronAssemblyRuneOfDeathTrigger, IronAssemblyRuneOfDeathAction>(ACTION_EMERGENCY + 5, EncounterRow::Mover);
    e.Node<IronAssemblyInterruptTrigger, IronAssemblyInterruptAction>(ACTION_EMERGENCY + 4);
    e.Node<IronAssemblyMeltdownRunOutTrigger, IronAssemblyMeltdownRunOutAction>(ACTION_EMERGENCY + 3,
                                                                               EncounterRow::Mover);
    e.Node<IronAssemblyMeltdownStepOutTrigger, IronAssemblyMeltdownStepOutAction>(ACTION_EMERGENCY + 3,
                                                                                 EncounterRow::Mover);
    e.Node<IronAssemblyFusionPunchDefensiveTrigger, IronAssemblyFusionPunchDefensiveAction>(ACTION_EMERGENCY + 2);
    e.Node<IronAssemblyTankBattleRezTrigger, IronAssemblyTankBattleRezAction>(ACTION_RAID + 7, EncounterRow::Mover);
    e.Node<IronAssemblyTankAssignmentTrigger, IronAssemblyTankAssignmentAction>(ACTION_RAID + 6);
    e.Node<IronAssemblyUnheldTauntTrigger, IronAssemblyUnheldTauntAction>(ACTION_RAID + 5);
    e.Node<IronAssemblyShieldOfRunesTrigger, IronAssemblyShieldOfRunesAction>(ACTION_RAID + 4);
    e.Node<IronAssemblyFusionPunchDispelTrigger, IronAssemblyFusionPunchDispelAction>(ACTION_RAID + 3);
    e.Node<IronAssemblyNatureAspectTrigger, IronAssemblyNatureAspectAction>(ACTION_RAID + 2);
    e.Node<IronAssemblyNatureTotemTrigger, IronAssemblyNatureTotemAction>(ACTION_RAID + 2);
    e.Node<IronAssemblyTankRebuffTrigger, IronAssemblyTankRebuffAction>(ACTION_RAID + 2);
    e.Node<IronAssemblyRedirectThreatTrigger, IronAssemblyRedirectThreatAction>(ACTION_RAID + 2);
    e.Node<IronAssemblyRuneOfPowerMeleeTrigger, IronAssemblyRuneOfPowerMeleeAction>(ACTION_RAID + 1,
                                                                                   EncounterRow::Mover);
    e.Node<IronAssemblySetDpsPriorityTrigger, IronAssemblySetDpsPriorityAction>(ACTION_RAID + 1);
    e.Node<IronAssemblyRaidPositionTrigger, IronAssemblyRaidPositionAction>(ACTION_RAID, EncounterRow::Mover);

    e.OwnTargeting("iron assembly disable automatic targeting", Role::Any, IronAssemblyOwnsTargets);

    e.OwnMovement("iron assembly movement guard", Role::Any, IronAssemblyHazardMove,
                  Family::Attack | Family::Reach | Family::ReachHeal);

    e.Block("iron assembly charge guard", Role::Any, IronAssemblyBrundirHazard, Family::Charge);

    e.Block("iron assembly meltdown carrier hold", Role::Tank, IronAssemblyMeltdownCarrierAway,
            Family::Reach | Family::ReachHeal | Family::Charge);

    e.OwnMovement("iron assembly meltdown melee guard", Role::Melee, IronAssemblyMeltdownMeleeWindow, Family::Attack,
                  {IronAssemblyMeltdownStepOutAction::Name, IronAssemblyRuneOfDeathAction::Name});
    e.Block("iron assembly meltdown melee charge guard", Role::Melee, IronAssemblyMeltdownMeleeWindow,
            Family::Charge);

    e.Block("iron assembly tank defensive hold", Role::Tank, IronAssemblyTankDefensiveHeld, 0,
            IronAssemblyHeldTankDefensiveNames());

    e.Block("iron assembly disable tank face", Role::Any, IronAssemblyTankPinned, Family::TankFace);

    e.Block("iron assembly rune of power hold", Role::Ranged, IronAssemblyOnRuneOfPower,
            Family::Flee | Family::RunAway | Family::Blink | Family::Disengage);

    e.Block("iron assembly nature aspect hold", Role::Any, IsIronAssemblyNatureHunter, 0,
            {"aspect of the viper", "aspect of the dragonhawk", "aspect of the hawk", "aspect of the monkey",
             "aspect of the cheetah", "aspect of the pack"});

    e.Block("iron assembly nature totem hold", Role::Any, IsIronAssemblyNatureTotemShaman, 0,
            {"wrath of air totem", "windfury totem", "set wrath of air totem", "set windfury totem",
             "set grounding totem"});

    e.Multiplier<IronAssemblyHoldDpsCooldownsMultiplier>(Family::AnyAction);
    e.Multiplier<IronAssemblyTauntGuardMultiplier>(Family::Spell);
}
}

EncounterDefinition const& UldIronAssemblyDefinition()
{
    static EncounterDefinition const definition(ULD_BOSS_ASSEMBLY, BossStateGate, "iron assembly", &DefineIronAssembly);
    return definition;
}
