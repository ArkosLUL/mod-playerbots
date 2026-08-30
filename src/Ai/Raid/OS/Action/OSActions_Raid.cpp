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

// Everyone else: wave and fissure dodges, the raid line, melee positioning, target selection
// and the threat redirect.

bool OsTsunamiCorridorAction::Execute(Event /*event*/)
{
    // Everyone but the main tank keeps the X they dodged from, so the raid line and whoever is holding
    // a drake stay where they are instead of drifting downrange every time a wave goes out.
    // The tank is the exception: his two holds are measured positions 1.3yd apart on X, and keeping his
    // own X would leave him beside the coordinate rather than on it.
    float const x = botAI->IsMainTank(bot) ? TankHoldX(bot) : bot->GetPositionX();
    return MoveToClamped(x, SafeCorridorY(bot), CORRIDOR_ARRIVAL_TOLERANCE, MovementPriority::MOVEMENT_FORCED);
}

bool OsAvoidTwilightFissureAction::Execute(Event /*event*/)
{
    // requireSelectable off: the fissure is permanently UNIT_FLAG_NOT_SELECTABLE, which is why this
    // dodge never once fired.
    Unit* fissure = FindUnitByEntries(bot, { NpcId::TwilightFissure, NpcId::TwilightFissureH },
                                      FISSURE_CLEAR_RADIUS, false);
    if (!fissure)
        return false;

    // The Y an X sidestep keeps: the bot's own, not the raid corridor's. For ranged the two are the
    // same; melee on a drake stand up to 28yd off it, and walking there to dodge a 5.5yd blast cost a
    // round trip on every fissure. A live wave still overrides, because that Y is not a choice.
    TsunamiWave const wave = ClassifyTsunamiWave(bot);
    float const stepY = WaveClearsY(bot->GetPositionY(), wave) ? bot->GetPositionY() : SafeCorridorY(bot);
    float const fx = fissure->GetPositionX();
    float const fy = fissure->GetPositionY();
    // Stepping to exactly the detection radius leaves the bot on the trigger boundary, where the
    // dodge fires again next tick.
    float const step = FISSURE_CLEAR_RADIUS + FISSURE_STEP_MARGIN;

    // The default list, for ranged and for melee: X steps first, because the corridor is 15yd wide and
    // the blast is 5.5yd, so sidestepping along X always clears it while keeping the Y a tsunami dodge
    // just bought. A bare MoveAway is free to pick Y, which walks the bot out of the corridor and
    // starts a fight with the corridor action. The Y steps are the rim fallback, for when both X
    // candidates clamp back onto the fissure. Both tanks replace it below - for them X is the axis that
    // swings a cone into the raid.
    std::array<std::pair<float, float>, 4> candidates = { {
        { fx + step, stepY },
        { fx - step, stepY },
        { bot->GetPositionX(), fy + step },
        { bot->GetPositionX(), fy - step },
    } };

    if (botAI->IsMainTank(bot))
    {
        // He is the one bot whose dodge turns the boss, and X is the axis that turns him into the raid:
        // a 10yd step east off his left hold swings the 60yd frontal cone from 138 degrees round to 66,
        // straight down the raid line at 57. South keeps both cones off it and puts him back on his
        // hold X on the way; north is no better than east, because PlatformMinX steps from 3220 to 3227
        // at Y 520 and turns the step into a 5.6yd move east.
        //
        // With a wave in the air he does not dodge at all. His safe band is 15yd on the left hold and
        // 5yd on the right, so no 10yd step fits inside either, and one Void Blast on a tank is a better
        // trade than a frontal cone on the whole ranged line. Returning false hands the tick down to the
        // corridor dodge, which owns his Y anyway.
        //
        // The X pair below is kept as the tail. Between waves he is always on the left hold, and the two
        // Y candidates are 20yd apart with 10yd of clearance each, so both only fail together for a
        // fissure that pins him against the southern rim.
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
        // His position is the drake's facing, and the 15yd Shadow Breath points at empty platform only
        // while he stands where the spot puts him. Shadron decides it: held from the west at
        // (3228.94, 534.66) the drake faces away, and a 10yd step east turns it onto the raid line,
        // whose west end is 10.2yd off on the same Y. A Y step turns it across the line instead.
        //
        // The X pair is kept as the tail, for a fissure that pins him against a rim. Both Y candidates
        // have to clamp or fall in a lane before it is reached.
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

        // Only the off-tank's Y pair can fail this. The X candidates carry stepY, which is wave-clear by
        // construction, and the main tank does not dodge at all while a wave is up.
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

    // Melee hold the northern flank, and the southern one is only the fallback. Both are equally safe
    // from the cones, so the side has to be picked on something else, and picking it on which survived
    // the platform clamp better - the old rule - is not a side at all: it follows the boss's facing, so
    // it hands melee the north flank while the tank is on his left hold and the south one the moment a
    // right wave swaps him over.
    float const bearing = boss->GetOrientation() + static_cast<float>(M_PI) / 2.0f;
    std::pair<float, float> north = { boss->GetPositionX() + reach * std::cos(bearing),
                                      boss->GetPositionY() + reach * std::sin(bearing) };
    // The other flank is this one reflected through him, which is exact and saves a second sin/cos.
    std::pair<float, float> south = { 2.0f * boss->GetPositionX() - north.first,
                                      2.0f * boss->GetPositionY() - north.second };

    if (south.second > north.second)
        std::swap(north, south);

    TsunamiWave const wave = ClassifyTsunamiWave(bot);

    for (auto const& flank : { north, south })
    {
        float x = flank.first;
        float y = flank.second;

        // The flank keeps X and a wave takes Y, so nothing pins melee sideways for a mechanic that
        // only kills along Y. His northern flank lands around Y 518.7, 5.3yd off the left line at 524,
        // so without this it walked them back into the wave the corridor dodge had just cleared.
        if (!WaveClearsY(y, wave))
            y = SafeCorridorY(bot);

        ClampDestination(x, y);

        // Re-checked after clamping: pulling the point back onto the platform can swing it into a
        // cone, and moving into one is worse than standing still.
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

    // Drawn off straight back rather than on it, so a pack of melee spreads across the tail instead of
    // stacking on one coordinate. The mirror is the fallback: Vesperon is held 2yd from the east rim,
    // so one side of the tail can clamp back out of the band.
    float const spread = frand(0.0f, DRAKE_REAR_DRAW_DEGREES) * static_cast<float>(M_PI) / 180.0f;
    float const rear = drake->GetOrientation() + static_cast<float>(M_PI);
    TsunamiWave const wave = ClassifyTsunamiWave(bot);

    for (float const bearing : { rear + spread, rear - spread })
    {
        float x = drake->GetPositionX() + reach * std::cos(bearing);
        float y = drake->GetPositionY() + reach * std::sin(bearing);

        // X keeps tracking the drake and only Y gives way. Without this the corridor dodge and this
        // action alternated for the whole 11s a wave was up: the dodge moves Y and then releases the
        // tick on its own duplicate-move guard, and this walked them straight back into the lane.
        if (!WaveClearsY(y, wave))
            y = SafeCorridorY(bot);

        ClampDestination(x, y);

        // The rear check only bites inside the breath. A wave puts melee well outside it, and the
        // off-tank drags the drake to the same corridor Y, so they are on its tail again as it lands.
        Position const candidate(x, y, bot->GetPositionZ(), 0.0f);
        if (drake->GetExactDist2d(x, y) <= DRAKE_BREATH_RANGE && !BehindDrake(drake, candidate))
            continue;

        return MoveToClamped(x, y, CORRIDOR_ARRIVAL_TOLERANCE);
    }

    return false;
}

bool OsReturnToPlatformAction::Execute(Event /*event*/)
{
    // No tolerance anywhere below: this only ever runs on a bot that is already off the arena, so
    // there is no "close enough" to settle for.
    //
    // The off-tank goes to both of his anchor's axes rather than to its X on the corridor Y. His hold
    // is a drake spot, so taking the corridor Y here only bought a second walk north once he was back
    // on the platform.
    if (IsOffTank(bot))
    {
        Position const anchor = OffTankAnchor(bot);
        return MoveToClamped(anchor.GetPositionX(), anchor.GetPositionY(), 0.0f, MovementPriority::MOVEMENT_FORCED);
    }

    float const x = botAI->IsMainTank(bot) ? TankHoldX(bot) : RaidLineX(bot);
    return MoveToClamped(x, SafeCorridorY(bot), 0.0f, MovementPriority::MOVEMENT_FORCED);
}

bool OsRaidHoldAction::Execute(Event /*event*/)
{
    float const corridorY = SafeCorridorY(bot);

    // Y first and on its own, keeping X: that is the correction a wave demands, and doing it without
    // touching X means the ranged group does not stampede along the line every time one goes out.
    if (std::abs(bot->GetPositionY() - corridorY) > CORRIDOR_ARRIVAL_TOLERANCE)
        return MoveToClamped(bot->GetPositionX(), corridorY);

    // X is frozen while a wave can still reach this bot, and thaws the moment it is past rather than
    // when it despawns. Re-forming the line is a sideways walk of up to 8yd, and doing it while the
    // group is dodging is what makes a bot look like it is drifting east mid-wave - the dodge above is
    // Y-only for exactly that reason.
    if (ClassifyTsunamiWave(bot) != TsunamiWave::None)
        return false;

    // The slot is 10yd from where a fissure dodge just put this bot, which is more than the line
    // tolerance forgives, so re-forming would land him back on the fissure he stepped off.
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

    // Ahead of the early return below, or a pet whose owner is already on the right target never
    // gets the order. Skipped while the owner is shifted: he resolves phase-16 adds his phase-1 pet
    // cannot touch, and leaving the last order standing keeps the pet on the drake it is hitting.
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
