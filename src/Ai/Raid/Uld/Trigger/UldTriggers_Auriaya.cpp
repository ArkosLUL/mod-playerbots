#include "UldTriggers_Auriaya.h"

#include "GameObject.h"
#include "Object.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldEncounter_Auriaya.h"
#include "UldScripts.h"
#include "EncounterHelpers.h"
#include "ScriptedCreature.h"
#include "SharedDefines.h"
#include "Trigger.h"
#include "Vehicle.h"
#include <MovementActions.h>
#include <FollowMasterStrategy.h>

#include <cmath>

bool AuriayaFallFromFloorTrigger::IsActive()
{
    return bot->GetPositionZ() < ULDUAR_AURIAYA_AXIS_Z_PATHING_ISSUE_DETECT && AuriayaEncounterActive(botAI);
}

bool AuriayaSeepingEssenceTrigger::IsActive()
{
    return IsInAuriayaPool(bot) && AuriayaEncounterActive(botAI);
}

bool AuriayaRaidPositionTrigger::IsActive()
{
    if (!PlayerbotAI::IsMainTank(bot) && !PlayerbotAI::IsRanged(bot))
        return false;

    Unit* boss = GetAuriaya(botAI);
    if (!boss || !boss->IsInCombat())
        return false;

    if (IsInAuriayaPool(bot))
        return false;

    std::vector<Unit*> const roomPools = GetAuriayaRoomPools(botAI);

    Position anchor;
    float tolerance = 0.0f;
    if (!GetAuriayaAnchor(botAI, bot, boss, roomPools, anchor, tolerance))
        return false;

    if (GetAuriayaPoolClearance(roomPools, anchor.GetPositionX(), anchor.GetPositionY()) <
        ULDUAR_AURIAYA_SEEPING_ESSENCE_RADIUS)
        return false;

    return bot->GetExactDist2d(&anchor) > tolerance;
}

bool AuriayaSetDpsPriorityTrigger::IsActive()
{
    if (PlayerbotAI::IsTank(bot))
        return false;

    if (!IsAuriayaEngaged(botAI))
        return false;

    return !AuriayaSentriesUnengaged(botAI) || !EncounterHelpers::GetGroupAssistTank(bot, 0);
}

bool AuriayaSentryTauntTrigger::IsActive()
{
    if (!PlayerbotAI::IsTank(bot))
        return false;

    if (!IsAuriayaEngaged(botAI))
        return false;

    if (!PlayerbotAI::IsAssistTankOfIndex(bot, 0, true))
        return false;

    Unit* sentry = GetAuriayaLooseSentry(botAI, bot);
    if (!sentry)
        return false;

    return !InAuriayaSentryTauntReach(bot, sentry) || EncounterHelpers::ClassTauntReady(botAI);
}

bool AuriayaOffTankTargetTrigger::IsActive()
{
    if (!AuriayaOffTankHoldsSentries(botAI))
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (target && target->IsAlive() && target->GetEntry() == NPC_AURIAYA_SANCTUM_SENTRY)
        return false;

    return GetAuriayaOffTankHoldSentry(bot) != nullptr;
}

bool AuriayaMainTankSentryTrigger::IsActive()
{
    if (!AuriayaMainTankAlone(botAI))
        return false;

    if (AI_VALUE(Unit*, "current target") != GetAuriaya(botAI))
        return true;

    return EncounterHelpers::ClassTauntReady(botAI) && GetAuriayaMainTankSentry(bot);
}
