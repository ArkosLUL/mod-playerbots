/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCHelpers_BloodQueen.h"
#include "Playerbots.h"
#include "RaidObs.h"

IccBqlPhase IccBqlPhaseOf(Player* bot, Unit* boss)
{
    IccBqlPhase phase = IccBqlPhase::Ground;
    Creature* queen = boss->ToCreature();
    if (queen && queen->HasReactState(REACT_PASSIVE))
        phase = boss->IsLevitating() ? IccBqlPhase::Air : IccBqlPhase::Center;

    if (RaidObs::Active())
    {
        static char const* const NAMES[] = {"ground", "center", "air"};
        RaidObs::NoteDerived(bot, "bql.phase", NAMES[static_cast<uint8>(phase)]);
    }

    return phase;
}
