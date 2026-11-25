#include "UldTriggers_FlameLeviathan.h"

#include "GameObject.h"
#include "Object.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "ScriptedCreature.h"
#include "SharedDefines.h"
#include "Trigger.h"
#include "UldEncounter_FlameLeviathan.h"
#include "UldScripts.h"
#include "Vehicle.h"

namespace
{
constexpr uint32 RIDEABLE_VEHICLES[] = {NPC_VEHICLE_CHOPPER, NPC_SALVAGED_DEMOLISHER, NPC_SALVAGED_DEMOLISHER_TURRET,
                                        NPC_SALVAGED_SIEGE_ENGINE, NPC_SALVAGED_SIEGE_ENGINE_TURRET};
}

bool FlameLeviathanOnVehicleTrigger::IsActive()
{
    Unit* vehicleBase = bot->GetVehicleBase();
    Vehicle* vehicle = bot->GetVehicle();
    if (!vehicleBase || !vehicle)
        return false;

    uint32 entry = vehicleBase->GetEntry();
    for (uint32 rideable : RIDEABLE_VEHICLES)
        if (entry == rideable)
            return true;

    return false;
}

bool FlameLeviathanOnSeatTrigger::IsActive()
{
    return FlameLeviathanSeatOf(bot) == FlameLeviathanSeat::LeviathanSeat;
}

bool FlameLeviathanVehicleNearTrigger::IsActive() { return FlameLeviathanVehicleToBoard(botAI) != nullptr; }

bool FlameLeviathanFlameVentsTrigger::IsActive()
{
    Unit* vehicleBase = bot->GetVehicleBase();
    if (!vehicleBase || vehicleBase->GetEntry() != NPC_SALVAGED_SIEGE_ENGINE)
        return false;

    if (!FlameLeviathanIsVentChanneling(FlameLeviathanBoss(botAI)))
        return false;

    return FlameLeviathanIsVentInterrupter(botAI, bot);
}

bool FlameLeviathanDriveUrgentTrigger::IsActive()
{
    if (!FlameLeviathanIsDriver(bot) || !FlameLeviathanEngaged(botAI))
        return false;

    bool const stunned = FlameLeviathanShutdown(FlameLeviathanBoss(botAI));
    if (!stunned && (FlameLeviathanIsPursued(bot) || FlameLeviathanIsRamTarget(bot)))
        return true;

    if (FlameLeviathanShouldClearBatteringRam(botAI, bot))
        return true;

    Unit* vehicleBase = bot->GetVehicleBase();
    return GetFlameLeviathanNearestTowerHazard(botAI, vehicleBase, FL_TOWER_ALL) != nullptr;
}
