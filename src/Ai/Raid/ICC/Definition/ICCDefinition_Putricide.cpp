/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCDefinitions.h"

#include "Group.h"
#include "ICCActions_Putricide.h"
#include "ICCData.h"
#include "ICCEncounterGate.h"
#include "ICCHelpers_Shared.h"
#include "ICCMultipliers_Putricide.h"
#include "ICCTriggers_Putricide.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RaidDifficulty.h"
#include "Strategy.h"

namespace Family = RaidEncounterRules::Family;

namespace
{
// Playerbots.EnableICCBuffs on heroic, for every group member, humans too.
void TickPutricide(PlayerbotAI* botAI)
{
    if (!sPlayerbotAIConfig.EnableICCBuffs || !IccPutricideDefinition().Live(botAI))
        return;

    Player* bot = botAI->GetBot();
    Difficulty const diff = GetRaidMapDifficulty(bot);
    if (diff != RAID_DIFFICULTY_10MAN_HEROIC && diff != RAID_DIFFICULTY_25MAN_HEROIC)
        return;

    AiObjectContext* context = botAI->GetAiObjectContext();
    Unit* boss = AI_VALUE2(Unit*, "find target", "professor putricide");
    Group* group = bot->GetGroup();
    if (!boss || !boss->IsInCombat() || !group)
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

void DefinePutricide(EncounterBuilder& e)
{
    e.Node<IccPutricideVolatileOozeTrigger, IccPutricideVolatileOozeAction>(ACTION_RAID + 4);
    e.Node<IccPutricideGasCloudTrigger, IccPutricideGasCloudAction>(ACTION_RAID + 5);
    e.Node<IccPutricideGrowingOozePuddleTrigger, IccPutricideGrowingOozePuddleAction>(ACTION_RAID + 3);
    e.Node<IccPutricideMutatedPlagueTrigger, IccPutricideMutatedPlagueAction>(ACTION_RAID + 3);
    e.Node<IccPutricideMalleableGooTrigger, IccPutricideAvoidMalleableGooAction>(ACTION_RAID + 6);
    e.Node<IccPutricideAbominationTrigger, IccPutricideAbominationAction>(ACTION_RAID + 7);

    e.LiveMultiplier<IccAddsPutricideMultiplier>(Family::AnyAction);

    e.Tick(&TickPutricide);
}
}  // namespace

EncounterDefinition const& IccPutricideDefinition()
{
    static EncounterDefinition const definition(ICC_BOSS_PROFESSOR_PUTRICIDE, BossStateGate,
                                                "professor-putricide", &DefinePutricide);
    return definition;
}
