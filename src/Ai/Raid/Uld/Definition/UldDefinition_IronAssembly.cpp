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

// Only while this bot is actually committed to a hazard. Outside that window the generic movers are
// what bring it back to the formation, and the position row has already yielded.
bool IronAssemblyHazardMove(PlayerbotAI* botAI)
{
    return IronAssemblyFormationActive(botAI) && IronAssemblyMemberMustMove(botAI, botAI->GetBot());
}

// The whole channel, not just while inside: the escape parks at the clearance, and Charge and
// Intercept reach 25 yd from there, right back into the blast.
bool IronAssemblyBrundirHazard(PlayerbotAI* botAI)
{
    if (!IronAssemblyFormationActive(botAI))
        return false;

    Unit* brundir = GetIronAssemblyMember(botAI, NPC_BRUNDIR);
    return brundir && (IronAssemblyOverloadActive(brundir) || IronAssemblyTendrilsActive(brundir));
}

// Swapped out and walking to the Meltdown point or waiting on it. Late and no-partner carriers stay
// on the boss, so they keep their reach.
bool IronAssemblyMeltdownCarrierAway(PlayerbotAI* botAI)
{
    IronAssemblyMeltdownRole const role = IronAssemblyMeltdownRoleFor(botAI, botAI->GetBot());
    return role == IronAssemblyMeltdownRole::Run || role == IronAssemblyMeltdownRole::Parked;
}

// Only a tank holding an assignment, which is the one that has a spot to be pulled off.
bool IronAssemblyTankPinned(PlayerbotAI* botAI)
{
    return IronAssemblyFormationActive(botAI) && IronAssemblyAssignedBoss(botAI, botAI->GetBot());
}

// Only held for Fusion Punch, so a normal mode kill order frees them once Steelbreaker is dead.
// Under the floor the class nodes are a last resort, but only while a council member is hitting the tank.
// A tank rises at 13-19% and a swapped-out Meltdown carrier gets there too, with nothing on either.
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

// Divine Shield and a self Lay on Hands put up Forbearance, which locks out Divine Protection, a paladin
// tank's one button for the punch, for 2 min.
std::vector<std::string> IronAssemblyHeldTankDefensiveNames()
{
    std::vector<std::string> names = HeldTankDefensiveNames();
    names.insert(names.end(), {"divine shield", "lay on hands"});
    return names;
}

void DefineIronAssembly(EncounterBuilder& e)
{
    // First success ends the tick, so this is a survival order. Interrupt sits above every class interrupt
    // (ACTION_INTERRUPT, 40) since Whirl has no positional answer. Position goes last, yielding once parked.
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
    // Above every heal: a healer druid that heals while a body waits never gets there.
    e.Node<IronAssemblyTankBattleRezTrigger, IronAssemblyTankBattleRezAction>(ACTION_RAID + 7, EncounterRow::Mover);
    e.Node<IronAssemblyTankAssignmentTrigger, IronAssemblyTankAssignmentAction>(ACTION_RAID + 6);
    e.Node<IronAssemblyUnheldTauntTrigger, IronAssemblyUnheldTauntAction>(ACTION_RAID + 5);
    e.Node<IronAssemblyShieldOfRunesTrigger, IronAssemblyShieldOfRunesAction>(ACTION_RAID + 4);
    e.Node<IronAssemblyFusionPunchDispelTrigger, IronAssemblyFusionPunchDispelAction>(ACTION_RAID + 3);
    e.Node<IronAssemblyNatureAspectTrigger, IronAssemblyNatureAspectAction>(ACTION_RAID + 2);
    e.Node<IronAssemblyNatureTotemTrigger, IronAssemblyNatureTotemAction>(ACTION_RAID + 2);
    // Above heals on purpose, one GCD per buff: a raised tank stands up ~14% short on health without them.
    e.Node<IronAssemblyTankRebuffTrigger, IronAssemblyTankRebuffAction>(ACTION_RAID + 2);
    e.Node<IronAssemblyRedirectThreatTrigger, IronAssemblyRedirectThreatAction>(ACTION_RAID + 2);
    e.Node<IronAssemblyRuneOfPowerMeleeTrigger, IronAssemblyRuneOfPowerMeleeAction>(ACTION_RAID + 1,
                                                                                   EncounterRow::Mover);
    e.Node<IronAssemblySetDpsPriorityTrigger, IronAssemblySetDpsPriorityAction>(ACTION_RAID + 1);
    e.Node<IronAssemblyRaidPositionTrigger, IronAssemblyRaidPositionAction>(ACTION_RAID, EncounterRow::Mover);

    // Targets picked in code for every role, all fight: the fallback is always a living member. No role
    // filter on purpose, IsRanged() is true for healers.
    e.OwnTargeting("iron assembly disable automatic targeting", Role::Any, IronAssemblyOwnsTargets);

    // Hazard window only: the position row yields once parked, so a permanent guard strands the raid
    // wherever its last dodge ended. ReachHeal would walk a healer to someone the formation can't reach.
    e.OwnMovement("iron assembly movement guard", Role::Any, IronAssemblyHazardMove,
                  Family::Attack | Family::Reach | Family::ReachHeal);

    // A gap-closer runs straight, reads nothing about the ground and isn't a MovementAction, so the
    // movement guard can't see it.
    e.Block("iron assembly charge guard", Role::Any, IronAssemblyBrundirHazard, Family::Charge);

    // The movement guard keeps reach, and the run only outranks it while walking. Parked, reach melee
    // tugs the carrier back toward Steelbreaker, and a charge at any point lands it on the melee.
    e.Block("iron assembly meltdown carrier hold", Role::Tank, IronAssemblyMeltdownCarrierAway,
            Family::Reach | Family::ReachHeal | Family::Charge);

    // A blast next to him in 3 s: reach, set behind or a charge would walk a melee bot back into it. A Rune
    // of Death still gets to move it.
    e.OwnMovement("iron assembly meltdown melee guard", Role::Melee, IronAssemblyMeltdownMeleeWindow, Family::Attack,
                  {IronAssemblyMeltdownStepOutAction::Name, IronAssemblyRuneOfDeathAction::Name});
    e.Block("iron assembly meltdown melee charge guard", Role::Melee, IronAssemblyMeltdownMeleeWindow,
            Family::Charge);

    // The class nodes spend these on health alone and would leave the Fusion Punch node nothing.
    e.Block("iron assembly tank defensive hold", Role::Tank, IronAssemblyTankDefensiveHeld, 0,
            IronAssemblyHeldTankDefensiveNames());

    // Tank face is a second mover at the tank spot's priority, and the two alternate. Set facing is a
    // separate node.
    e.Block("iron assembly disable tank face", Role::Any, IronAssemblyTankPinned, Family::TankFace);

    // The rune sits inside a big boss's "enemy too close for spell", so the class escape and the raid spot
    // would trade the caster back and forth. Safe to hold: he faces his tank with the rune behind him.
    e.Block("iron assembly rune of power hold", Role::Ranged, IronAssemblyOnRuneOfPower,
            Family::Flee | Family::RunAway | Family::Blink | Family::Disengage);

    // Every aspect, not just Dragonhawk: a vetoed node still pushes its alternatives, and the hunter
    // buff chain runs dragonhawk, hawk, monkey.
    e.Block("iron assembly nature aspect hold", Role::Any, IsIronAssemblyNatureHunter, 0,
            {"aspect of the viper", "aspect of the dragonhawk", "aspect of the hawk", "aspect of the monkey",
             "aspect of the cheetah", "aspect of the pack"});

    // The air slot holds one totem, so the shaman's own air nodes would trade it back on the next GCD.
    e.Block("iron assembly nature totem hold", Role::Any, IsIronAssemblyNatureTotemShaman, 0,
            {"wrath of air totem", "windfury totem", "set wrath of air totem", "set windfury totem",
             "set grounding totem"});

    // Potions and tinkers are on the burst list.
    e.Multiplier<IronAssemblyHoldDpsCooldownsMultiplier>(Family::AnyAction);
    e.Multiplier<IronAssemblyTauntGuardMultiplier>(Family::Spell);
}
}  // namespace

EncounterDefinition const& UldIronAssemblyDefinition()
{
    static EncounterDefinition const definition(ULD_BOSS_ASSEMBLY, BossStateGate, "iron assembly", &DefineIronAssembly);
    return definition;
}
