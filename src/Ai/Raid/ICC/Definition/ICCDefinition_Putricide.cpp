/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "Creature.h"
#include "Group.h"
#include "ICCActions_Putricide.h"
#include "ICCData.h"
#include "ICCEncounterGate.h"
#include "ICCHelpers_Putricide.h"
#include "ICCHelpers_Shared.h"
#include "ICCMultipliers_Putricide.h"
#include "ICCSeams.h"
#include "ICCTriggers_Putricide.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RaidDifficulty.h"
#include "RaidTankDefensive.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
bool IsPpDutyHeld(PlayerbotAI* botAI)
{
    return IsPpStacking(botAI) || IsPpPlagueCarrier(botAI) || IsPpPlagueReceiving(botAI);
}

void TickHeroicBuffs(PlayerbotAI* botAI)
{
    if (!sPlayerbotAIConfig.EnableICCBuffs)
        return;

    Player* bot = botAI->GetBot();
    Difficulty const diff = GetRaidMapDifficulty(bot);
    if (diff != RAID_DIFFICULTY_10MAN_HEROIC && diff != RAID_DIFFICULTY_25MAN_HEROIC)
        return;

    Creature* boss = GetPpBoss(botAI);
    Group* group = bot->GetGroup();
    if (!boss || !group)
        return;

    for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
    {
        Player* member = itr->GetSource();
        if (!member || !member->IsAlive() || !member->IsInWorld())
            continue;

        IccApplyHeroicBuffToMember(bot, member, false, true, "pp.heroicbuff");

        if (PlayerbotAI::IsTank(member) && !member->HasAura(SPELL_SPITEFULL_FURY) && boss->GetVictim() != member)
        {
            member->AddAura(SPELL_SPITEFULL_FURY, member);
            IccNoteCheat(bot, "pp.spitefulfury", member);
        }
    }
}

void TickPutricide(PlayerbotAI* botAI)
{
    TickPpEncounter(botAI);
    if (IccPutricideDefinition().Live(botAI))
        TickHeroicBuffs(botAI);
}

void DefinePutricide(EncounterBuilder& e)
{
    e.Node<IccPutricideDodgeTrigger, IccPutricideDodgeAction>(ACTION_EMERGENCY + 6, EncounterRow::Mover);
    e.Node<IccPutricideSwapTrigger, IccPutricideSwapAction>(ACTION_RAID + 11);
    e.Node<IccPutricideTankDefensiveTrigger, IccPutricideCastTankDefensiveAction>(ACTION_RAID + 10);
    e.Node<IccPutricideKiteTrigger, IccPutricideKiteAction>(ACTION_RAID + 9, EncounterRow::Mover);
    e.Node<IccPutricidePlagueTrigger, IccPutricidePlagueAction>(ACTION_RAID + 8, EncounterRow::Mover);
    e.Node<IccPutricideStackTrigger, IccPutricideStackAction>(ACTION_RAID + 7, EncounterRow::Mover);
    e.Node<IccPutricideAbominationTrigger, IccPutricideAbominationAction>(ACTION_RAID + 6, EncounterRow::Mover);
    e.Node<IccPutricideBrakeTrigger, IccPutricideBrakeStopAction>(ACTION_RAID + 5);
    e.Node<IccPutricideTargetTrigger, IccPutricideTargetAction>(ACTION_RAID + 4);
    e.Node<IccPutricideStandTrigger, IccPutricideStandAction>(ACTION_RAID + 3, EncounterRow::Mover);

    e.LiveMultiplier<IccPutricideBrakeMultiplier>(Family::Spell | Family::Melee);

    e.OwnTargeting("icc putricide targeting", Role::Any, &IccResolverPicks<&GetPpTarget>,
                   Family::DpsAssist | Family::TankAssist | Family::DpsAoe);
    e.Block("icc putricide brake hold", Role::Any, &IsPpBraked,
            Family::DpsAssist | Family::TankAssist | Family::DpsAoe | Family::AggressiveTarget |
                Family::AttackAnything | Family::AttackLeastHp | Family::AttackRti | Family::Follow |
                Family::MoveRandom);
    e.OwnMovement("icc putricide stand hold", Role::Any, &IsPpHoldingStand, Family::Attack | Family::ReachHeal,
                  {IccPutricideStandAction::Name, IccPutricideDodgeAction::Name, "reach party member to resurrect"});
    e.OwnMovement("icc putricide kite hold", Role::Any, &IsPpKiter, Family::Attack, {IccPutricideKiteAction::Name});
    e.OwnMovement("icc putricide duty hold", Role::Any, &IsPpDutyHeld, Family::Attack,
                  {IccPutricideStackAction::Name, IccPutricidePlagueAction::Name, IccPutricideDodgeAction::Name});
    e.OwnMovement("icc putricide driver hold", Role::Any, &IsPpDriver, 0,
                  {IccPutricideAbominationAction::Name, IccPutricideSwapAction::Name, IccPutricidePlagueAction::Name});
    e.Block("icc putricide spell movers", Role::Any, &IsPpMovementOwned,
            Family::Charge | Family::Blink | Family::Disengage);
    e.Block("icc putricide avoid aoe", Role::Any, &IsPpLive, Family::AvoidAoe);
    e.Block("icc putricide taunts", Role::Any, &IsPpLive, Family::Taunt);
    e.Block("icc putricide tank form", Role::Tank, &IsPpLive, 0, {"caster form"});
    e.Block("icc putricide tank defensive hold", Role::Tank, &IsPpTankDefensiveHeld, 0, HeldTankDefensiveNames());

    e.Tick(&TickPutricide);
}
}

EncounterDefinition const& IccPutricideDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_PROFESSOR_PUTRICIDE, BossStateGate, "professor-putricide",
                                                &DefinePutricide);
    return definition;
}
