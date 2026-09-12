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

// Every boss lookup here sweeps every unit in sight, so the cheap tests in each trigger go first.

bool AuriayaFallFromFloorTrigger::IsActive()
{
    return bot->GetPositionZ() < ULDUAR_AURIAYA_AXIS_Z_PATHING_ISSUE_DETECT && AuriayaEncounterActive(botAI);
}

//
// Auriaya
//
bool AuriayaSeepingEssenceTrigger::IsActive()
{
    // 7 yd is a cell or two, so the pool search goes ahead of the boss.
    return !CollectAuriayaEssencePools(bot, ULDUAR_AURIAYA_SEEPING_ESSENCE_RADIUS).empty() &&
           AuriayaEncounterActive(botAI);
}

bool AuriayaRaidPositionTrigger::IsActive()
{
    // Only the main tank and the ranged half get an anchor.
    if (!botAI->IsMainTank(bot) && !botAI->IsRanged(bot))
        return false;

    // Combat-gated: an anchor that fires on sight has the raid walking to its spots before anyone
    // has pulled.
    Unit* boss = GetAuriaya(botAI);
    if (!boss || !boss->IsInCombat())
        return false;

    // Standing in a pool beats standing on a spot. Fear and stuns need no check here - CanFreeMove
    // already gates the action. Sonic Screech deliberately gets no stand-down: the cast is exactly
    // when everyone needs to be on their anchor splitting it.
    if (!CollectAuriayaEssencePools(bot, ULDUAR_AURIAYA_SEEPING_ESSENCE_RADIUS).empty())
        return false;

    // Searching off the boss rather than the bot, because the bot is by definition off its anchor
    // here and the pool that fouls it can be out of its own reach.
    std::vector<Unit*> const& roomPools = GetAuriayaRoomPools(botAI);

    Position anchor;
    float tolerance = 0.0f;
    if (!GetAuriayaAnchor(botAI, bot, boss, &roomPools, anchor, tolerance))
        return false;

    // A fouled anchor is worth nothing. Without this the dodge and the anchor take turns: the bot
    // steps clear, the essence trigger goes quiet, and this one walks it straight back into the pool.
    for (Unit* pool : roomPools)
        if (pool->GetExactDist2d(&anchor) < ULDUAR_AURIAYA_SEEPING_ESSENCE_RADIUS)
            return false;

    return bot->GetExactDist2d(&anchor) > tolerance;
}

bool AuriayaSetDpsPriorityTrigger::IsActive()
{
    if (botAI->IsTank(bot))
        return false;

    // The action calls Attack() directly, so without the combat gate the first bot to lay eyes on her
    // pulls the room. Whoever pulls flips this for everyone, which is what makes the raid engage
    // together.
    return IsAuriayaEngaged(botAI);
}

bool AuriayaSentryTauntTrigger::IsActive()
{
    // IsAssistTankOfIndex repeats this, but it walks the group, so it stays last and this goes first.
    if (!botAI->IsTank(bot))
        return false;

    // A taunt is a pull, so the off-tank waits for the encounter to be live like everyone else.
    if (!IsAuriayaEngaged(botAI))
        return false;

    if (!botAI->IsAssistTankOfIndex(bot, 0, true))
        return false;

    // A sentry already on the off-tank stays in melee, where its Savage Pounce (8-25 yd) cannot fire
    return GetAuriayaLooseSentry(botAI, bot) != nullptr;
}
