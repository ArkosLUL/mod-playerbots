/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "Group.h"
#include "ICCActions_Sindragosa.h"
#include "ICCData.h"
#include "ICCEncounterGate.h"
#include "ICCHelpers_Shared.h"
#include "ICCMultipliers_Sindragosa.h"
#include "ICCTriggers_Sindragosa.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RaidDifficulty.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;

namespace
{
// Playerbots.EnableICCBuffs on heroic, for every group member, humans too.
void TickSindragosa(PlayerbotAI* botAI)
{
    if (!sPlayerbotAIConfig.EnableICCBuffs || !IccSindragosaDefinition().Live(botAI))
        return;

    Player* bot = botAI->GetBot();
    Difficulty const diff = GetRaidMapDifficulty(bot);
    if (diff != RAID_DIFFICULTY_10MAN_HEROIC && diff != RAID_DIFFICULTY_25MAN_HEROIC)
        return;

    Unit* boss = bot->FindNearestCreature(NPC_SINDRAGOSA, 200.0f);
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

void DefineSindragosa(EncounterBuilder& e)
{
    e.Node<IccSindragosaGroupPositionTrigger, IccSindragosaGroupPositionAction>(ACTION_RAID + 1);
    e.Node<IccSindragosaFrostBeaconTrigger, IccSindragosaFrostBeaconAction>(ACTION_RAID + 5);
    e.Node<IccSindragosaHotTrigger, IccSindragosaHotAction>(ACTION_RAID + 6);
    e.Node<IccSindragosaBlisteringColdTrigger, IccSindragosaBlisteringColdAction>(ACTION_EMERGENCY + 4);
    e.Node<IccSindragosaUnchainedMagicTrigger, IccSindragosaUnchainedMagicAction>(ACTION_RAID + 2);
    e.Node<IccSindragosaChilledToTheBoneTrigger, IccSindragosaChilledToTheBoneAction>(ACTION_RAID + 2);
    e.Node<IccSindragosaMysticBuffetTrigger, IccSindragosaMysticBuffetAction>(ACTION_RAID + 3);
    e.Node<IccSindragosaFrostBombTrigger, IccSindragosaFrostBombAction>(ACTION_RAID + 7);

    e.LiveMultiplier<IccSindragosaMultiplier>(Family::AnyAction);

    e.Tick(&TickSindragosa);
}
}  // namespace

EncounterDefinition const& IccSindragosaDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_SINDRAGOSA, BossStateGate, "sindragosa", &DefineSindragosa);
    return definition;
}
