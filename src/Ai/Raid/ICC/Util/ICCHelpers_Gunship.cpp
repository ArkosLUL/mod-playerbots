/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCHelpers_Gunship.h"
#include "Playerbots.h"
#include "RaidObs.h"

Unit* IccGunshipEnemyCaptain(Player* bot, float range)
{
    for (uint32 const entry : {NPC_HIGH_OVERLORD_SAURFANG, NPC_MURADIN_BRONZEBEARD})
    {
        Unit* captain = bot->FindNearestCreature(entry, range);
        if (captain && captain->IsAlive() && captain->IsHostileTo(bot))
            return captain;
    }

    return nullptr;
}

bool IccGunshipCannonsFrozen(Player* bot)
{
    bool ownCannons = false;
    for (uint32 const entry : {NPC_CANNONA, NPC_CANNONH})
    {
        std::list<Creature*> cannons;
        bot->GetCreatureListWithEntryInGrid(cannons, entry, 100.0f);
        for (Creature* cannon : cannons)
        {
            if (!cannon || !cannon->IsFriendlyTo(bot))
                continue;

            ownCannons = true;
            if (cannon->HasAura(SPELL_BELOW_ZERO))
            {
                RaidObs::NoteDerived(bot, "gsb.phase", "frozen");
                return true;
            }
        }
    }

    if (ownCannons)
        RaidObs::NoteDerived(bot, "gsb.phase", "fight");
    return false;
}
