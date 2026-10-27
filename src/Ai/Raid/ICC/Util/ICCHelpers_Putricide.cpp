/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCHelpers_Putricide.h"
#include "ICCScripts.h"
#include "Playerbots.h"
#include "RaidObs.h"
#include <string>

uint8 IccPutricidePhase(Player* bot, Unit* boss)
{
    uint8 const phase = boss->HealthAbovePct(80) ? 1 : (boss->HealthAbovePct(35) ? 2 : 3);
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "pp.phase", std::to_string(phase));

    return phase;
}

void IccPutricideForgetBloatFlee(Player* bot)
{
    auto& lastDir = IcecrownHelpers::IccState(bot->GetMap()->GetInstanceId()).ppBloatLastDir;
    if (lastDir.erase(bot->GetGUID()) && RaidObs::Active())
        RaidObs::NoteDerived(bot, "pp.bloatflee", "none");
}
