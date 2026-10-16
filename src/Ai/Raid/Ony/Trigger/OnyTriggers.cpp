/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "OnyTriggers.h"

#include <cmath>

#include "OnyHelpers.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidTankDefensive.h"

using namespace OnyHelpers;

bool OnyxiaDodgeTrigger::IsActive() { return DodgeReasons(botAI) != 0; }

bool OnyxiaMainTankCooldownTrigger::IsActive()
{
    return MainTankCooldownWanted(botAI) && NextTankDefensive(botAI, bot, nullptr) != 0;
}

bool OnyxiaMainTankTrigger::IsActive()
{
    TankBranch const branch = MainTankBranch(botAI);
    if (branch == TankBranch::None)
        return false;

    if (branch != TankBranch::Hold && branch != TankBranch::Secure)
        return true;

    Unit* boss = OnyxiaSnapshotFor(botAI).boss;
    if (!boss)
        return false;

    // Building the lead is the generic tank rotation's job, this only makes sure he swings at her.
    if (branch == TankBranch::Secure)
        return bot->GetVictim() != boss;

    // On the spot: only facing and the swing need keeping.
    return bot->GetVictim() != boss || !bot->HasInArc(static_cast<float>(M_PI) / 2.0f, boss);
}

bool OnyxiaAntiFearTrigger::FearWindowActive() { return OnyHelpers::FearWindowActive(botAI); }

bool OnyxiaRedirectThreatTrigger::IsActive()
{
    if (bot->getClass() != CLASS_HUNTER && bot->getClass() != CLASS_ROGUE)
        return false;

    return RedirectTankFor(botAI) != nullptr;
}

bool OnyxiaChooseTargetTrigger::IsActive()
{
    Unit* target = OnyxiaPickTarget(botAI);
    return target && (bot->GetVictim() != target || AI_VALUE(Unit*, "current target") != target);
}

bool OnyxiaGroundSlotTrigger::IsActive()
{
    Position slot;
    return GroundSlotFor(botAI, slot) && bot->GetExactDist2d(&slot) > ONYXIA_SLOT_RETRIGGER;
}

bool OnyxiaAirSpotTrigger::IsActive()
{
    Position spot;
    return AirSpotFor(botAI, spot) && bot->GetExactDist2d(&spot) > ONYXIA_SLOT_RETRIGGER;
}

bool OnyxiaRearFlankTrigger::IsActive()
{
    Position point;
    return RearFlankPointFor(botAI, point, true);
}
