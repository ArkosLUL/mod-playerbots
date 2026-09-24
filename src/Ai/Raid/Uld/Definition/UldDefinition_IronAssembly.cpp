/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldDefinitions.h"

#include "PlayerbotAI.h"
#include "Strategy.h"
#include "UldActions_IronAssembly.h"
#include "UldEncounterGate.h"
#include "UldEncounter_IronAssembly.h"
#include "UldMultipliers_IronAssembly.h"
#include "UldScripts.h"
#include "UldTriggers_IronAssembly.h"

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

// The whole window, not only while the bot is still inside the circle. MustMove goes false the moment
// it clears the clearance, and Charge and Intercept both reach 25 yd from there, so gating on that
// hands the ability back at exactly the range that puts the bot back in the blast.
bool IronAssemblyBrundirHazard(PlayerbotAI* botAI)
{
    if (!IronAssemblyFormationActive(botAI))
        return false;

    Unit* brundir = GetIronAssemblyMember(botAI, NPC_BRUNDIR);
    return brundir && (IronAssemblyOverloadActive(brundir) || IronAssemblyTendrilsActive(brundir));
}

// Only a tank holding an assignment, which is the one that has a spot to be pulled off.
bool IronAssemblyTankPinned(PlayerbotAI* botAI)
{
    return IronAssemblyFormationActive(botAI) && IronAssemblyAssignedBoss(botAI, botAI->GetBot());
}

void DefineIronAssembly(EncounterBuilder& e)
{
    // The engine stops at the first action that returns true, so this order is a survival ranking.
    // The three hazards lead, being 20,000 nature, 5000 a second, and 5500 a second respectively.
    //
    // The interrupt has to sit above the RAID band, because every class interrupt lives at
    // ACTION_INTERRUPT (40): Lightning Whirl reaches 100 yd and has no positional answer at all, so
    // nothing else in the fight can substitute for stopping the cast.
    //
    // Below that, tanking outranks damage and damage outranks standing still. Position is last on
    // purpose and yields as soon as it is parked, which is what leaves a bot free to take the kick.
    e.Node<IronAssemblyResetEncounterStateTrigger, IronAssemblyResetEncounterStateAction>(ACTION_EMERGENCY + 10);
    e.Node<IronAssemblyOverloadTrigger, IronAssemblyOverloadAction>(ACTION_EMERGENCY + 6, EncounterRow::Mover);
    e.Node<IronAssemblyLightningTendrilsTrigger, IronAssemblyLightningTendrilsAction>(ACTION_EMERGENCY + 6,
                                                                                     EncounterRow::Mover);
    e.Node<IronAssemblyRuneOfDeathTrigger, IronAssemblyRuneOfDeathAction>(ACTION_EMERGENCY + 5, EncounterRow::Mover);
    e.Node<IronAssemblyInterruptTrigger, IronAssemblyInterruptAction>(ACTION_EMERGENCY + 4);
    e.Node<IronAssemblyTankAssignmentTrigger, IronAssemblyTankAssignmentAction>(ACTION_RAID + 6);
    e.Node<IronAssemblyShieldOfRunesTrigger, IronAssemblyShieldOfRunesAction>(ACTION_RAID + 4);
    e.Node<IronAssemblyFusionPunchDispelTrigger, IronAssemblyFusionPunchDispelAction>(ACTION_RAID + 3);
    e.Node<IronAssemblyRedirectThreatTrigger, IronAssemblyRedirectThreatAction>(ACTION_RAID + 2);
    e.Node<IronAssemblyRuneOfPowerSoakTrigger, IronAssemblyRuneOfPowerSoakAction>(ACTION_RAID + 1,
                                                                                 EncounterRow::Mover);
    e.Node<IronAssemblySetDpsPriorityTrigger, IronAssemblySetDpsPriorityAction>(ACTION_RAID + 1);
    e.Node<IronAssemblyRaidPositionTrigger, IronAssemblyRaidPositionAction>(ACTION_RAID, EncounterRow::Mover);

    // Every non-tank's target is picked in code, because killing one council member restores the other
    // two to full, so damage sprayed across three health bars is thrown away. For every role and the
    // whole fight: the fallback is always a living council member, so nothing is ever left without a
    // target. No role filter on purpose - IsRanged() is true for healers.
    e.OwnTargeting("iron assembly disable automatic targeting", Role::Any, IronAssemblyOwnsTargets);

    // Scoped to the hazard window rather than the whole fight. A permanent guard would leave the raid
    // parked wherever its last dodge ended, because the position row yields once it arrives and nothing
    // else would walk anyone back - the Void Reaver failure. ReachHeal walks a healer into range of
    // someone the formation can't reach.
    e.OwnMovement("iron assembly movement guard", Role::Any, IronAssemblyHazardMove,
                  Family::Attack | Family::Reach | Family::ReachHeal);

    // A gap-closer runs in a straight line, reads nothing about the ground, and isn't a MovementAction,
    // so the movement guard can't see it. Traced: the only three melee that cast one during an Overload
    // were the only three that took Overload damage.
    e.Block("iron assembly charge guard", Role::Any, IronAssemblyBrundirHazard, Family::Charge);

    // The tank spot decides where a tank stands, and tank face is a second mover at the same priority.
    // Left alone the two alternate every second between the spot and a point 6.8 yd nearer the raid,
    // which also drags the Meltdown blast in over the ranged. Set facing is a separate node.
    e.Block("iron assembly disable tank face", Role::Any, IronAssemblyTankPinned, Family::TankFace);

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
