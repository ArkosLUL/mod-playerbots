/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldDefinitions.h"

#include "Strategy.h"
#include "UldActions_XT002.h"
#include "UldEncounterGate.h"
#include "UldMultipliers_XT002.h"
#include "UldTriggers_XT002.h"

namespace Family = RaidEncounterRules::Family;

namespace
{
void DefineXT002(EncounterBuilder& e)
{
    // Stepping out of a Boombot blast or a Void Zone outranks everything else: both one-shot a bot that
    // stands in them. The debuff carrier sits above that because a carrier walking through the raid is
    // a mechanic nobody else can answer - the raid holds still and the carrier leaves on its own.
    //
    // Both are single nodes covering two mechanics apiece, and that is load-bearing. Two nodes on equal
    // relevance cannot share a bot: the engine ends the tick at the first action returning true and
    // leaves the loser queued, so the pair trade the tick and the bot walks the line between their
    // destinations. Nothing below shares a relevance with anything else here either.
    e.Node<XT002AvoidHazardTrigger, XT002AvoidHazardAction>(ACTION_EMERGENCY);
    e.Node<XT002DebuffCarrierTrigger, XT002DebuffCarrierAction>(ACTION_EMERGENCY + 1);

    // One action owns every bot's target, tanks included, so nothing is marked - a raid icon is
    // group-global and would overwrite whatever the player and the other bots are using. The Pummeller
    // taunt sits above it so add control keeps running through the Heart window.
    //
    // Positioning is deliberately the lowest node here. The engine ends a tick at the first action
    // that succeeds, so during an add wave the priority action starves it - which is the behaviour we
    // want, since killing a Scrapbot beats standing on a spot and the anchor has yards of tolerance.
    e.Node<XT002PummellerTauntTrigger, XT002PummellerTauntAction>(ACTION_RAID + 4);
    e.Node<XT002SetDpsPriorityTrigger, XT002SetDpsPriorityAction>(ACTION_RAID + 3);
    e.Node<XT002RedirectThreatTrigger, XT002RedirectThreatAction>(ACTION_RAID + 1);
    e.Node<XT002RaidPositionTrigger, XT002RaidPositionAction>(ACTION_RAID);

    // Both hold items as well as spells: the burst list has trinkets and potions on it, and the Heart
    // floor stops anything that would land a hit.
    e.Multiplier<XT002BurstWindowMultiplier>(Family::AnyAction);
    e.Multiplier<XT002TargetGuardMultiplier>(Family::AnyAction);
}
}  // namespace

EncounterDefinition const& UldXT002Definition()
{
    static EncounterDefinition const definition(ULD_BOSS_XT002, BossStateGate, "xt002", &DefineXT002);
    return definition;
}
