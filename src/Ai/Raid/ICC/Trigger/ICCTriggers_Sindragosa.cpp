/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCTriggers_Sindragosa.h"
#include "EncounterHelpers.h"
#include "ICCHelpers_Shared.h"
#include "ICCHelpers_Sindragosa.h"
#include "ObjectAccessor.h"
#include "Playerbots.h"
#include "RaidObs.h"
#include "Trigger.h"

bool IccSindragosaGroupPositionTrigger::IsActive()
{
    Unit* boss = bot->FindNearestCreature(NPC_SINDRAGOSA, 200.0f);  // sindra
    if (!boss)
        return false;

    IccSindragosaPhaseId const phase = IccSindragosaPhase(bot, boss);

    bool const overAirStation = IccSindragosaOverAirStation(bot, boss, 50.0f);

    // Air phase: tanks use their Nitro Boosts to reach the tomb spots
    if (overAirStation && PlayerbotAI::IsTank(bot))
        EncounterHelpers::UseNitroBoosts(bot);

    // Last phase: the main tank must keep tanking, never run to a tomb spot.
    // Strip its Frost Beacon so the tomb-positioning logic doesn't apply. The
    // assist tank keeps its beacon and is allowed to move to the beacon spot.
    if (PlayerbotAI::IsMainTank(bot) && bot->HasAura(SPELL_FROST_BEACON) && phase == IccSindragosaPhaseId::Last &&
        IccRaidCheat(botAI, "sg.frostbeacon"))
        bot->RemoveAura(SPELL_FROST_BEACON);

    if (PlayerbotAI::IsTank(bot))
    {
        // Strip Mystic Buffet: the cheat that makes single-tanking P3 survivable.
        Aura* buffet = botAI->GetAura("mystic buffet", bot, false, false);
        if (buffet && IccRaidCheat(botAI, "sg.mysticbuffet"))
            bot->RemoveAura(buffet->GetId());

        // Shield tanks while a tomb is up, Blistering Cold is casting, or a last
        // phase beacon is out (tomb about to form, healers already scattering).
        bool const shield = (boss->HasUnitState(UNIT_STATE_CASTING) && IccBossCastingBlisteringCold(boss)) ||
                            (phase == IccSindragosaPhaseId::Last &&
                             IccAnyGroupMemberHasAura(bot, SPELL_FROST_BEACON)) ||
                            !IccGetCreaturesByEntries(bot, {NPC_TOMB1, NPC_TOMB2, NPC_TOMB3, NPC_TOMB4}, 150.0f).empty();
        if (shield && !bot->HasAura(SPELL_MAGIC_BARRIER))
        {
            if (IccRaidCheat(botAI, "sg.magicbarrier"))
                bot->AddAura(SPELL_MAGIC_BARRIER, bot);
        }
        else if (!shield && bot->HasAura(SPELL_MAGIC_BARRIER))
            bot->RemoveAura(SPELL_MAGIC_BARRIER);

        // Assist tanks generate no threat while another main tank is alive, so
        // they can never win the threat race at the pull; dropped on promotion.
        bool mtAlive = false;
        if (Group* group = bot->GetGroup())
        {
            for (GroupReference* itr = group->GetFirstMember(); itr; itr = itr->next())
            {
                Player* member = itr->GetSource();
                if (member && member != bot && member->IsAlive() && PlayerbotAI::IsMainTank(member))
                {
                    mtAlive = true;
                    break;
                }
            }
        }

        if (mtAlive && !PlayerbotAI::IsMainTank(bot))
        {
            if (!bot->HasAura(SPELL_NO_THREAT) && IccRaidCheat(botAI, "sg.nothreat"))
                bot->AddAura(SPELL_NO_THREAT, bot);
        }
        else if (bot->HasAura(SPELL_NO_THREAT))
            bot->RemoveAura(SPELL_NO_THREAT);
    }

    if (bot->HasAura(SPELL_FROST_BEACON) || overAirStation)
        return false;

    return true;
}

bool IccSindragosaFrostBeaconTrigger::IsActive()
{
    Unit* boss = bot->FindNearestCreature(NPC_SINDRAGOSA, 200.0f);
    if (!boss)
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    if (bot->HasAura(SPELL_FROST_BEACON))
        return true;

    Group::MemberSlotList const& groupSlot = group->GetMemberSlots();
    for (Group::member_citerator itr = groupSlot.begin(); itr != groupSlot.end(); ++itr)
    {
        Player* member = ObjectAccessor::FindPlayer(itr->guid);
        if (!member || !member->IsAlive())
            continue;

        if (member->HasAura(SPELL_FROST_BEACON))  // If any player has Frost Beacon, keep trigger active
            return true;
    }

    return false;
}

bool IccSindragosaHotTrigger::IsActive()
{
    if (!PlayerbotAI::IsHeal(bot))
        return false;

    if (bot->HasAura(SPELL_FROST_BEACON))
        return false;

    Unit* boss = bot->FindNearestCreature(NPC_SINDRAGOSA, 200.0f);
    if (!boss)
        return false;

    return IccAnyGroupMemberHasAura(bot, SPELL_FROST_BEACON);
}

bool IccSindragosaBlisteringColdTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "sindragosa");
    if (!boss)
        return false;

    if (PlayerbotAI::IsTank(bot))
        return false;

    // Don't move if any bot in group has ice tomb
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    float dist = bot->GetExactDist2d(boss->GetPositionX(), boss->GetPositionY());

    if (dist >= 33.0f)
        return false;

    bool isCasting = false;
    if (boss && boss->HasUnitState(UNIT_STATE_CASTING))
        isCasting = true;

    bool isBlisteringCold = IccBossCastingBlisteringCold(boss);

    return isCasting && isBlisteringCold;
}

bool IccSindragosaUnchainedMagicTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "sindragosa");
    if (!boss)
        return false;

    Aura* aura = botAI->GetAura("unchained magic", bot, false, false);
    if (!aura)
        return false;

    bool isBlisteringCold = IccBossCastingBlisteringCold(boss);

    if (boss && boss->HasUnitState(UNIT_STATE_CASTING) && isBlisteringCold)
        return false;

    return true;
}

bool IccSindragosaChilledToTheBoneTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "sindragosa");
    if (!boss)
        return false;

    Aura* aura = botAI->GetAura("Chilled to the Bone", bot, false, false);
    if (!aura)
        return false;

    bool isBlisteringCold = IccBossCastingBlisteringCold(boss);

    if (boss && boss->HasUnitState(UNIT_STATE_CASTING) && isBlisteringCold)
        return false;

    return true;
}

bool IccSindragosaMysticBuffetTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "sindragosa");
    if (!boss)
        return false;

    if (PlayerbotAI::IsTank(bot))
        return false;

    if (boss->GetVictim() == bot)
        return false;

    // Last phase only. Do not gate on the bot's own Mystic Buffet aura:
    // hiding sheds it, so aura-less bots must still run the action or nobody
    // is left to mark and kill the tomb once the raid has shed its stacks.
    if (IccSindragosaPhase(bot, boss) != IccSindragosaPhaseId::Last)
        return false;

    if (bot->HasAura(SPELL_FROST_BEACON))
        return false;

    // Blistering Cold takes priority over tomb-hiding in the last phase:
    // skip hiding so the bot can run to the safe spot instead.
    if (boss->HasUnitState(UNIT_STATE_CASTING) && IccBossCastingBlisteringCold(boss))
        return false;

    return true;
}

bool IccSindragosaFrostBombTrigger::IsActive()
{
    Unit* boss = bot->FindNearestCreature(NPC_SINDRAGOSA, 200.0f);
    if (!boss)
        return false;

    if (!bot->IsAlive())  // Skip if dead
        return false;

    // Tombed bots intentionally pass through: the action pins their group to
    // the current tomb's zone so when freed they don't migrate to the wrong
    // zone. The action returns false for tombed bots without moving them.
    if (IccSindragosaOverAirStation(bot, boss, 50.0f))
        return true;

    // closes the sg.losmove window the action writes while she's over a station
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "sg.losmove", "none");

    return false;
}
