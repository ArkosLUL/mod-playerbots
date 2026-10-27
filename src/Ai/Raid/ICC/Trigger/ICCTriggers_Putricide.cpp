/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCTriggers_Putricide.h"
#include "ICCHelpers_Putricide.h"
#include "ICCHelpers_Shared.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RaidDifficulty.h"
#include "Trigger.h"
#include "Vehicle.h"

bool IccPutricideGrowingOozePuddleTrigger::IsActive()
{
    // Early return if boss doesn't exist
    Unit* boss = AI_VALUE2(Unit*, "find target", "professor putricide");
    if (!boss)
        return false;

    GuidVector const& npcs = AI_VALUE(GuidVector, "nearest hostile npcs");
    for (auto const& npc : npcs)
    {
        if (Unit* unit = botAI->GetUnit(npc))
        {
            const uint32 entry = unit->GetEntry();
            if (entry == NPC_GROWING_OOZE_PUDDLE)
                return true;
        }
    }

    return false;
}

bool IccPutricideVolatileOozeTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "volatile ooze");
    if (!boss)
        return false;

    if (botAI->HasAura("Gaseous Bloat", bot))
        return false;

    return true;
}

bool IccPutricideGasCloudTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "gas cloud");
    if (!boss)
        return false;

    Unit* boss1 = AI_VALUE2(Unit*, "find target", "volatile ooze");

    bool hasGaseousBloat = botAI->HasAura("Gaseous Bloat", bot);
    // each bloat is a new cast on a new victim, so a direction kept from the last one is stale
    if (!hasGaseousBloat)
        IccPutricideForgetBloatFlee(bot);

    if (hasGaseousBloat && boss1)
        return true;

    if (boss1)
        return false;

    return true;
}

bool IccPutricideMutatedPlagueTrigger::IsActive()
{
    return AI_VALUE2(Unit*, "find target", "professor putricide") != nullptr;
}

bool IccPutricideMalleableGooTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "professor putricide");
    if (!boss)
        return false;

    Difficulty const diff = GetRaidMapDifficulty(bot);

    // Heroic cheat buffs — apply to all group members (bots + real players)
    if (boss && boss->IsInCombat() && sPlayerbotAIConfig.EnableICCBuffs &&
        (diff == RAID_DIFFICULTY_10MAN_HEROIC || diff == RAID_DIFFICULTY_25MAN_HEROIC))
    {
        if (Group* buffGroup = bot->GetGroup())
        {
            for (GroupReference* itr = buffGroup->GetFirstMember(); itr; itr = itr->next())
            {
                Player* member = itr->GetSource();
                if (!member || !member->IsAlive() || !member->IsInWorld())
                    continue;

                IccApplyHeroicBuffToMember(bot, member, false, true, "pp.heroicbuff");

                if (PlayerbotAI::IsTank(member) && !member->HasAura(SPELL_SPITEFULL_FURY) &&
                    boss->GetVictim() != member)
                {
                    member->AddAura(SPELL_SPITEFULL_FURY, member);
                    IccNoteCheat(bot, "pp.spitefulfury", member);
                }
            }
        }
    }

    return true;
}

bool IccPutricideAbominationTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "professor putricide");
    if (!boss)
        return false;

    if (!PlayerbotAI::IsAssistTank(bot))
        return false;

    // Already piloting - keep action firing until vehicle drops.
    if (Unit* veh = bot->GetVehicleBase())
    {
        uint32 e = veh->GetEntry();
        if (e == NPC_MUTATED_ABOMINATION_10 || e == NPC_MUTATED_ABOMINATION_25)
            return true;
    }

    // Phase 3: boss takes toy back. No transformation.
    if (IccPutricidePhase(bot, boss) == 3)
        return false;

    // Someone else already piloting - do not drink.
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
        {
            Player* m = itr->GetSource();
            if (!m || m == bot || !m->IsAlive())
                continue;
            if (Unit* vb = m->GetVehicleBase())
            {
                uint32 e = vb->GetEntry();
                if (e == NPC_MUTATED_ABOMINATION_10 || e == NPC_MUTATED_ABOMINATION_25)
                    return false;
            }
        }
    }

    // Require at least one Growing Ooze Puddle nearby.
    GuidVector const npcs = AI_VALUE(GuidVector, "nearest hostile npcs");
    for (auto const& g : npcs)
    {
        if (Unit* u = botAI->GetUnit(g))
            if (u->GetEntry() == NPC_GROWING_OOZE_PUDDLE)
                return true;
    }

    return false;
}
