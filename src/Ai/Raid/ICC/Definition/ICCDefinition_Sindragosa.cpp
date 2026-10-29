/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "Creature.h"
#include "Group.h"
#include "ICCActions_Sindragosa.h"
#include "ICCData.h"
#include "ICCEncounterGate.h"
#include "ICCHelpers_Shared.h"
#include "ICCHelpers_Sindragosa.h"
#include "ICCMultipliers_Sindragosa.h"
#include "ICCSeams.h"
#include "ICCTriggers_Sindragosa.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RaidDifficulty.h"
#include "RaidTankDefensive.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;
namespace Role = RaidEncounterRules::Role;

namespace
{
// Playerbots.EnableICCBuffs on heroic, for every group member, humans too.
void ApplyHeroicBuffs(PlayerbotAI* botAI)
{
    if (!sPlayerbotAIConfig.EnableICCBuffs || !IccSindragosaDefinition().Live(botAI))
        return;

    Player* bot = botAI->GetBot();
    Difficulty const diff = GetRaidMapDifficulty(bot);
    if (diff != RAID_DIFFICULTY_10MAN_HEROIC && diff != RAID_DIFFICULTY_25MAN_HEROIC)
        return;

    Creature* boss = GetSgBoss(botAI);
    Group* group = bot->GetGroup();
    if (!boss || !boss->IsInCombat() || !group)
        return;

    for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
    {
        Player* member = itr->GetSource();
        if (!member || !member->IsAlive() || !member->IsInWorld())
            continue;

        IccApplyHeroicBuffToMember(bot, member, false, true, "sg.heroicbuff");

        if (PlayerbotAI::IsMainTank(member) && boss->GetVictim() != member && !member->HasAura(SPELL_SPITEFULL_FURY))
        {
            member->AddAura(SPELL_SPITEFULL_FURY, member);
            IccNoteCheat(bot, "sg.spitefulfury", member);
        }
    }
}

void TickSindragosa(PlayerbotAI* botAI)
{
    TickSgEncounter(botAI);
    ApplyHeroicBuffs(botAI);
}

void DefineSindragosa(EncounterBuilder& e)
{
    e.Node<IccSindragosaColdTrigger, IccSindragosaColdAction>(ACTION_EMERGENCY + 6, EncounterRow::Mover);
    e.Node<IccSindragosaSplashTrigger, IccSindragosaSplashAction>(ACTION_EMERGENCY + 5, EncounterRow::Mover);
    e.Node<IccSindragosaBombHideTrigger, IccSindragosaBombHideAction>(ACTION_EMERGENCY + 4, EncounterRow::Mover);
    e.Node<IccSindragosaBeaconTrigger, IccSindragosaBeaconAction>(ACTION_EMERGENCY + 3, EncounterRow::Mover);
    e.Node<IccSindragosaTankDefensiveTrigger, IccSindragosaCastTankDefensiveAction>(ACTION_RAID + 10);
    e.Node<IccSindragosaTauntTrigger, IccSindragosaTauntAction>(ACTION_RAID + 9);
    e.Node<IccSindragosaBuffetHideTrigger, IccSindragosaBuffetHideAction>(ACTION_RAID + 8, EncounterRow::Mover);
    e.Node<IccSindragosaUnchainedTrigger, IccSindragosaUnchainedAction>(ACTION_RAID + 7, EncounterRow::Mover);
    e.Node<IccSindragosaFreedomTrigger, IccSindragosaFreedomAction>(ACTION_RAID + 6);
    e.Node<IccSindragosaBeaconHealTrigger, IccSindragosaBeaconHealAction>(ACTION_RAID + 5);
    e.Node<IccSindragosaHoldTrigger, IccSindragosaHoldAction>(ACTION_RAID + 4);
    // Above the stand: a bot takes a new pick before it walks back to its spot
    e.Node<IccSindragosaTargetTrigger, IccSindragosaTargetAction>(ACTION_RAID + 3);
    e.Node<IccSindragosaStandTrigger, IccSindragosaStandAction>(ACTION_RAID + 2, EncounterRow::Mover);

    e.LiveMultiplier<IccSindragosaBrakeMultiplier>(Family::Spell | Family::Melee);
    e.LiveMultiplier<IccSindragosaTombMultiplier>(Family::Spell | Family::Melee);

    e.OwnTargeting("icc sindragosa targeting", Role::Any, &IccResolverPicks<&GetSgTarget>,
                   Family::DpsAssist | Family::TankAssist | Family::DpsAoe);
    // A held, braked, chilled or hiding bot's resolver hands it nothing, so no picker may give her back, and with
    // no target nothing may walk it off to its master
    e.Block("icc sindragosa target hold", Role::Any, &IsSgTargetHeld,
            Family::DpsAssist | Family::TankAssist | Family::DpsAoe | Family::AggressiveTarget |
                Family::AttackAnything | Family::AttackLeastHp | Family::AttackRti | Family::Follow |
                Family::MoveRandom | Family::PetAttack);
    // Every spell a carrier finishes stacks Instability, and Backlash goes off when it drops
    e.Block("icc sindragosa unchained hold", Role::Any, &IsSgUnchainedHeld, Family::Spell,
            {IccSindragosaFreedomAction::Name, IccSindragosaBeaconHealAction::Name});
    // The stands are in reach of her and of the holder, and reach would walk a bot off its spot
    e.OwnMovement("icc sindragosa movement", Role::Any, &IsSgMovementOwned, Family::Attack);
    e.Block("icc sindragosa spell movers", Role::Any, &IsSgMovementOwned,
            Family::Charge | Family::Blink | Family::Disengage);
    // avoid aoe flees a few yards at most, against the forced runs
    e.Block("icc sindragosa avoid aoe", Role::Any, &IsSgLive, Family::AvoidAoe);
    // Only the holder's taunt back and the elected swap move her
    e.Block("icc sindragosa taunts", Role::Tank, &IsSgLive, Family::Taunt);
    // A braked tank has no target, and a druid's non-combat nodes drop bear form
    e.Block("icc sindragosa tank form", Role::Tank, &IsSgLive, 0, {"caster form"});
    // The tank buttons wait for the breath windows or a low tank
    e.Block("icc sindragosa tank defensive hold", Role::Tank, &IsSgTankDefensiveHeld, 0, HeldTankDefensiveNames());

    e.Tick(&TickSindragosa);
}
}  // namespace

EncounterDefinition const& IccSindragosaDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_SINDRAGOSA, BossStateGate, "sindragosa", &DefineSindragosa);
    return definition;
}
