/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "ICCActions_SindragosaAdds.h"
#include "ICCEncounterGate.h"
#include "ICCHelpers_SindragosaAdds.h"
#include "ICCSeams.h"
#include "ICCTriggers_SindragosaAdds.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
void DefineSindragosaGauntlet(EncounterBuilder& e)
{
    e.Node<IccSgGauntletDodgeTrigger, IccSgGauntletDodgeAction>(ACTION_EMERGENCY + 6, EncounterRow::Mover);
    // The Sorceress's Ice Tomb stuns her victim, mostly a tank, for 5 s
    e.Node<IccSgGauntletKickTrigger, IccSgGauntletKickAction>(ACTION_RAID + 8);
    // A root inside a Frostblade's whirl holds the bot there for its 3.5k a second
    e.Node<IccSgGauntletFreeTrigger, IccSgGauntletFreeAction>(ACTION_RAID + 7);
    e.Node<IccSgGauntletTauntTrigger, IccSgGauntletTauntAction>(ACTION_RAID + 6);
    e.Node<IccSgGauntletTargetTrigger, IccSgGauntletTargetAction>(ACTION_RAID + 5);

    // Area damage stays with the generic pickers: thirty Broodlings drop with every wave
    e.OwnTargeting("icc sindragosa gauntlet targeting", Role::Any, &IccResolverPicks<&GetSgGauntletTarget>,
                   Family::DpsAssist | Family::TankAssist);
    // Reaching a target inside a Frostblade's whirl walks the bot back into it
    e.Block("icc sindragosa gauntlet blade hold", Role::Any, &IsSgGauntletBladeReachHeld,
            Family::Reach | Family::Charge);
    e.Block("icc sindragosa gauntlet avoid aoe", Role::Any, &IsSgGauntletInBlade, Family::AvoidAoe);

    e.Tick(&TickSgGauntletEncounter);
}
}  // namespace

EncounterDefinition const& IccSindragosaGauntletDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_SINDRAGOSA_GAUNTLET, BossStateGate, "sindragosa-gauntlet",
                                                &DefineSindragosaGauntlet);
    return definition;
}
