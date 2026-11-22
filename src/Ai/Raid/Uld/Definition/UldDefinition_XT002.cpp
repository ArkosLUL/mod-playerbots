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

bool XT002OwnsTankBuffs(PlayerbotAI* botAI) { return GetXT002BuffTank(botAI->GetBot(), GetXT002(botAI)) != nullptr; }

void DefineXT002(EncounterBuilder& e)
{
    e.Node<XT002AvoidHazardTrigger, XT002AvoidHazardAction>(ACTION_EMERGENCY);
    e.Node<XT002DebuffCarrierTrigger, XT002DebuffCarrierAction>(ACTION_EMERGENCY + 1);

    e.Node<XT002PummellerTauntTrigger, XT002PummellerTauntAction>(ACTION_RAID + 4);
    e.Node<XT002SetDpsPriorityTrigger, XT002SetDpsPriorityAction>(ACTION_RAID + 3);
    e.Node<XT002BombHealTrigger, XT002BombHealAction>(ACTION_RAID + 2);
    e.Node<XT002BombCarrierHealTrigger, XT002BombCarrierHealAction>(ACTION_RAID + 1.5f);
    e.Node<XT002RedirectThreatTrigger, XT002RedirectThreatAction>(ACTION_RAID + 1);
    e.Node<XT002RaidPositionTrigger, XT002RaidPositionAction>(ACTION_RAID);

    e.Node(
        "xt002 holder beacon trigger",
        [](PlayerbotAI* botAI) -> Trigger*
        { return new XT002HolderBuffTrigger(botAI, "xt002 holder beacon trigger", CLASS_PALADIN, "beacon of light"); },
        "xt002 holder beacon action",
        [](PlayerbotAI* botAI) -> Action*
        { return new XT002HolderBuffAction(botAI, "xt002 holder beacon action", "beacon of light"); },
        ACTION_CRITICAL_HEAL + 7);
    e.Node(
        "xt002 holder sacred shield trigger",
        [](PlayerbotAI* botAI) -> Trigger*
        {
            return new XT002HolderBuffTrigger(botAI, "xt002 holder sacred shield trigger", CLASS_PALADIN,
                                              "sacred shield");
        },
        "xt002 holder sacred shield action",
        [](PlayerbotAI* botAI) -> Action*
        { return new XT002HolderBuffAction(botAI, "xt002 holder sacred shield action", "sacred shield"); },
        ACTION_CRITICAL_HEAL + 2);
    e.Node(
        "xt002 holder earth shield trigger",
        [](PlayerbotAI* botAI) -> Trigger*
        {
            return new XT002HolderBuffTrigger(botAI, "xt002 holder earth shield trigger", CLASS_SHAMAN,
                                              "earth shield");
        },
        "xt002 holder earth shield action",
        [](PlayerbotAI* botAI) -> Action*
        { return new XT002HolderBuffAction(botAI, "xt002 holder earth shield action", "earth shield"); },
        ACTION_MEDIUM_HEAL + 6);
    e.Node(
        "xt002 holder power word shield trigger",
        [](PlayerbotAI* botAI) -> Trigger*
        {
            return new XT002HolderBuffTrigger(botAI, "xt002 holder power word shield trigger", CLASS_PRIEST,
                                              "power word: shield", "disc");
        },
        "xt002 holder power word shield action",
        [](PlayerbotAI* botAI) -> Action*
        { return new XT002HolderBuffAction(botAI, "xt002 holder power word shield action", "power word: shield"); },
        ACTION_MEDIUM_HEAL + 4.5f);
    e.Node(
        "xt002 holder renew trigger",
        [](PlayerbotAI* botAI) -> Trigger*
        { return new XT002HolderBuffTrigger(botAI, "xt002 holder renew trigger", CLASS_PRIEST, "renew"); },
        "xt002 holder renew action",
        [](PlayerbotAI* botAI) -> Action*
        { return new XT002HolderBuffAction(botAI, "xt002 holder renew action", "renew"); },
        ACTION_MEDIUM_HEAL + 4);

    e.Multiplier<XT002BurstWindowMultiplier>(Family::AnyAction);
    e.Multiplier<XT002TargetGuardMultiplier>(Family::AnyAction);
    e.LiveMultiplier<XT002ThreatBrakeMultiplier>(Family::Melee | Family::Spell);

    e.Block("xt002 taunt guard", Role::Tank, XT002TauntWasted, Family::Taunt);

    e.Block("xt002 class tank buffs", Role::Heal, XT002OwnsTankBuffs, 0,
            {"beacon of light on tank", "beacon of light on main tank", "sacred shield on tank",
             "sacred shield on main tank", "earth shield on main tank", "power word: shield on main tank",
             "renew on main tank"});
}
}

EncounterDefinition const& UldXT002Definition()
{
    static EncounterDefinition const definition(ULD_BOSS_XT002, BossStateGate, "xt002", &DefineXT002);
    return definition;
}
