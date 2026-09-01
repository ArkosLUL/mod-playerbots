/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldMultipliers_FlameLeviathan.h"

#include "AttackAction.h"
#include "BurstCooldowns.h"
#include "ChooseTargetActions.h"
#include "EncounterHelpers.h"
#include "FollowActions.h"
#include "GenericSpellActions.h"
#include "HunterActions.h"
#include "MageActions.h"
#include "MovementActions.h"
#include "PaladinActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "PriestActions.h"
#include "ReachTargetActions.h"
#include "RogueActions.h"
#include "SharedDefines.h"
#include "Spell.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "UldActions.h"
#include "UldData.h"
#include "UldEncounter_FlameLeviathan.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "UldTriggers.h"
#include "VehicleActions.h"

#include <set>
#include <string>

using namespace EncounterHelpers;

// Only the two main-tank redirects are blocked - casting to the shared BuffOnMainTankAction base
// would take Beacon of Light, Earth Shield and Thorns down with them.
//
// Entry lookups rather than "find target": that value only resolves creatures which already have
// this bot on their threat list, so a bot parked on one Iron Assembly member never sees the other
// two.
// Flame Leviathan
// The whole encounter is fought from vehicles and FlameLeviathanDriveAction is the only thing that
// steers one. Anything else that moves would fight it for the MotionMaster, so the generic movers
// are zeroed outright - but only once the boss is actually engaged, or the raid could never drive
// into the arena in the first place.
float FlameLeviathanVehicleMovementMultiplier::GetValue(Action* action)
{
    if (!action || bot->GetMapId() != ULDUAR_MAP_ID)
        return 1.0f;

    Unit* vehicleBase = bot->GetVehicleBase();
    if (!vehicleBase)
        return 1.0f;

    switch (vehicleBase->GetEntry())
    {
        case NPC_SALVAGED_SIEGE_ENGINE:
        case NPC_SALVAGED_SIEGE_ENGINE_TURRET:
        case NPC_SALVAGED_DEMOLISHER:
        case NPC_SALVAGED_DEMOLISHER_TURRET:
        case NPC_VEHICLE_CHOPPER:
            break;
        default:
            return 1.0f;
    }

    // One dynamic_cast up front: every rotation cast falls out here without touching the rest.
    if (!dynamic_cast<MovementAction*>(action))
        return 1.0f;

    // Boarding has to survive, or a bot that lost its vehicle can never take another one; and
    // leaving has to survive so nobody is welded in after the kill.
    if (dynamic_cast<FlameLeviathanDriveAction*>(action) ||
        dynamic_cast<FlameLeviathanEnterVehicleAction*>(action) || dynamic_cast<LeaveVehicleAction*>(action))
        return 1.0f;

    // Asked last, because it walks the target list: only once something would actually be blocked.
    return FlameLeviathanEngaged(botAI) ? 0.0f : 1.0f;
}
