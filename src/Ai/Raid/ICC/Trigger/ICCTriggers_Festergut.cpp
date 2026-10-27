/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCTriggers_Festergut.h"
#include "ICCHelpers_Festergut.h"
#include "ICCHelpers_Shared.h"
#include "ICCScripts.h"
#include "Playerbots.h"
#include "RaidObs.h"
#include "Trigger.h"

bool IccFestergutGroupPositionTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "festergut");
    if (!boss)
        return false;

    IccStripExperienced(bot);

    return true;
}

bool IccFestergutSporeTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "festergut");
    if (!boss || PlayerbotAI::IsTank(bot))
        return false;

    return IccFestergutSporeCycle(bot);
}

bool IccFestergutAvoidMalleableGooTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "festergut");
    if (!boss)
        return false;

    // Tanks hold the boss at the fixed tank spot; goo can land on tanks but
    // moving would lose threat and let goo land on melee stack anyway.
    if (PlayerbotAI::IsTank(bot))
        return false;

    // During spore phase, position switching handles goo avoidance — free-dodge
    // would pull bots out of their assigned spore spots.
    if (IccFestergutSporeCycle(bot))
        return false;

    constexpr uint32 impactLifetimeMs = 8000;
    constexpr float gooDangerRadius = 12.0f;

    uint32 now = getMSTime();
    float botX = bot->GetPositionX();
    float botY = bot->GetPositionY();
    ObjectGuid botGuid = bot->GetGUID();

    IcecrownHelpers::IccInstanceState& st = IcecrownHelpers::IccState(bot->GetMap()->GetInstanceId());

    for (auto const& impact : st.malleableGoo)
    {
        if (getMSTimeDiff(impact.castTime, now) > impactLifetimeMs)
            continue;
        float dx = botX - impact.position.GetPositionX();
        float dy = botY - impact.position.GetPositionY();
        if (dx * dx + dy * dy < gooDangerRadius * gooDangerRadius)
        {
            uint32 waitUntil = impact.castTime + impactLifetimeMs;
            auto& slot = st.festergutGooWaitUntil[botGuid];
            if (waitUntil > slot)
                slot = waitUntil;
            if (RaidObs::Active())
                RaidObs::NoteDerived(bot, "fg.goohold", "dodge");
            return true;
        }
    }

    auto it = st.festergutGooWaitUntil.find(botGuid);
    if (it != st.festergutGooWaitUntil.end())
    {
        if (now < it->second)
        {
            if (RaidObs::Active())
                RaidObs::NoteDerived(bot, "fg.goohold", "hold");
            return true;
        }
        st.festergutGooWaitUntil.erase(it);
        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "fg.goohold", "none");
    }

    return false;
}
