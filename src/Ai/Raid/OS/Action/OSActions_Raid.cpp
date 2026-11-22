/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "OSActions.h"
#include "OSTriggers.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "Random.h"
#include "Timer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

using namespace OsHelpers;
using namespace EncounterHelpers;

bool OsTsunamiCorridorAction::Execute(Event /*event*/)
{
    float const x = PlayerbotAI::IsMainTank(bot) ? TankHoldX(bot) : bot->GetPositionX();
    return MoveToClamped(x, SafeCorridorY(bot), CORRIDOR_ARRIVAL_TOLERANCE, MovementPriority::MOVEMENT_FORCED);
}

bool OsAvoidTwilightFissureAction::Execute(Event /*event*/)
{
    Unit* fissure = FindUnitByEntries(bot, { NpcId::TwilightFissure, NpcId::TwilightFissureH },
                                      FISSURE_CLEAR_RADIUS, false);
    if (!fissure)
        return false;

    TsunamiWave const wave = ClassifyTsunamiWave(bot);
    float const stepY = WaveClearsY(bot->GetPositionY(), wave) ? bot->GetPositionY() : SafeCorridorY(bot);
    float const fx = fissure->GetPositionX();
    float const fy = fissure->GetPositionY();
    float const step = FISSURE_CLEAR_RADIUS + FISSURE_STEP_MARGIN;

    std::array<std::pair<float, float>, 4> candidates = { {
        { fx + step, stepY },
        { fx - step, stepY },
        { bot->GetPositionX(), fy + step },
        { bot->GetPositionX(), fy - step },
    } };

    if (PlayerbotAI::IsMainTank(bot))
    {
        if (wave != TsunamiWave::None)
            return false;

        float const holdX = TankHoldX(bot);
        candidates = { {
            { holdX, fy - step },
            { holdX, fy + step },
            { fx + step, stepY },
            { fx - step, stepY },
        } };
    }
    else if (IsOffTank(bot))
    {
        candidates = { {
            { bot->GetPositionX(), fy - step },
            { bot->GetPositionX(), fy + step },
            { fx + step, stepY },
            { fx - step, stepY },
        } };
    }

    float bestX = 0.0f;
    float bestY = 0.0f;
    float bestClearance = -1.0f;
    for (auto& candidate : candidates)
    {
        float x = candidate.first;
        float y = candidate.second;
        ClampDestination(x, y);

        if (!WaveClearsY(y, wave))
            continue;

        float const clearance = std::sqrt((x - fx) * (x - fx) + (y - fy) * (y - fy));
        if (clearance >= FISSURE_CLEAR_RADIUS)
            return MoveToClamped(x, y, 0.0f, MovementPriority::MOVEMENT_FORCED);

        if (clearance > bestClearance)
        {
            bestClearance = clearance;
            bestX = x;
            bestY = y;
        }
    }

    if (bestClearance < 0.0f)
        return false;

    return MoveToClamped(bestX, bestY, 0.0f, MovementPriority::MOVEMENT_FORCED);
}

bool OsSartharionFlankAction::Execute(Event /*event*/)
{
    Unit* boss = GetSartharion(bot);
    if (!boss)
        return false;

    float const reach = std::max(bot->GetMeleeRange(boss) - FLANK_STAND_BACK, 1.0f);

    float const bearing = boss->GetOrientation() + static_cast<float>(M_PI) / 2.0f;
    std::pair<float, float> north = { boss->GetPositionX() + reach * std::cos(bearing),
                                      boss->GetPositionY() + reach * std::sin(bearing) };
    std::pair<float, float> south = { 2.0f * boss->GetPositionX() - north.first,
                                      2.0f * boss->GetPositionY() - north.second };

    if (south.second > north.second)
        std::swap(north, south);

    TsunamiWave const wave = ClassifyTsunamiWave(bot);

    for (auto const& flank : { north, south })
    {
        float x = flank.first;
        float y = flank.second;

        if (!WaveClearsY(y, wave))
            y = SafeCorridorY(bot);

        ClampDestination(x, y);

        Position const candidate(x, y, bot->GetPositionZ(), 0.0f);
        if (InSartharionCone(boss, candidate))
            continue;

        return MoveToClamped(x, y, CORRIDOR_ARRIVAL_TOLERANCE);
    }

    return false;
}

bool OsDrakeRearAction::Execute(Event /*event*/)
{
    Unit* drake = bot->GetVictim();
    if (!drake || !IsDrakeEntry(drake->GetEntry()))
        return false;

    float const reach = std::max(bot->GetMeleeRange(drake) - FLANK_STAND_BACK, 1.0f);

    float const spread = frand(0.0f, DRAKE_REAR_DRAW_DEGREES) * static_cast<float>(M_PI) / 180.0f;
    float const rear = drake->GetOrientation() + static_cast<float>(M_PI);
    TsunamiWave const wave = ClassifyTsunamiWave(bot);

    for (float const bearing : { rear + spread, rear - spread })
    {
        float x = drake->GetPositionX() + reach * std::cos(bearing);
        float y = drake->GetPositionY() + reach * std::sin(bearing);

        if (!WaveClearsY(y, wave))
            y = SafeCorridorY(bot);

        ClampDestination(x, y);

        Position const candidate(x, y, bot->GetPositionZ(), 0.0f);
        if (drake->GetExactDist2d(x, y) <= DRAKE_BREATH_RANGE && !BehindDrake(drake, candidate))
            continue;

        return MoveToClamped(x, y, CORRIDOR_ARRIVAL_TOLERANCE);
    }

    return false;
}

bool OsReturnToPlatformAction::Execute(Event /*event*/)
{
    if (IsOffTank(bot))
    {
        Position const anchor = OffTankAnchor(bot);
        return MoveToClamped(anchor.GetPositionX(), anchor.GetPositionY(), 0.0f, MovementPriority::MOVEMENT_FORCED);
    }

    float const x = PlayerbotAI::IsMainTank(bot) ? TankHoldX(bot) : RaidLineX(bot);
    return MoveToClamped(x, SafeCorridorY(bot), 0.0f, MovementPriority::MOVEMENT_FORCED);
}

bool OsRaidHoldAction::Execute(Event /*event*/)
{
    float const corridorY = SafeCorridorY(bot);

    if (std::abs(bot->GetPositionY() - corridorY) > CORRIDOR_ARRIVAL_TOLERANCE)
        return MoveToClamped(bot->GetPositionX(), corridorY);

    if (ClassifyTsunamiWave(bot) != TsunamiWave::None)
        return false;

    float const lineX = RaidLineX(bot);
    if (FissureBlocks(bot, lineX, corridorY))
        return false;

    return MoveToClamped(lineX, corridorY, RAID_LINE_TOLERANCE_X);
}

Player* OsRedirectThreatAction::GetRedirectTank()
{
    return RedirectTankFor(botAI, bot);
}

Unit* OsRedirectThreatAction::GetThreatDumpTarget()
{
    return PriorityTarget(bot);
}

bool OsTranquilizeEnrageAction::Execute(Event /*event*/)
{
    Unit* target = TranquilizeTargetFor(bot);
    if (!target)
        return false;

    return botAI->CanCastSpell("tranquilizing shot", target) &&
           botAI->CastSpell("tranquilizing shot", target);
}

bool SartharionAttackPriorityAction::Execute(Event /*event*/)
{
    Unit* target = PriorityTarget(bot);

    if (bot->GetGuardianPet() && !HasTwilightShift(bot))
    {
        if (target)
            CommandPetAttack(botAI, target);
        else
            StopPet(botAI);
    }

    if (target && bot->GetVictim() != target)
        return Attack(target);

    return false;
}
