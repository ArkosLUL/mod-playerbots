#include "UldActions_Auriaya.h"
#include "UldActions_Shared.h"

#include <CombatStrategy.h>
#include <FollowMasterStrategy.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "AiObjectContext.h"
#include "DBCEnums.h"
#include "GameObject.h"
#include "Group.h"
#include "LastMovementValue.h"
#include "ObjectGuid.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "Position.h"
#include "UldBossHelper.h"
#include "UldScripts.h"
#include "EncounterHelpers.h"
#include "ScriptedCreature.h"
#include "ServerFacade.h"
#include "Unit.h"
#include "Vehicle.h"
#include <TankAssistStrategy.h>

using namespace EncounterHelpers;

bool AuriayaFallFromFloorAction::Execute(Event /*event*/)
{
    // Prefer the bot's own anchor: it is a known-good spot in the room, where the master may well be
    // standing out in the corridor.
    Position anchor;
    float tolerance = 0.0f;
    if (GetAuriayaAnchor(botAI, bot, anchor, tolerance))
    {
        return bot->TeleportTo(bot->GetMapId(), anchor.GetPositionX(), anchor.GetPositionY(),
                               anchor.GetPositionZ(), bot->GetOrientation());
    }

    Player* master = botAI->GetMaster();

    if (!master)
        return false;

    return bot->TeleportTo(bot->GetMapId(), master->GetPositionX(), master->GetPositionY(), master->GetPositionZ(),
                           master->GetOrientation());
}

bool AuriayaFallFromFloorAction::isUseful()
{
    AuriayaFallFromFloorTrigger auriayaFallFromFloorTrigger(botAI);
    return auriayaFallFromFloorTrigger.IsActive();
}

bool AuriayaSeepingEssenceAction::isUseful()
{
    AuriayaSeepingEssenceTrigger auriayaSeepingEssenceTrigger(botAI);
    return auriayaSeepingEssenceTrigger.IsActive();
}

bool AuriayaSeepingEssenceAction::Execute(Event /*event*/)
{
    Unit* boss = GetAuriaya(botAI);
    if (!boss)
        return false;

    // Wide enough that any pool able to invalidate a candidate inside the leash is in the list.
    std::vector<Unit*> const pools =
        CollectAuriayaEssencePools(bot, ULDUAR_AURIAYA_ESSENCE_LEASH + ULDUAR_AURIAYA_SEEPING_ESSENCE_RADIUS);
    if (pools.empty())
        return false;

    // Melee ride the boss and are not anchored, so they are leashed to her instead.
    Position anchor;
    float tolerance = 0.0f;
    if (!GetAuriayaAnchor(botAI, bot, anchor, tolerance))
        anchor = Position(boss->GetPositionX(), boss->GetPositionY(), boss->GetPositionZ());

    constexpr int directions = 8;
    constexpr float increment = 3.0f;

    bool found = false;
    float bestX = 0.0f;
    float bestY = 0.0f;
    float bestDisplacement = std::numeric_limits<float>::max();

    for (int i = 0; i < directions; ++i)
    {
        float const angle = (i * 2.0f * static_cast<float>(M_PI)) / directions;
        for (float distance = increment; distance <= ULDUAR_AURIAYA_ESSENCE_LEASH; distance += increment)
        {
            float const candX = bot->GetPositionX() + distance * std::cos(angle);
            float const candY = bot->GetPositionY() + distance * std::sin(angle);

            if (anchor.GetExactDist2d(candX, candY) > ULDUAR_AURIAYA_ESSENCE_LEASH)
                continue;

            bool clear = true;
            for (Unit* pool : pools)
            {
                if (pool->GetExactDist2d(candX, candY) < ULDUAR_AURIAYA_SEEPING_ESSENCE_RADIUS)
                {
                    clear = false;
                    break;
                }
            }

            if (!clear || !bot->IsWithinLOS(candX, candY, bot->GetPositionZ()))
                continue;

            // Smallest step that clears, not the furthest one from the pools - maximising distance is
            // what walks bots out of the room once the pools have piled up.
            if (distance < bestDisplacement)
            {
                bestDisplacement = distance;
                bestX = candX;
                bestY = candY;
                found = true;
            }
        }
    }

    if (found)
    {
        return MoveTo(bot->GetMapId(), bestX, bestY, bot->GetPositionZ(), false, false, false, false,
                      MovementPriority::MOVEMENT_COMBAT);
    }

    // Boxed in. FleePosition is navmesh-validated and refuses to reverse a recent flee, so it will not
    // start a ping-pong the way a raw MoveTo would.
    Unit* nearest = nullptr;
    float nearestDistance = std::numeric_limits<float>::max();
    for (Unit* pool : pools)
    {
        float const distance = bot->GetExactDist2d(pool);
        if (distance < nearestDistance)
        {
            nearestDistance = distance;
            nearest = pool;
        }
    }

    if (!nearest)
        return false;

    return FleePosition(nearest->GetPosition(), ULDUAR_AURIAYA_SEEPING_ESSENCE_RADIUS, 500);
}

bool AuriayaSentryTauntAction::isUseful()
{
    AuriayaSentryTauntTrigger auriayaSentryTauntTrigger(botAI);
    return auriayaSentryTauntTrigger.IsActive();
}

bool AuriayaSentryTauntAction::Execute(Event /*event*/)
{
    return UldCastClassTaunt(botAI, GetAuriayaLooseSentry(botAI, bot));
}

bool AuriayaRaidPositionAction::isUseful()
{
    AuriayaRaidPositionTrigger auriayaRaidPositionTrigger(botAI);
    return auriayaRaidPositionTrigger.IsActive();
}

bool AuriayaRaidPositionAction::Execute(Event /*event*/)
{
    Position anchor;
    float tolerance = 0.0f;
    if (!GetAuriayaAnchor(botAI, bot, anchor, tolerance))
        return false;

    // Parked. Returning false hands the tick back so the rotation still runs.
    if (bot->GetExactDist2d(&anchor) <= tolerance)
        return false;

    return MoveTo(bot->GetMapId(), anchor.GetPositionX(), anchor.GetPositionY(), anchor.GetPositionZ(), false,
                  false, false, false, MovementPriority::MOVEMENT_COMBAT);
}

bool AuriayaSetDpsPriorityAction::isUseful()
{
    AuriayaSetDpsPriorityTrigger auriayaSetDpsPriorityTrigger(botAI);
    return auriayaSetDpsPriorityTrigger.IsActive();
}

bool AuriayaSetDpsPriorityAction::IsAllowedPriorityTarget(Unit* boss, Unit* candidate)
{
    if (!candidate || !candidate->IsAlive())
        return false;

    if (candidate->GetEntry() == NPC_AURIAYA_FERAL_DEFENDER)
    {
        // Between lives it lies feigned at 1 HP and unselectable, so it is "alive" but unhittable.
        if (IsDownOrFeigning(candidate))
            return false;

        // It re-rolls aggro constantly and roams the room. Melee swing at it when it comes to them and
        // otherwise stay on the boss, or they spend the fight chasing it.
        if (PlayerbotAI::IsMelee(bot) &&
            candidate->GetExactDist2d(boss) > ULDUAR_AURIAYA_MELEE_DEFENDER_RANGE)
        {
            return false;
        }
    }

    return true;
}

bool AuriayaSetDpsPriorityAction::Execute(Event /*event*/)
{
    Unit* boss = GetAuriaya(botAI);
    if (!boss)
        return false;

    // Sentries first: they stay dead and their Strength of the Pack buffs the boss while they live.
    static uint32 const priorityOrder[] = {NPC_AURIAYA_SANCTUM_SENTRY, NPC_AURIAYA_FERAL_DEFENDER, NPC_AURIAYA};
    constexpr size_t priorityCount = sizeof(priorityOrder) / sizeof(priorityOrder[0]);
    constexpr float targetSwitchDistance = 10.0f;

    Unit* currentTarget = context->GetValue<Unit*>("current target")->Get();

    // Nearest live candidate of each entry.
    Unit* perEntry[priorityCount] = {nullptr};

    for (auto const& guid : AI_VALUE(GuidVector, "nearest npcs"))
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!IsAllowedPriorityTarget(boss, unit))
            continue;

        for (size_t index = 0; index < priorityCount; ++index)
        {
            if (unit->GetEntry() != priorityOrder[index])
                continue;

            Unit*& selected = perEntry[index];
            if (!selected || unit->GetExactDist2d(bot) < selected->GetExactDist2d(bot))
                selected = unit;

            break;
        }
    }

    Unit* target = nullptr;
    size_t desiredPriority = priorityCount;
    for (size_t index = 0; index < priorityCount; ++index)
    {
        if (perEntry[index])
        {
            target = perEntry[index];
            desiredPriority = index;
            break;
        }
    }

    size_t currentPriority = priorityCount;
    if (currentTarget && IsAllowedPriorityTarget(boss, currentTarget))
    {
        for (size_t index = 0; index < priorityCount; ++index)
        {
            if (currentTarget->GetEntry() == priorityOrder[index])
            {
                currentPriority = index;
                break;
            }
        }
    }

    if (currentPriority < priorityCount)
    {
        // Never downgrade off something at least as urgent as the new pick, and within one entry only
        // switch for something meaningfully closer - otherwise two sentries ping-pong the whole raid.
        if (currentPriority < desiredPriority)
            target = currentTarget;
        else if (currentPriority == desiredPriority && target &&
                 target->GetExactDist2d(bot) + targetSwitchDistance >= currentTarget->GetExactDist2d(bot))
        {
            target = currentTarget;
        }
    }

    if (!target)
        target = AI_VALUE(Unit*, "dps target");

    if (!target)
        return false;

    bool needsAttack = currentTarget != target;
    if (PlayerbotAI::IsMelee(bot))
        needsAttack = needsAttack || !bot->HasUnitState(UNIT_STATE_MELEE_ATTACKING);

    // Already on the right thing - yield so the lower nodes get the tick.
    if (!needsAttack)
        return false;

    return Attack(target);
}
