/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCHelpers_Svalna.h"
#include "Playerbots.h"
#include "RaidObs.h"

bool IccSvalnaShielded(Player* bot, Unit* svalna)
{
    bool const shielded = svalna->HasAura(SPELL_AETHER_SHIELD);
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "ss.shield", shielded ? "up" : "down");

    return shielded;
}
