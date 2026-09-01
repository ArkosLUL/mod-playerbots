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
    if (!AuriayaEncounterActive(botAI))
        return false;

    // Check if bot is on the floor
    return bot->GetPositionZ() < ULDUAR_AURIAYA_AXIS_Z_PATHING_ISSUE_DETECT;
}

//
// Auriaya
//
bool AuriayaSeepingEssenceTrigger::IsActive()
{
    if (!AuriayaEncounterActive(botAI))
        return false;

    return !CollectAuriayaEssencePools(bot, ULDUAR_AURIAYA_SEEPING_ESSENCE_RADIUS).empty();
}

bool AuriayaRaidPositionTrigger::IsActive()
{
    // Combat-gated: an anchor that fires on sight has the raid walking to its spots before anyone
    // has pulled.
    if (!IsAuriayaEngaged(botAI))
        return false;

    // Standing in a pool beats standing on a spot. Fear and stuns need no check here - CanFreeMove
    // already gates the action. Sonic Screech deliberately gets no stand-down: the cast is exactly
    // when everyone needs to be on their anchor splitting it.
    AuriayaSeepingEssenceTrigger seepingEssence(botAI);
    if (seepingEssence.IsActive())
        return false;

    Position anchor;
    float tolerance = 0.0f;
    if (!GetAuriayaAnchor(botAI, bot, anchor, tolerance))
        return false;

    // A fouled anchor is worth nothing. Without this the dodge and the anchor take turns: the bot
    // steps clear, the essence trigger goes quiet, and this one walks it straight back into the pool.
    // Searching off the boss rather than the bot, because the bot is by definition off its anchor
    // here and the pool that fouls it can be out of its own reach.
    for (Unit* pool : CollectAuriayaEssencePools(GetAuriaya(botAI), ULDUAR_AURIAYA_ROOM_SEARCH_RADIUS))
        if (pool->GetExactDist2d(&anchor) < ULDUAR_AURIAYA_SEEPING_ESSENCE_RADIUS)
            return false;

    return bot->GetExactDist2d(&anchor) > tolerance;
}

bool AuriayaSetDpsPriorityTrigger::IsActive()
{
    // The action calls Attack() directly, so without the combat gate the first bot to lay eyes on her
    // pulls the room. Whoever pulls flips this for everyone, which is what makes the raid engage
    // together.
    if (!IsAuriayaEngaged(botAI))
        return false;

    return !botAI->IsTank(bot);
}

bool AuriayaSentryTauntTrigger::IsActive()
{
    // A taunt is a pull, so the off-tank waits for the encounter to be live like everyone else.
    if (!IsAuriayaEngaged(botAI))
        return false;

    if (!botAI->IsAssistTankOfIndex(bot, 0, true))
        return false;

    // A sentry already on the off-tank stays in melee, where its Savage Pounce (8-25 yd) cannot fire
    return GetAuriayaLooseSentry(botAI, bot) != nullptr;
}
