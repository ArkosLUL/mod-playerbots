/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCTriggers_Gunship.h"

#include "ICCEncounterGate.h"
#include "InstanceScript.h"
#include "Playerbots.h"
#include "Transport.h"
#include "Vehicle.h"

namespace
{
constexpr float GSB_ZAFOD_RANGE = 150.0f;
}

bool IccGunshipDodgeTrigger::IsActive() { return GsbDodging(botAI); }

bool IccGunshipRocketJumpTrigger::IsActive()
{
    Position spot;
    GsbJump const plan = GsbJumpPlan(botAI, spot);
    return plan == GsbJump::Walk || plan == GsbJump::Fire;
}

bool IccGunshipTauntTrigger::IsActive() { return GsbTauntTarget(botAI) != nullptr; }

bool IccGunshipCannonNearTrigger::IsActive() { return GsbWantsCannon(botAI); }

bool IccInCannonTrigger::IsActive()
{
    Unit* vehicleBase = bot->GetVehicleBase();
    if (!vehicleBase || !bot->GetVehicle())
        return false;

    uint32 const entry = vehicleBase->GetEntry();
    return entry == NPC_CANNONA || entry == NPC_CANNONH;
}

bool IccGunshipStandTrigger::IsActive()
{
    Position spot;
    return GsbStandWalk(botAI, spot);
}

bool IccGunshipRocketPackSetupTrigger::IsActive()
{
    // Aboard the raid's own ship, before the battle or during it
    Transport* transport = bot->GetTransport();
    InstanceScript* instance = transport ? bot->GetInstanceScript() : nullptr;
    if (!instance || transport->GetGUID() != instance->GetGuidData(ICC_BOSS_GUNSHIP_BATTLE) || bot->GetVehicle() ||
        GsbHasRocketPack(bot))
        return false;

    return bot->FindNearestCreature(NPC_ZAFOD_BOOMBOX, GSB_ZAFOD_RANGE) != nullptr;
}
