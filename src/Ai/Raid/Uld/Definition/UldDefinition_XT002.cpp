/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldDefinitions.h"

#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Strategy.h"
#include "UldActions_XT002.h"
#include "UldEncounterGate.h"
#include "UldMultipliers_XT002.h"
#include "UldScripts.h"
#include "UldTriggers_XT002.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
bool XT002TauntWasted(PlayerbotAI* botAI)
{
    Unit* target = botAI->GetAiObjectContext()->GetValue<Unit*>("current target")->Get();
    return target && target->GetEntry() == NPC_XT002;
}

void DefineXT002(EncounterBuilder& e)
{
    // Hazard dodge above the rest and the carrier above that, since only the carrier can solve its mechanic.
    // Each node covers two mechanics and no two nodes share a relevance: equal-relevance movers trade the tick.
    e.Node<XT002AvoidHazardTrigger, XT002AvoidHazardAction>(ACTION_EMERGENCY);
    e.Node<XT002DebuffCarrierTrigger, XT002DebuffCarrierAction>(ACTION_EMERGENCY + 1);

    // Taunt above targeting so add control runs through the Heart window. Positioning is lowest on purpose: an
    // add wave's priority action starves it, and killing a Scrapbot beats standing on a spot.
    e.Node<XT002PummellerTauntTrigger, XT002PummellerTauntAction>(ACTION_RAID + 4);
    e.Node<XT002SetDpsPriorityTrigger, XT002SetDpsPriorityAction>(ACTION_RAID + 3);
    e.Node<XT002RedirectThreatTrigger, XT002RedirectThreatAction>(ACTION_RAID + 1);
    e.Node<XT002RaidPositionTrigger, XT002RaidPositionAction>(ACTION_RAID);

    // Both hold items as well as spells: the burst list has trinkets and potions on it, and the Heart
    // floor stops anything that would land a hit.
    e.Multiplier<XT002BurstWindowMultiplier>(Family::AnyAction);
    e.Multiplier<XT002TargetGuardMultiplier>(Family::AnyAction);

    // The class "lose aggro" nodes taunt XT whenever another tank holds him, and he's taunt-immune, so a
    // paladin burns a global on Righteous Defense every 8 s. Taunts at a Pummeller still go out.
    e.Block("xt002 taunt guard", Role::Tank, XT002TauntWasted, Family::Taunt);
}
}  // namespace

EncounterDefinition const& UldXT002Definition()
{
    static EncounterDefinition const definition(ULD_BOSS_XT002, BossStateGate, "xt002", &DefineXT002);
    return definition;
}
