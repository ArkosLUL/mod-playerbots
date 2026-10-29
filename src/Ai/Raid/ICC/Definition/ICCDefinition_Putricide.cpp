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
bool IsPpDutyHeld(PlayerbotAI* botAI) { return IsPpStacking(botAI) || IsPpPlagueCarrier(botAI); }

// Playerbots.EnableICCBuffs on heroic, for every group member, humans too.
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
    // The taunt lands in the second before Mutated Plague, so nothing below may hold it back
    e.Node<IccPutricideSwapTrigger, IccPutricideSwapAction>(ACTION_RAID + 11);
    e.Node<IccPutricideTankDefensiveTrigger, IccPutricideCastTankDefensiveAction>(ACTION_RAID + 10);
    e.Node<IccPutricideKiteTrigger, IccPutricideKiteAction>(ACTION_RAID + 9, EncounterRow::Mover);
    e.Node<IccPutricidePlagueTrigger, IccPutricidePlagueAction>(ACTION_RAID + 8, EncounterRow::Mover);
    e.Node<IccPutricideStackTrigger, IccPutricideStackAction>(ACTION_RAID + 7, EncounterRow::Mover);
    e.Node<IccPutricideAbominationTrigger, IccPutricideAbominationAction>(ACTION_RAID + 6, EncounterRow::Mover);
    e.Node<IccPutricideBrakeTrigger, IccPutricideBrakeStopAction>(ACTION_RAID + 5);
    // Above the stand: a bot takes a new pick before it walks back to its spot
    e.Node<IccPutricideTargetTrigger, IccPutricideTargetAction>(ACTION_RAID + 4);
    e.Node<IccPutricideStandTrigger, IccPutricideStandAction>(ACTION_RAID + 3, EncounterRow::Mover);

    e.LiveMultiplier<IccPutricideBrakeMultiplier>(Family::Spell | Family::Melee);

    e.OwnTargeting("icc putricide targeting", Role::Any, &IccResolverPicks<&GetPpTarget>,
                   Family::DpsAssist | Family::TankAssist | Family::DpsAoe);
    // A braked bot's resolver hands it nothing on him, so no picker may give him back, and with no target
    // nothing may walk it off to its master
    e.Block("icc putricide brake hold", Role::Any, &IsPpBraked,
            Family::DpsAssist | Family::TankAssist | Family::DpsAoe | Family::AggressiveTarget |
                Family::AttackAnything | Family::AttackLeastHp | Family::AttackRti | Family::Follow |
                Family::MoveRandom);
    // Every slot is in reach of him already, and reach would walk a slot off the ring. Healers keep theirs
    // for the kiter and the ooze stack.
    e.OwnMovement("icc putricide stand hold", Role::Any, &IsPpHoldingStand, Family::Attack | Family::ReachHeal,
                  {IccPutricideStandAction::Name, IccPutricideDodgeAction::Name, "reach party member to resurrect"});
    e.OwnMovement("icc putricide kite hold", Role::Any, &IsPpKiter, Family::Attack, {IccPutricideKiteAction::Name});
    e.OwnMovement("icc putricide duty hold", Role::Any, &IsPpDutyHeld, Family::Attack,
                  {IccPutricideStackAction::Name, IccPutricidePlagueAction::Name, IccPutricideDodgeAction::Name});
    // The driver walks to the table and then drives: nothing else moves it, and it attacks only through the
    // abomination, or with a pickup taunt while still on foot
    e.OwnMovement("icc putricide driver hold", Role::Tank, &IsPpDriver, 0,
                  {IccPutricideAbominationAction::Name, IccPutricideSwapAction::Name});
    e.Block("icc putricide spell movers", Role::Any, &IsPpMovementOwned,
            Family::Charge | Family::Blink | Family::Disengage);
    // avoid aoe sees the puddles and flees them 5 yd at most, against the dodge
    e.Block("icc putricide avoid aoe", Role::Any, &IsPpLive, Family::AvoidAoe);
    // He obeys taunt diminishing returns, so only the swap spends a taunt on him
    e.Block("icc putricide taunts", Role::Any, &IsPpLive, Family::Taunt);
    // A grip drags the ooze or the cloud off the path the script gives it
    e.Block("icc putricide pulls", Role::Any, &IsPpLive, 0, {"death grip"});
    // A braked tank has no target, and a druid's non-combat nodes drop bear form
    e.Block("icc putricide tank form", Role::Tank, &IsPpLive, 0, {"caster form"});
    // The tank buttons wait for a phase 3 hold or a low tank
    e.Block("icc putricide tank defensive hold", Role::Tank, &IsPpTankDefensiveHeld, 0, HeldTankDefensiveNames());

    e.Tick(&TickPutricide);
}
}  // namespace

EncounterDefinition const& IccPutricideDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_PROFESSOR_PUTRICIDE, BossStateGate, "professor-putricide",
                                                &DefinePutricide);
    return definition;
}
