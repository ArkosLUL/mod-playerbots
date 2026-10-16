/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include <cmath>

#include "EncounterHelpers.h"
#include "OnyActions.h"
#include "OnyHelpers.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidTankDefensive.h"

using namespace OnyHelpers;

bool OnyxiaMainTankAction::Execute(Event /*event*/)
{
    TankBranch const branch = MainTankBranch(botAI);
    Unit* boss = OnyxiaSnapshotFor(botAI).boss;
    if (!boss || branch == TankBranch::None)
        return false;

    if (branch == TankBranch::Taunt && EncounterHelpers::CastClassTaunt(botAI, boss))
        return true;

    // Taunt failed or is down, or he is still building his lead: hit her and leave the moving to the
    // generic reach and facing.
    if (branch == TankBranch::Taunt || branch == TankBranch::Recover || branch == TankBranch::Secure)
        return bot->GetVictim() != boss && Attack(boss);

    // Acquiring her must not cost the walk, so this falls through.
    bool acted = bot->GetVictim() != boss && Attack(boss);

    Position const* destination = MainTankDestination(branch);
    if (!destination || bot->GetExactDist2d(destination) <= ONYXIA_TANK_SPOT_TOLERANCE)
    {
        if (!bot->HasInArc(static_cast<float>(M_PI) / 2.0f, boss))
        {
            bot->SetFacingToObject(boss);
            acted = true;
        }

        return acted;
    }

    // True only on the tick that issues the walk, so the threat buttons run on the ticks between.
    if (!MainTankWalkDue(bot))
        return acted;

    if (!MoveTo(ONYXIA_MAP_ID, destination->GetPositionX(), destination->GetPositionY(), destination->GetPositionZ(),
                false, false, false, false, MovementPriority::MOVEMENT_COMBAT, true))
        return acted;

    MarkMainTankWalk(bot);
    return true;
}

bool OnyxiaMainTankCooldownAction::Execute(Event /*event*/)
{
    uint32 const spell = NextTankDefensive(botAI, bot, "onyxia.defensive");
    return spell && botAI->CastSpell(spell, bot);
}

Player* OnyxiaRedirectThreatAction::GetRedirectTank() { return RedirectTankFor(botAI); }

Unit* OnyxiaRedirectThreatAction::GetThreatDumpTarget() { return OnyxiaSnapshotFor(botAI).boss; }
