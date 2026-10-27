/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCHelpers_Festergut.h"
#include "ICCHelpers_Shared.h"
#include "Playerbots.h"
#include "RaidObs.h"

bool IccFestergutSporeCycle(Player* bot)
{
    bool const spores = IccAnyGroupMemberHasAura(bot, SPELL_GAS_SPORE);
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "fg.phase", spores ? "spore" : "normal");

    return spores;
}
