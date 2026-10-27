/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCTriggers_Gunship.h"
#include "ICCHelpers_Gunship.h"
#include "Playerbots.h"
#include "Trigger.h"
#include "Vehicle.h"

bool IccInCannonTrigger::IsActive()
{
    Unit* vehicleBase = bot->GetVehicleBase();
    Vehicle* vehicle = bot->GetVehicle();
    if (!vehicleBase || !vehicle)
        return false;

    uint32 entry = vehicleBase->GetEntry();
    return entry == NPC_CANNONA || entry == NPC_CANNONH;
}

bool IccGunshipCannonNearTrigger::IsActive()
{
    if (bot->GetVehicle() || !PlayerbotAI::IsDps(bot))
        return false;

    if (!bot->FindNearestCreature(NPC_CANNONA, 100.0f) && !bot->FindNearestCreature(NPC_CANNONH, 100.0f))
        return false;

    return !IccGunshipCannonsFrozen(bot);
}

// Keyed on the enemy captain rather than on cannon friendliness, which the cannon-near trigger uses.
bool IccGunshipRocketJumpTrigger::IsActive() { return IccGunshipEnemyCaptain(bot, 100.0f) != nullptr; }

bool IccGunshipRocketPackSetupTrigger::IsActive()
{
    // Fires any time a bot is standing on a friendly gunship deck, regardless of
    // combat state. Lets bots walk to Zafod and equip the rocket pack before the
    // encounter starts (and keep it ready if they acquire it mid-fight).
    Unit* cannonA = bot->FindNearestCreature(NPC_CANNONA, 100.0f);
    if (cannonA && cannonA->IsFriendlyTo(bot))
        return true;

    Unit* cannonH = bot->FindNearestCreature(NPC_CANNONH, 100.0f);
    if (cannonH && cannonH->IsFriendlyTo(bot))
        return true;

    return false;
}
