/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "ICCActions_Svalna.h"
#include "ICCEncounterGate.h"
#include "ICCHelpers_Svalna.h"
#include "ICCSeams.h"
#include "ICCTriggers_Svalna.h"
#include "RaidTankDefensive.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
void DefineSvalnaGauntlet(EncounterBuilder& e)
{
    // All but the dodge sit below Svalna's rows: her captains rise while Corrupted Ymirjar may still fight
    e.Node<IccSvalnaGauntletDodgeTrigger, IccSvalnaGauntletDodgeAction>(ACTION_EMERGENCY + 6, EncounterRow::Mover);
    e.Node<IccSvalnaGauntletInterruptTrigger, IccSvalnaGauntletInterruptAction>(ACTION_RAID + 4);
    e.Node<IccSvalnaGauntletTankDefensiveTrigger, IccSvalnaGauntletCastTankDefensiveAction>(ACTION_RAID + 3);
    e.Node<IccSvalnaGauntletTauntTrigger, IccSvalnaGauntletTauntAction>(ACTION_RAID + 2);
    e.Node<IccSvalnaGauntletTargetTrigger, IccSvalnaGauntletTargetAction>(ACTION_RAID + 1);

    // Area damage stays with the generic pickers: the packs stand bunched and a Deathbringer raises five
    e.OwnTargeting("icc sister svalna gauntlet targeting", Role::Any, &IccResolverPicks<&GetSsGauntletTarget>,
                   Family::DpsAssist | Family::TankAssist);
    // Reaching a target inside a Whirlwind or a Frozen Orb walks the bot back in
    e.Block("icc sister svalna gauntlet reach hold", Role::Any, &IsSsGauntletReachHeld,
            Family::Reach | Family::Charge);
    // The tank buttons wait for a Ymirjar to have the tank low
    e.Block("icc sister svalna gauntlet tank defensive hold", Role::Tank, &IsSsGauntletTankDefensiveHeld, 0,
            HeldTankDefensiveNames());

    e.Tick(&TickSsGauntletEncounter);
}
}  // namespace

EncounterDefinition const& IccSvalnaGauntletDefinition()
{
    static EncounterDefinition const definition(
        ICC_BOSS_NONE,
        UnitGate<NPC_SS_BATTLE_MAIDEN, NPC_SS_DEATHBRINGER, NPC_SS_FROSTBINDER, NPC_SS_HUNTRESS, NPC_SS_WARLORD,
                 NPC_SS_CORRUPTED_YMIRJAR, NPC_SS_WARHAWK>,
        "svalna-gauntlet", &DefineSvalnaGauntlet);
    return definition;
}
