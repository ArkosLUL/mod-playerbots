#include "UldActions_Auriaya.h"
#include "UldActions_Shared.h"

#include <CombatStrategy.h>
#include <FollowMasterStrategy.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "AiObjectContext.h"
#include "CellImpl.h"
#include "DBCEnums.h"
#include "GameObject.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "LastMovementValue.h"
#include "ObjectGuid.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "Position.h"
#include "RaidCombatHelpers.h"
#include "RaidObs.h"
#include "UldEncounter_Auriaya.h"
#include "UldScripts.h"
#include "EncounterHelpers.h"
#include "ScriptedCreature.h"
#include "ServerFacade.h"
#include "Unit.h"
#include "Vehicle.h"
#include <TankAssistStrategy.h>

using namespace EncounterHelpers;

namespace
{
struct AnyUnitOfEntriesInRangeCheck
{
    Acore::AnyUnitInObjectRangeCheck inRange;
    uint32 const* entries;
    size_t count;

    bool operator()(Unit* unit)
    {
        return std::find(entries, entries + count, unit->GetEntry()) != entries + count && inRange(unit);
    }
};

bool FindAuriayaPoolEscape(Player* bot, Position const& center, std::vector<Unit*> const& pools, Position& out)
{
    struct Candidate
    {
        float x;
        float y;
        int inPool;
        int nearPool;
        float walk;
    };

    constexpr int directions = 8;
    constexpr float increment = 3.0f;

    std::vector<Candidate> candidates;
    auto const add = [&](float x, float y)
    {
        if (center.GetExactDist2d(x, y) > ULDUAR_AURIAYA_ESSENCE_LEASH)
            return;

        Candidate candidate{x, y, 0, 0, bot->GetExactDist2d(x, y)};
        for (Unit* pool : pools)
        {
            float const distance = pool->GetExactDist2d(x, y);
            if (distance < ULDUAR_AURIAYA_SEEPING_ESSENCE_RADIUS)
                ++candidate.inPool;

            if (distance < ULDUAR_AURIAYA_ESSENCE_CLEARANCE)
                ++candidate.nearPool;
        }

        candidates.push_back(candidate);
    };

    add(bot->GetPositionX(), bot->GetPositionY());
    add(center.GetPositionX(), center.GetPositionY());

    for (int i = 0; i < directions; ++i)
    {
        float const angle = (i * 2.0f * static_cast<float>(M_PI)) / directions;
        for (float distance = increment; distance <= ULDUAR_AURIAYA_ESSENCE_LEASH; distance += increment)
            add(bot->GetPositionX() + distance * std::cos(angle), bot->GetPositionY() + distance * std::sin(angle));
    }

    std::sort(candidates.begin(), candidates.end(),
              [](Candidate const& a, Candidate const& b)
              {
                  if (a.inPool != b.inPool)
                      return a.inPool < b.inPool;

                  if (a.nearPool != b.nearPool)
                      return a.nearPool < b.nearPool;

                  return a.walk < b.walk;
              });

    for (Candidate const& candidate : candidates)
    {
        if (candidate.walk < 0.5f)
        {
            out = bot->GetPosition();
            return true;
        }

        float x = candidate.x;
        float y = candidate.y;
        float z = bot->GetPositionZ();
        if (!bot->GetMap()->CheckCollisionAndGetValidCoords(bot, bot->GetPositionX(), bot->GetPositionY(),
                                                            bot->GetPositionZ(), x, y, z))
            continue;

        if (std::hypot(x - candidate.x, y - candidate.y) > 1.0f)
            continue;

        out = Position(x, y, z);
        return true;
    }

    return false;
}
}

bool AuriayaFallFromFloorAction::Execute(Event /*event*/)
{
    if (Unit* boss = GetAuriaya(botAI))
    {
        std::vector<Unit*> const pools = GetAuriayaRoomPools(botAI);

        Position spot;
        bool found = false;
        if (PlayerbotAI::IsMainTank(bot))
        {
            spot = ULDUAR_AURIAYA_MAINTANK_SPOTS[GetAuriayaStationIndex(pools)];
            found = true;
        }
        else if (PlayerbotAI::IsRanged(bot))
            found = PickAuriayaSoakSpotFromBoss(boss, pools, spot);

        if (found)
        {
            bot->NearTeleportTo(spot.GetPositionX(), spot.GetPositionY(), spot.GetPositionZ(), bot->GetOrientation());
            return true;
        }
    }

    Player* leader = GetRaidLeaderOnInstance(botAI);
    if (!leader || leader == bot)
        return false;

    bot->NearTeleportTo(leader->GetPositionX(), leader->GetPositionY(), leader->GetPositionZ(),
                        leader->GetOrientation());
    return true;
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

    std::vector<Unit*> const pools = GetAuriayaRoomPools(botAI);
    if (pools.empty())
        return false;

    Position anchor;
    float tolerance = 0.0f;
    bool const anchored = GetAuriayaAnchor(botAI, bot, boss, pools, anchor, tolerance);
    if (!anchored)
        anchor = Position(boss->GetPositionX(), boss->GetPositionY(), boss->GetPositionZ());

    Position destination;
    if (anchored && GetAuriayaPoolClearance(pools, anchor.GetPositionX(), anchor.GetPositionY()) >=
                        ULDUAR_AURIAYA_ESSENCE_CLEARANCE)
        destination = anchor;
    else if (!FindAuriayaPoolEscape(bot, anchor, pools, destination))
        return false;

    if (bot->GetExactDist2d(&destination) < 1.0f)
        return false;

    if (bot->IsMovementPreventedByCasting())
        bot->InterruptNonMeleeSpells(true);

    return MoveTo(bot->GetMapId(), destination.GetPositionX(), destination.GetPositionY(),
                  destination.GetPositionZ(), false, false, false, false, MovementPriority::MOVEMENT_FORCED);
}

bool AuriayaSentryTauntAction::isUseful()
{
    AuriayaSentryTauntTrigger auriayaSentryTauntTrigger(botAI);
    return auriayaSentryTauntTrigger.IsActive();
}

bool AuriayaSentryTauntAction::Execute(Event /*event*/)
{
    Unit* sentry = GetAuriayaLooseSentry(botAI, bot);
    if (!sentry)
        return false;

    if (!InAuriayaSentryTauntReach(bot, sentry))
        return MoveTo(sentry, ULDUAR_AURIAYA_SENTRY_TAUNT_WALK, MovementPriority::MOVEMENT_COMBAT);

    // Read the victim before taunting, the taunt moves it.
    Unit* victim = sentry->GetVictim();
    Player* victimPlayer = victim && sentry->IsInCombat() ? victim->ToPlayer() : nullptr;
    bool const fromTank = victimPlayer && PlayerbotAI::IsTank(victimPlayer);

    if (!CastClassTaunt(botAI, sentry))
        return false;

    if (!fromTank)
        AttackDirect(sentry);

    return true;
}

bool AuriayaOffTankTargetAction::isUseful()
{
    AuriayaOffTankTargetTrigger auriayaOffTankTargetTrigger(botAI);
    return auriayaOffTankTargetTrigger.IsActive();
}

bool AuriayaOffTankTargetAction::Execute(Event /*event*/) { return AttackDirect(GetAuriayaOffTankHoldSentry(bot)); }

bool AuriayaMainTankSentryAction::isUseful()
{
    AuriayaMainTankSentryTrigger auriayaMainTankSentryTrigger(botAI);
    return auriayaMainTankSentryTrigger.IsActive();
}

bool AuriayaMainTankSentryAction::Execute(Event /*event*/)
{
    Unit* boss = GetAuriaya(botAI);
    if (!boss)
        return false;

    bool taunted = false;
    if (ClassTauntReady(botAI))
        if (Unit* sentry = GetAuriayaMainTankSentry(bot))
            taunted = CastClassTaunt(botAI, sentry);

    return AttackDirect(boss) || taunted;
}

bool AuriayaRaidPositionAction::isUseful()
{
    AuriayaRaidPositionTrigger auriayaRaidPositionTrigger(botAI);
    return auriayaRaidPositionTrigger.IsActive();
}

bool AuriayaRaidPositionAction::Execute(Event /*event*/)
{
    Unit* boss = GetAuriaya(botAI);
    if (!boss)
        return false;

    Position anchor;
    float tolerance = 0.0f;
    if (!GetAuriayaAnchor(botAI, bot, boss, GetAuriayaRoomPools(botAI), anchor, tolerance))
        return false;

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
        if (IsDownOrFeigning(candidate))
            return false;

        if (PlayerbotAI::IsMelee(bot) &&
            candidate->GetExactDist2d(boss) > ULDUAR_AURIAYA_MELEE_DEFENDER_RANGE)
            return false;
    }

    return true;
}

bool AuriayaSetDpsPriorityAction::Execute(Event /*event*/)
{
    Unit* boss = GetAuriaya(botAI);
    if (!boss)
        return false;

    static uint32 const priorityOrder[] = {NPC_AURIAYA_SANCTUM_SENTRY, NPC_AURIAYA_FERAL_DEFENDER, NPC_AURIAYA};
    constexpr size_t priorityCount = sizeof(priorityOrder) / sizeof(priorityOrder[0]);
    constexpr float targetSwitchDistance = 10.0f;

    Unit* currentTarget = context->GetValue<Unit*>("current target")->Get();

    Unit* perEntry[priorityCount] = {nullptr};

    float const range = sPlayerbotAIConfig.SightDistance;
    std::vector<Unit*> candidates;
    AnyUnitOfEntriesInRangeCheck check{Acore::AnyUnitInObjectRangeCheck(bot, range), priorityOrder, priorityCount};
    Acore::UnitListSearcher<AnyUnitOfEntriesInRangeCheck> searcher(bot, candidates, check);
    Cell::VisitObjects(bot, searcher, range);

    for (Unit* unit : candidates)
    {
        if (!IsAllowedPriorityTarget(boss, unit) || !bot->IsWithinLOSInMap(unit))
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

    bool const sentryWanted = desiredPriority == 0;
    Unit* offTankSentry = sentryWanted ? GetAuriayaOffTankSentry(bot) : nullptr;
    if (offTankSentry && (!IsAllowedPriorityTarget(boss, offTankSentry) || !bot->IsWithinLOSInMap(offTankSentry)))
        offTankSentry = nullptr;

    if (offTankSentry)
        target = offTankSentry;

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "auriaya.focus", offTankSentry ? "offtank" : sentryWanted ? "nearest" : "none");

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

    if (!offTankSentry && currentPriority < priorityCount)
    {
        if (currentPriority < desiredPriority)
            target = currentTarget;
        else if (currentPriority == desiredPriority && target &&
                 target->GetExactDist2d(bot) + targetSwitchDistance >= currentTarget->GetExactDist2d(bot))
            target = currentTarget;
    }

    if (!target)
        target = AI_VALUE(Unit*, "dps target");

    if (!target)
        return false;

    bool needsAttack = currentTarget != target;
    if (PlayerbotAI::IsMelee(bot))
        needsAttack = needsAttack || !bot->HasUnitState(UNIT_STATE_MELEE_ATTACKING);

    if (!needsAttack)
        return false;

    return Attack(target);
}
