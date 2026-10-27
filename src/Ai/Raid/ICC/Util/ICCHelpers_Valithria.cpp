/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCHelpers_Valithria.h"
#include "ICCHelpers_Shared.h"
#include "Playerbots.h"
#include "RaidObs.h"
#include <algorithm>

std::vector<Creature*> IccValithriaPortals(Player* bot)
{
    std::vector<Creature*> portals = IccGetCreaturesByEntries(
        bot, {NPC_DREAM_PORTAL, NPC_DREAM_PORTAL_PRE_EFFECT, NPC_NIGHTMARE_PORTAL, NPC_NIGHTMARE_PORTAL_PRE_EFFECT},
        100.0f);

    // A bot in Dream State is phased away from every portal, so it would read none mid-wave
    if (RaidObs::Active() && !bot->HasAura(SPELL_DREAM_STATE))
    {
        // A pre-effect lingers 6 s past its real portal with its visual gone, so it counts only while shown.
        constexpr uint32 SPELL_DREAM_PORTAL_VISUAL_PRE = 71304;
        constexpr uint32 SPELL_NIGHTMARE_PORTAL_VISUAL_PRE = 71986;
        char const* wave = "none";
        for (Creature* portal : portals)
        {
            uint32 const entry = portal->GetEntry();
            if (entry == NPC_DREAM_PORTAL || entry == NPC_NIGHTMARE_PORTAL)
            {
                wave = "open";
                break;
            }
            if (portal->HasAura(SPELL_DREAM_PORTAL_VISUAL_PRE) || portal->HasAura(SPELL_NIGHTMARE_PORTAL_VISUAL_PRE))
                wave = "pre";
        }
        RaidObs::NoteDerived(bot, "vd.wave", wave);
    }

    return portals;
}

std::optional<bool> IccValithriaShouldHealRaid(Player* bot)
{
    Group* group = bot->GetGroup();
    if (!group)
        return std::nullopt;

    std::vector<ObjectGuid> healerGuids;
    std::vector<ObjectGuid> druidGuids;
    int healerCount = 0;

    for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* member = itr->GetSource();
        if (!member || !member->IsAlive() || IsSelfBot(bot))
            continue;

        if (PlayerbotAI::IsHeal(member) && !IsSelfBot(bot))
        {
            healerCount++;
            healerGuids.push_back(member->GetGUID());
            if (member->getClass() == CLASS_DRUID)
                druidGuids.push_back(member->GetGUID());
        }
    }

    std::sort(healerGuids.begin(), healerGuids.end());
    std::sort(druidGuids.begin(), druidGuids.end());

    auto botGuidPos = std::find(healerGuids.begin(), healerGuids.end(), bot->GetGUID());
    if (botGuidPos == healerGuids.end())
    {
        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "vd.raidheal", "none");
        return std::nullopt;
    }

    int healerIndex = std::distance(healerGuids.begin(), botGuidPos);
    bool isDruid = (bot->getClass() == CLASS_DRUID);

    bool shouldHealRaid = false;
    int druidCount = druidGuids.size();

    if (druidCount > 0)
    {
        if (isDruid)
        {
            int raidHealersNeeded = healerCount > 3 ? 2 : 1;
            int druidIndex =
                std::distance(druidGuids.begin(), std::find(druidGuids.begin(), druidGuids.end(), bot->GetGUID()));
            shouldHealRaid = (druidIndex < raidHealersNeeded);
        }
        else if (healerCount > 3 && druidCount == 1)
        {
            if (healerIndex == (healerCount - 1))
                shouldHealRaid = true;
        }
    }
    else if (healerCount > 3)
        shouldHealRaid = (healerIndex >= (healerCount - 2));
    else
        shouldHealRaid = (healerIndex == (healerCount - 1));

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "vd.raidheal", shouldHealRaid ? "raid" : "boss");

    return shouldHealRaid;
}
