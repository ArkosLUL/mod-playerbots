#include "UldActions_Mimiron.h"
#include "UldActions_Shared.h"

#include <CombatStrategy.h>
#include <FollowMasterStrategy.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <limits>

#include "AiObjectContext.h"
#include "DBCEnums.h"
#include "GameObject.h"
#include "Group.h"
#include "LastMovementValue.h"
#include "ObjectGuid.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RaidObs.h"
#include "RaidTankDefensive.h"
#include "Spell.h"
#include "Position.h"
#include "UldData.h"
#include "UldEncounter_Mimiron.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "UldTargets.h"
#include "EncounterHelpers.h"
#include "RtiValue.h"
#include "ScriptedCreature.h"
#include "ServerFacade.h"
#include "Unit.h"
#include "Vehicle.h"
#include <RtiTargetValue.h>
#include <TankAssistStrategy.h>

using namespace EncounterHelpers;

bool MimironFleeAction::MoveAwayClearOfMines(Unit* from, float distance, MovementPriority priority,
                                             bool fallbackUnfiltered, bool interrupt, char const* what,
                                             float clearRadius, bool sectorScreen, bool allowFire)
{
    if (!from)
        return false;

    return FleeFan(from->GetPosition(), from, distance, priority, fallbackUnfiltered, interrupt, what,
                   clearRadius, allowFire, sectorScreen);
}

bool MimironFleeAction::MoveAwayClearOfMines(Position const& from, float distance,
                                             MovementPriority priority, bool fallbackUnfiltered,
                                             bool interrupt, char const* what, float clearRadius,
                                             std::optional<float> toward)
{
    return FleeFan(from, nullptr, distance, priority, fallbackUnfiltered, interrupt, what,
                   clearRadius, false, false, toward);
}

bool MimironFleeAction::MoveTowardClearOfMines(Position const& dest, MovementPriority priority,
                                               bool fallbackUnfiltered, bool interrupt,
                                               char const* what, bool allowFire, bool sirenScreen)
{
    float const distance = bot->GetExactDist2d(dest.GetPositionX(), dest.GetPositionY());
    if (distance <= 0.0f)
        return false;

    Position const mirror(2.0f * bot->GetPositionX() - dest.GetPositionX(),
                          2.0f * bot->GetPositionY() - dest.GetPositionY(), bot->GetPositionZ());

    return FleeFan(mirror, nullptr, distance, priority, fallbackUnfiltered, interrupt, what, 0.0f,
                   allowFire, false, std::nullopt, sirenScreen);
}

bool MimironFleeAction::FleeFan(Position const& from, Unit* fallbackFrom, float distance,
                                MovementPriority priority, bool fallbackUnfiltered, bool interrupt,
                                char const* what, float clearRadius, bool allowFire, bool sectorScreen,
                                std::optional<float> toward, bool sirenScreen)
{
    if (distance <= 0.0f)
        return false;

    uint32 refusedBack = 0;
    uint32 refusedMine = 0;
    uint32 refusedCone = 0;
    uint32 refusedFire = 0;
    uint32 refusedBomb = 0;
    uint32 refusedRocket = 0;
    uint32 refusedBurst = 0;
    uint32 refusedShock = 0;
    uint32 refusedSpray = 0;
    uint32 refusedMove = 0;
    uint32 refusedHold = 0;
    uint32 refusedBombBot = 0;

    if (interrupt)
        bot->CastStop();

    UpdateMovementState();
    if (!IsMovingAllowed() || IsWaitingForLastMove(priority))
    {
        NoteFleeOutcome(what, "locked", nullptr, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0);
        return false;
    }

    float const holderRadius = GetMimironHolderRadius(botAI, bot);

    float const speed = bot->GetSpeed(MOVE_RUN);
    float const away = from.GetAngle(bot->GetPositionX(), bot->GetPositionY());
    float const started = bot->GetExactDist2d(from.GetPositionX(), from.GetPositionY());

    Unit* const vx001 = GetUldUnitByEntry(botAI, NPC_VX001);
    MimironBarrageWindow const barrage =
        vx001 ? GetMimironBarrageWindow(bot, vx001) : MimironBarrageWindow();
    MimironRapidBurstWindow const burst =
        vx001 ? GetMimironRapidBurstWindow(botAI, bot, vx001) : MimironRapidBurstWindow();
    MimironFirefighterHazards const hazards = GetMimironFirefighterHazards(botAI);
    MimironBurstSector const sector =
        sectorScreen && vx001 ? GetMimironOtherGroupSector(botAI, bot, vx001) : MimironBurstSector();

    float centre = 0.0f;
    if (toward)
    {
        float const pi = static_cast<float>(M_PI);
        float off = Position::NormalizeOrientation(*toward - away);
        if (off > pi)
            off -= 2.0f * pi;
        centre = std::clamp(off, -ULDUAR_MIMIRON_FLEE_FAN_MAX, ULDUAR_MIMIRON_FLEE_FAN_MAX);
    }
    float const sweep = toward ? 2.0f * ULDUAR_MIMIRON_FLEE_FAN_MAX : ULDUAR_MIMIRON_FLEE_FAN_MAX;

    std::vector<Position> const mines = GetMimironMines(botAI);
    std::vector<MimironBombBotBlast> const bombBotBlasts = GetMimironBombBotBlasts(botAI, bot);
    for (bool const mineClearWalk : {true, false})
    {
        if (mineClearWalk && mines.empty())
            continue;

        for (float delta = 0.0f; delta <= sweep + 0.001f; delta += ULDUAR_MIMIRON_FLEE_FAN_STEP)
        {
            for (float sign : {1.0f, -1.0f})
            {
                if (delta == 0.0f && sign < 0.0f)
                    continue;

                float const offAway = centre + sign * delta;
                if (std::fabs(offAway) > ULDUAR_MIMIRON_FLEE_FAN_MAX + 0.001f)
                    continue;

                float const angle = away + offAway;

                float hop = distance;
                if (clearRadius > 0.0f)
                {
                    float const leg = started * sin(offAway);
                    float const disc = clearRadius * clearRadius - leg * leg;
                    hop = disc > 0.0f ? sqrt(disc) - started * cos(offAway) : 0.0f;
                    if (hop <= 0.0f)
                    {
                        ++refusedBack;
                        continue;
                    }
                }

                float dx = bot->GetPositionX() + cos(angle) * hop;
                float dy = bot->GetPositionY() + sin(angle) * hop;
                float dz = bot->GetPositionZ();
                bool exact = true;
                if (!bot->GetMap()->CheckCollisionAndGetValidCoords(bot, bot->GetPositionX(),
                                                                   bot->GetPositionY(),
                                                                   bot->GetPositionZ(), dx, dy, dz))
                {
                    dx = bot->GetPositionX() + cos(angle) * hop;
                    dy = bot->GetPositionY() + sin(angle) * hop;
                    dz = bot->GetPositionZ();
                    exact = false;
                }

                Position const dest(dx, dy, dz);
                float const travel = speed > 0.0f ? hop / speed : 0.0f;

                if ((exact && !IsMimironSpotOnFloor(dest)) ||
                    dest.GetExactDist2d(bot->GetPositionX(), bot->GetPositionY()) <
                        hop * ULDUAR_MIMIRON_FLEE_MIN_PROGRESS_PCT)
                {
                    ++refusedMove;
                    continue;
                }

                if (dest.GetExactDist2d(from.GetPositionX(), from.GetPositionY()) <= started)
                {
                    ++refusedBack;
                    continue;
                }

                if (!IsMimironSpotMineSafe(bot, dest) ||
                    (mineClearWalk && !IsMimironLegMineSafe(bot->GetPosition(), mines, dest)))
                {
                    ++refusedMine;
                    continue;
                }

                if (!IsMimironSpotBarrageSafe(vx001, barrage, dest, travel))
                {
                    ++refusedCone;
                    continue;
                }

                if (!allowFire && !IsMimironSpotFireSafe(hazards, dest))
                {
                    ++refusedFire;
                    continue;
                }

                if (!IsMimironSpotBombSafe(hazards, dest))
                {
                    ++refusedBomb;
                    continue;
                }

                if (!IsMimironSpotBombBotSafe(bombBotBlasts, dest))
                {
                    ++refusedBombBot;
                    continue;
                }

                if (!IsMimironSpotRocketSafe(botAI, dest))
                {
                    ++refusedRocket;
                    continue;
                }

                if (!IsMimironSpotRapidBurstSafe(vx001, burst, dest))
                {
                    ++refusedBurst;
                    continue;
                }

                if (IsMimironSpotInSector(vx001, sector, dest))
                {
                    ++refusedBurst;
                    continue;
                }

                if (!IsMimironSpotShockSafe(botAI, dest))
                {
                    ++refusedShock;
                    continue;
                }

                if (!IsMimironSpotHolderSafe(holderRadius, dest))
                {
                    ++refusedHold;
                    continue;
                }

                if (!IsMimironSpotFireBotSafe(bot, hazards, dest,
                                              sirenScreen ? ULDUAR_MIMIRON_FIREBOT_SIREN_STAND : 0.0f,
                                              ULDUAR_MIMIRON_FIREBOT_SPRAY_STAND_HALF_WIDTH))
                {
                    ++refusedSpray;
                    continue;
                }

                if (mineClearWalk && !IsMimironPathMineSafe(bot, mines, dest))
                {
                    ++refusedMine;
                    continue;
                }

                if (TryMoveTo(bot->GetMapId(), dx, dy, dz, false, false, true, exact, priority) ==
                    MoveOutcome::Issued)
                {
                    NoteFleeOutcome(what, "ok", &offAway, refusedBack, refusedMine, refusedCone, refusedFire,
                                    refusedBomb, refusedRocket, refusedBurst, refusedShock, refusedSpray,
                                    refusedMove, refusedHold, refusedBombBot);
                    return true;
                }

                ++refusedMove;
            }
        }
    }

    if (sector.valid)
        return FleeFan(from, fallbackFrom, distance, priority, fallbackUnfiltered, interrupt, what,
                       clearRadius, allowFire, false, toward, sirenScreen);

    if (!fallbackUnfiltered)
    {
        NoteFleeOutcome(what, "none", nullptr, refusedBack, refusedMine, refusedCone, refusedFire,
                        refusedBomb, refusedRocket, refusedBurst, refusedShock, refusedSpray, refusedMove,
                        refusedHold, refusedBombBot);
        return false;
    }

    float const fallbackTravel = speed > 0.0f ? distance / speed : 0.0f;
    auto const fallbackSafe = [&](Position const& spot)
    {
        return IsMimironSpotShockSafe(botAI, spot) && IsMimironSpotBombSafe(hazards, spot) &&
               IsMimironSpotRocketSafe(botAI, spot) &&
               IsMimironSpotBarrageSafe(vx001, barrage, spot, fallbackTravel) &&
               !IsMimironSpotInFireBotSpray(hazards, spot, ULDUAR_MIMIRON_FIREBOT_SPRAY_HALF_WIDTH) &&
               IsMimironSpotHolderSafe(holderRadius, spot);
    };

    if (allowFire || fallbackFrom || clearRadius > 0.0f)
    {
        Position const straightAway(bot->GetPositionX() + cos(away) * distance,
                                    bot->GetPositionY() + sin(away) * distance, bot->GetPositionZ());
        if (!fallbackSafe(straightAway))
        {
            NoteFleeOutcome(what, "unsafe", nullptr, refusedBack, refusedMine, refusedCone, refusedFire,
                            refusedBomb, refusedRocket, refusedBurst, refusedShock, refusedSpray, refusedMove,
                            refusedHold, refusedBombBot);
            return false;
        }

        NoteFleeOutcome(what, "fallback", nullptr, refusedBack, refusedMine, refusedCone, refusedFire,
                        refusedBomb, refusedRocket, refusedBurst, refusedShock, refusedSpray, refusedMove,
                        refusedHold, refusedBombBot);

        if (fallbackFrom)
            return MoveAwayOnFloor(fallbackFrom, distance);

        return MoveTo(bot->GetMapId(), straightAway.GetPositionX(), straightAway.GetPositionY(),
                      straightAway.GetPositionZ(), false, false, true, false, priority);
    }

    struct FallbackSpot
    {
        Position dest;
        bool exact = true;
        float offAway = 0.0f;
        bool lethal = false;
        uint32 burning = 0;
        bool mined = false;
        float clearance = 0.0f;
    };

    std::vector<FallbackSpot> spots;
    for (float delta = 0.0f; delta <= ULDUAR_MIMIRON_FLEE_FAN_MAX + 0.001f; delta += ULDUAR_MIMIRON_FLEE_FAN_STEP)
    {
        for (float sign : {1.0f, -1.0f})
        {
            if (delta == 0.0f && sign < 0.0f)
                continue;

            FallbackSpot spot;
            spot.offAway = sign * delta;
            float const angle = away + spot.offAway;
            float dx = bot->GetPositionX() + cos(angle) * distance;
            float dy = bot->GetPositionY() + sin(angle) * distance;
            float dz = bot->GetPositionZ();
            if (!bot->GetMap()->CheckCollisionAndGetValidCoords(bot, bot->GetPositionX(), bot->GetPositionY(),
                                                               bot->GetPositionZ(), dx, dy, dz))
            {
                dx = bot->GetPositionX() + cos(angle) * distance;
                dy = bot->GetPositionY() + sin(angle) * distance;
                dz = bot->GetPositionZ();
                spot.exact = false;
            }

            spot.dest = Position(dx, dy, dz);
            if ((spot.exact && !IsMimironSpotOnFloor(spot.dest)) ||
                spot.dest.GetExactDist2d(bot->GetPositionX(), bot->GetPositionY()) <
                    distance * ULDUAR_MIMIRON_FLEE_MIN_PROGRESS_PCT ||
                spot.dest.GetExactDist2d(from.GetPositionX(), from.GetPositionY()) <= started ||
                !fallbackSafe(spot.dest))
                continue;

            spot.lethal = !IsMimironSpotBombBotSafe(bombBotBlasts, spot.dest) ||
                          !IsMimironSpotRapidBurstSafe(vx001, burst, spot.dest);
            spot.clearance = ULDUAR_MIMIRON_FLAMES_RADIUS;
            for (Position const& node : hazards.flames)
            {
                float const gap = spot.dest.GetExactDist2d(node.GetPositionX(), node.GetPositionY());
                spot.burning += gap < ULDUAR_MIMIRON_FLAMES_BURN_RADIUS ? 1 : 0;
                spot.clearance = std::min(spot.clearance, gap);
            }

            for (Position const& mine : mines)
                spot.mined = spot.mined || spot.dest.GetExactDist2d(mine.GetPositionX(), mine.GetPositionY()) <
                                               ULDUAR_MIMIRON_MINE_CLEARANCE;

            spots.push_back(spot);
        }
    }

    if (spots.empty())
    {
        NoteFleeOutcome(what, "unsafe", nullptr, refusedBack, refusedMine, refusedCone, refusedFire,
                        refusedBomb, refusedRocket, refusedBurst, refusedShock, refusedSpray, refusedMove,
                        refusedHold, refusedBombBot);
        return false;
    }

    std::stable_sort(spots.begin(), spots.end(),
                     [](FallbackSpot const& a, FallbackSpot const& b)
                     {
                         if (a.lethal != b.lethal)
                             return !a.lethal;
                         if (a.burning != b.burning)
                             return a.burning < b.burning;
                         if (a.mined != b.mined)
                             return !a.mined;
                         return a.clearance > b.clearance;
                     });

    for (FallbackSpot const& spot : spots)
    {
        if (TryMoveTo(bot->GetMapId(), spot.dest.GetPositionX(), spot.dest.GetPositionY(), spot.dest.GetPositionZ(),
                      false, false, true, spot.exact, priority) != MoveOutcome::Issued)
            continue;

        NoteFleeOutcome(what, "fallback", &spot.offAway, refusedBack, refusedMine, refusedCone, refusedFire,
                        refusedBomb, refusedRocket, refusedBurst, refusedShock, refusedSpray, refusedMove,
                        refusedHold, refusedBombBot);
        return true;
    }

    NoteFleeOutcome(what, "fallback", nullptr, refusedBack, refusedMine, refusedCone, refusedFire,
                    refusedBomb, refusedRocket, refusedBurst, refusedShock, refusedSpray, refusedMove,
                    refusedHold, refusedBombBot);
    return false;
}

bool MimironFleeAction::MoveAwayOnFloor(Unit* from, float distance)
{
    float const away = from->GetAngle(bot);
    float const step = static_cast<float>(M_PI) / 8.0f;
    for (float delta = 0.0f; delta <= static_cast<float>(M_PI) / 2.0f + 0.001f; delta += step)
    {
        for (float sign : {1.0f, -1.0f})
        {
            if (delta == 0.0f && sign < 0.0f)
                continue;

            float const angle = away + sign * delta;
            float dx = bot->GetPositionX() + cos(angle) * distance;
            float dy = bot->GetPositionY() + sin(angle) * distance;
            float dz = bot->GetPositionZ();
            bool exact = true;
            if (!bot->GetMap()->CheckCollisionAndGetValidCoords(bot, bot->GetPositionX(), bot->GetPositionY(),
                                                               bot->GetPositionZ(), dx, dy, dz))
            {
                dx = bot->GetPositionX() + cos(angle) * distance;
                dy = bot->GetPositionY() + sin(angle) * distance;
                dz = bot->GetPositionZ();
                exact = false;
            }
            else if (!IsMimironSpotOnFloor(Position(dx, dy, dz)))
                continue;

            if (MoveTo(from->GetMapId(), dx, dy, dz, false, false, true, exact, MovementPriority::MOVEMENT_COMBAT))
                return true;
        }
    }

    return false;
}

void MimironFleeAction::NoteFleeOutcome(char const* what, char const* outcome, float const* taken,
                                        uint32 refusedBack, uint32 refusedMine, uint32 refusedCone,
                                        uint32 refusedFire, uint32 refusedBomb, uint32 refusedRocket,
                                        uint32 refusedBurst, uint32 refusedShock, uint32 refusedSpray,
                                        uint32 refusedMove, uint32 refusedHold, uint32 refusedBombBot)
{
    if (!RaidObs::Active())
        return;

    char bearing[16] = "";
    if (taken)
        snprintf(bearing, sizeof(bearing), " %+.0f", *taken * 180.0f / static_cast<float>(M_PI));

    char line[224];
    snprintf(line, sizeof(line),
             "%s %s%s (back%u mine%u cone%u fire%u bomb%u rocket%u burst%u shock%u spray%u move%u hold%u"
             " bombbot%u)",
             what, outcome, bearing, refusedBack, refusedMine, refusedCone, refusedFire, refusedBomb,
             refusedRocket, refusedBurst, refusedShock, refusedSpray, refusedMove, refusedHold, refusedBombBot);

    RaidObs::NoteDerived(bot, "mimiron.flee", line);
}

bool MimironShockBlastAction::FleeShockToAnchor()
{
    Position anchor;
    if (!GetMimironSpreadSlot(botAI, bot, anchor) || !IsMimironSpotShockSafe(botAI, anchor))
        anchor = ULDUAR_MIMIRON_ROOM_CENTER;

    return MoveTowardClearOfMines(anchor, MovementPriority::MOVEMENT_FORCED, false, true,
                                  "shock anchor");
}

bool MimironShockBlastAction::Execute(Event /*event*/)
{
    Unit* leviathanMkII = GetUldUnitByEntry(botAI, NPC_LEVIATHAN_MKII);
    if (!leviathanMkII)
        return false;

    float const dist = bot->GetExactDist2d(leviathanMkII);
    if (dist < ULDUAR_MIMIRON_SHOCK_BLAST_SAFE_DIST)
    {
        float const escape = ULDUAR_MIMIRON_SHOCK_BLAST_SAFE_DIST + ULDUAR_MIMIRON_SHOCK_FLEE_MARGIN;
        float const gap = escape - dist;
        if (!MoveAwayClearOfMines(leviathanMkII, gap, MovementPriority::MOVEMENT_FORCED, false, true,
                                  "shock", escape) &&
            !MoveAwayClearOfMines(leviathanMkII, gap, MovementPriority::MOVEMENT_FORCED, true, true,
                                  "shock fire", escape, false, true))
            FleeShockToAnchor();

        if (PlayerbotAI::IsMelee(bot))
            botAI->SetNextCheckDelay(100);

        return true;
    }

    return PlayerbotAI::IsMelee(bot);
}

bool MimironShockBlastAction::isUseful()
{
    MimironShockBlastTrigger mimironShockBlastTrigger(botAI);
    return mimironShockBlastTrigger.IsActive();
}

bool MimironUnreachableStepAction::Execute(Event /*event*/)
{
    Unit* mech = GetMimironUnreachableMech(botAI, bot);
    if (!mech)
        return false;

    float const centreX = ULDUAR_MIMIRON_ROOM_CENTER.GetPositionX();
    float const centreY = ULDUAR_MIMIRON_ROOM_CENTER.GetPositionY();
    float const reach = mech->GetCombatReach() + bot->GetCombatReach() + ULDUAR_MIMIRON_UNREACH_STEP_MARGIN;
    float const bearing =
        mech->GetExactDist2d(centreX, centreY) > reach ? mech->GetAngle(centreX, centreY) : mech->GetAngle(bot);

    Position const dest(mech->GetPositionX() + reach * std::cos(bearing),
                        mech->GetPositionY() + reach * std::sin(bearing), bot->GetPositionZ());
    if (bot->GetExactDist2d(dest.GetPositionX(), dest.GetPositionY()) <= ULDUAR_MIMIRON_UNREACH_ARRIVED)
        return false;

    return MoveTowardClearOfMines(dest, MovementPriority::MOVEMENT_FORCED, false, false, "unreach") ||
           MoveTowardClearOfMines(dest, MovementPriority::MOVEMENT_FORCED, false, false, "unreach fire", true);
}

bool MimironUnreachableStepAction::isUseful()
{
    MimironUnreachableTrigger mimironUnreachableTrigger(botAI);
    return mimironUnreachableTrigger.IsActive();
}

bool MimironResetEncounterStateAction::Execute(Event /*event*/)
{
    RESET_AI_VALUE(float, "disperse distance");

    return false;
}

bool MimironPhase1PositioningAction::Execute(Event /*event*/)
{
    SET_AI_VALUE(float, "disperse distance", GetMimironPhase1DisperseDistance(botAI));

    return false;
}

bool MimironPhase1PositioningAction::isUseful()
{
    MimironPhase1PositioningTrigger mimironPhase1PositioningTrigger(botAI);
    return mimironPhase1PositioningTrigger.IsActive();
}

void MimironP3Wx2LaserBarrageAction::NoteBarrageDecision(char const* branch, char const* direction,
                                                        float cw)
{
    if (!RaidObs::Active())
        return;

    int const bucket = static_cast<int>(cw * 180.0f / static_cast<float>(M_PI) / 15.0f) * 15;

    char line[64];
    snprintf(line, sizeof(line), "%s%s%s %d", branch, direction ? " " : "",
             direction ? direction : "", bucket);

    RaidObs::NoteDerived(bot, "mimiron.barrage", line);
}

bool MimironP3Wx2LaserBarrageAction::isUseful()
{
    MimironP3Wx2LaserBarrageTrigger mimironP3Wx2LaserBarrageTrigger(botAI);
    return mimironP3Wx2LaserBarrageTrigger.IsActive();
}

bool MimironP3Wx2LaserBarrageAction::Execute(Event /*event*/)
{
    Unit* boss = GetUldUnitByEntry(botAI, NPC_VX001);
    if (!boss || !boss->IsAlive())
        return false;

    MimironBarrageWindow const window = GetMimironBarrageWindow(bot, boss);
    if (!window.valid)
        return false;

    float const clearance = ULDUAR_MIMIRON_BARRAGE_HALF_ANGLE + ULDUAR_MIMIRON_BARRAGE_MARGIN;
    float const twoPi = 2.0f * static_cast<float>(M_PI);

    float const cw = Position::NormalizeOrientation(window.lead - boss->GetAngle(bot));

    bool const melee = PlayerbotAI::IsMelee(bot);
    float const minRing =
        boss->GetCombatReach() +
        (melee ? ULDUAR_MIMIRON_BARRAGE_MELEE_RING_MARGIN : ULDUAR_MIMIRON_BARRAGE_RING_MARGIN);
    float const radius =
        melee ? minRing
              : std::clamp(bot->GetExactDist2d(boss), minRing, ULDUAR_MIMIRON_SPREAD_RADIUS_MAX);

    float const shiftMin = melee ? ULDUAR_MIMIRON_BARRAGE_MELEE_RING_MIN : minRing;
    float const shiftMax = melee ? radius : ULDUAR_MIMIRON_SPREAD_RADIUS_MAX;

    float const sectorWidth = std::max(twoPi - 2.0f * clearance - window.sweep, 0.0f);

    MimironFirefighterHazards const hazards = GetMimironFirefighterHazards(botAI);
    auto const onOrbit = [&](float heading, float r)
    {
        return Position(boss->GetPositionX() + r * cos(heading), boss->GetPositionY() + r * sin(heading),
                        boss->GetPositionZ());
    };

    auto const isClean = [&](Position const& spot)
    {
        return IsMimironSpotFireSafe(hazards, spot) && IsMimironSpotBombSafe(hazards, spot) &&
               IsMimironSpotRocketSafe(botAI, spot) &&
               !IsMimironSpotInFireBotSpray(hazards, spot, ULDUAR_MIMIRON_FIREBOT_SPRAY_STAND_HALF_WIDTH);
    };

    auto const cleanOffset = [&](float bearing, float direction, float span)
    {
        for (float offset = 0.0f; offset <= span; offset += ULDUAR_MIMIRON_BARRAGE_REFUGE_PROBE)
        {
            for (float shift : ULDUAR_MIMIRON_BARRAGE_RADIUS_SHIFTS)
            {
                float const candidate = radius + shift;
                if (candidate < shiftMin || candidate > shiftMax)
                    continue;

                if (isClean(onOrbit(bearing + direction * offset, candidate)))
                    return offset;
            }
        }

        return -1.0f;
    };

    struct RingStep
    {
        float heading = 0.0f;
        float radius = 0.0f;
        int score = -1;
    };

    auto const stepRing = [&](float remaining, char const* branch)
    {
        float const bearing = boss->GetAngle(bot);
        float const fullArc = std::min(std::fabs(remaining), ULDUAR_MIMIRON_BARRAGE_STEP);

        std::vector<Position> const mines = GetMimironMines(botAI);
        int const bestScore = 3;
        int const mineClearScore = 2;

        UpdateMovementState();
        bool const canIssue = !IsWaitingForLastMove(MovementPriority::MOVEMENT_FORCED);
        auto const pickOnArc = [&](float arc, float maxShift)
        {
            RingStep pick;
            pick.heading = Position::NormalizeOrientation(bearing + std::copysign(arc, remaining));
            pick.radius = radius;
            for (float shift : ULDUAR_MIMIRON_BARRAGE_RADIUS_SHIFTS)
            {
                float const candidate = radius + shift;
                if (candidate < shiftMin || candidate > shiftMax || std::fabs(shift) > maxShift)
                    continue;

                Position const spot = onOrbit(pick.heading, candidate);
                if (!isClean(spot))
                    continue;

                int score = IsMimironWalkFireSafe(bot, hazards, spot) ? 1 : 0;
                if (IsMimironWalkMineSafe(bot, mines, spot))
                    score += 2;

                if (score >= 2 && score > pick.score && canIssue && !IsMimironPathMineSafe(bot, mines, spot))
                    score -= 2;

                if (score > pick.score)
                {
                    pick.radius = candidate;
                    pick.score = score;
                }

                if (score == bestScore)
                    break;
            }

            return pick;
        };

        RingStep step = pickOnArc(fullArc, std::numeric_limits<float>::max());
        if (canIssue && step.score < mineClearScore && !mines.empty())
        {
            for (float arc : ULDUAR_MIMIRON_BARRAGE_SHORT_STEPS)
            {
                if (arc >= fullArc)
                    continue;

                RingStep const shorter = pickOnArc(arc, 2.0f * radius * std::sin(0.5f * arc));
                if (shorter.score >= mineClearScore)
                {
                    step = shorter;
                    break;
                }
            }
        }

        NoteBarrageDecision(branch, remaining < 0.0f ? "cw" : (remaining > 0.0f ? "ccw" : nullptr), cw);

        bot->CastStop();

        Position const stepSpot = onOrbit(step.heading, step.radius);
        MoveTo(boss->GetMapId(), stepSpot.GetPositionX(), stepSpot.GetPositionY(), stepSpot.GetPositionZ(),
               false, false, false, true, MovementPriority::MOVEMENT_FORCED, true);

        botAI->SetNextCheckDelay(100);

        return true;
    };

    if (cw > window.sweep + clearance && cw < twoPi - clearance)
    {
        if (bot->GetExactDist2d(boss) < ULDUAR_MIMIRON_BARRAGE_BOUNDARY)
        {
            float const bearing = boss->GetAngle(bot);
            NoteBarrageDecision("boundary", nullptr, cw);
            bot->CastStop();
            MoveTo(boss->GetMapId(), boss->GetPositionX() + radius * cos(bearing),
                   boss->GetPositionY() + radius * sin(bearing), boss->GetPositionZ(), false, false, false,
                   true, MovementPriority::MOVEMENT_FORCED, true);
            return true;
        }

        if (IsMimironSpotBurning(hazards, bot->GetPosition()))
        {
            float const bearing = boss->GetAngle(bot);
            float const cwOffset = cleanOffset(bearing, -1.0f, twoPi - clearance - cw);
            float const ccwOffset = cleanOffset(bearing, 1.0f, cw - window.sweep - clearance);
            if (cwOffset >= 0.0f && (ccwOffset < 0.0f || cwOffset <= ccwOffset))
                return stepRing(-cwOffset, "refuge");

            if (ccwOffset >= 0.0f)
                return stepRing(ccwOffset, "refuge");
        }

        if (melee && bot->GetExactDist2d(boss) > radius + 1.0f)
        {
            float const bearing = boss->GetAngle(bot);
            Position const inward(boss->GetPositionX() + radius * cos(bearing),
                                  boss->GetPositionY() + radius * sin(bearing),
                                  boss->GetPositionZ());

            if (IsMimironSpotMineSafe(bot, inward) && isClean(inward))
            {
                NoteBarrageDecision("stepin", nullptr, cw);
                bot->CastStop();
                MoveTo(boss->GetMapId(), inward.GetPositionX(), inward.GetPositionY(),
                       inward.GetPositionZ(), false, false, false, true,
                       MovementPriority::MOVEMENT_FORCED, true);
                return true;
            }
        }

        NoteBarrageDecision("clear", nullptr, cw);
        return false;
    }

    float const turnRate = radius > 0.0f ? bot->GetSpeed(MOVE_RUN) / radius : 0.0f;
    float const half = ULDUAR_MIMIRON_BARRAGE_HALF_ANGLE;

    float const sweepRate = window.untilLive > 0.0f ? 0.0f : window.rate;

    char const* branch = "trailing";
    bool goClockwise = false;
    if (cw <= window.sweep + clearance)
    {
        if (cw > half)
        {
            branch = "ahead";
            goClockwise = true;
        }
        else if (window.untilLive > 0.0f && turnRate > 0.0f &&
                 (cw + clearance) / turnRate <= window.untilLive - ULDUAR_MIMIRON_BARRAGE_IGNITION_BUFFER)
        {
            branch = "inside";
        }
        else
        {
            branch = "inside";
            float const outCcw = (cw + half) / (turnRate + sweepRate);
            float const outCw = turnRate > sweepRate
                                    ? (half - cw) / (turnRate - sweepRate)
                                    : std::numeric_limits<float>::max();
            goClockwise = outCw < outCcw;
        }
    }

    float const ccwTravel = Position::NormalizeOrientation(cw + clearance);
    float const cwTravel = Position::NormalizeOrientation(window.sweep + clearance - cw);
    float remaining = goClockwise ? -cwTravel : ccwTravel;

    if (!hazards.flames.empty() || !hazards.sprayLanes.empty())
    {
        float const direction = goClockwise ? -1.0f : 1.0f;
        float const beyond = cleanOffset(boss->GetAngle(bot) + remaining, direction, sectorWidth);
        if (beyond > 0.0f)
            remaining += direction * beyond;
    }

    if (std::fabs(remaining) * radius < 1.0f)
    {
        NoteBarrageDecision("hold", nullptr, cw);
        return false;
    }

    return stepRing(remaining, branch);
}

bool MimironArcSpreadAction::isUseful()
{
    MimironArcSpreadTrigger mimironArcSpreadTrigger(botAI);
    return mimironArcSpreadTrigger.IsActive();
}

bool MimironArcSpreadAction::Execute(Event /*event*/)
{
    Position slot;
    if (!GetMimironSpreadSlot(botAI, bot, slot))
        return false;

    if (IsMimironTankAnchorSlot(botAI, bot))
        return MoveTo(bot->GetMapId(), slot.GetPositionX(), slot.GetPositionY(), slot.GetPositionZ(),
                      false, false, false, true, MovementPriority::MOVEMENT_COMBAT, true);

    std::vector<Position> const mines = GetMimironMines(botAI);
    UpdateMovementState();
    bool const canIssue = !IsWaitingForLastMove(MovementPriority::MOVEMENT_COMBAT);
    for (MimironApproach const& approach :
         GetMimironSlotApproaches(botAI, bot, slot, GetMimironFirefighterHazards(botAI)))
    {
        if (!approach.overMines && canIssue &&
            !IsDuplicateMove(approach.dest.GetPositionX(), approach.dest.GetPositionY(),
                             approach.dest.GetPositionZ()) &&
            !IsMimironPathMineSafe(bot, mines, approach.dest))
            continue;

        MoveOutcome const outcome =
            TryMoveTo(bot->GetMapId(), approach.dest.GetPositionX(), approach.dest.GetPositionY(),
                      approach.dest.GetPositionZ(), false, false, false, true,
                      MovementPriority::MOVEMENT_COMBAT, true);
        if (outcome == MoveOutcome::NoPath)
            continue;

        if (outcome != MoveOutcome::Issued)
            return false;

        if (std::strcmp(approach.how, "direct") != 0 && RaidObs::Active())
        {
            char line[32];
            if (std::strcmp(approach.how, "detour") == 0)
                snprintf(line, sizeof(line), "detour %+.0f", approach.turn * 180.0f / static_cast<float>(M_PI));
            else if (std::strcmp(approach.how, "cross") == 0 || std::strcmp(approach.how, "stranded") == 0)
                snprintf(line, sizeof(line), "%s %.1f", approach.how, approach.burn);
            else
                snprintf(line, sizeof(line), "substitute r%.0f",
                         approach.dest.GetExactDist2d(slot.GetPositionX(), slot.GetPositionY()));
            RaidObs::NoteDerived(bot, "mimiron.approach", line);
        }

        return true;
    }

    return false;
}

bool MimironAerialCommandUnitAction::Execute(Event /*event*/)
{
    if (PlayerbotAI::IsMainTank(bot) || PlayerbotAI::IsAssistTankOfIndex(bot, 0))
    {
        Unit* assaultBot = GetUldUnitByEntry(botAI, NPC_ASSAULT_BOT);
        Unit* boss = GetUldUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT);

        Unit* focus = assaultBot ? assaultBot : boss;
        if (focus && IsMechanicTrackerBot(bot, ULDUAR_MAP_ID))
            MarkTargetWithSkull(bot, focus);
    }

    return false;
}

bool MimironRocketStrikeAction::isUseful()
{
    MimironRocketStrikeTrigger mimironRocketStrikeTrigger(botAI);
    return mimironRocketStrikeTrigger.IsActive();
}

bool MimironRocketStrikeAction::Execute(Event /*event*/)
{
    Creature* rocketStrikeN = bot->FindNearestCreature(NPC_ROCKET_STRIKE_N, ULDUAR_MIMIRON_ROOM_SEARCH_RANGE);
    if (!rocketStrikeN)
        return false;

    float const escape = ULDUAR_MIMIRON_ROCKET_CLEARANCE + ULDUAR_MIMIRON_ROCKET_FLEE_MARGIN;
    float const gap = escape - bot->GetExactDist2d(rocketStrikeN);
    return MoveAwayClearOfMines(rocketStrikeN, gap, MovementPriority::MOVEMENT_FORCED, true, true,
                                "rocket", escape);
}

bool MimironPhase4FocusAction::Execute(Event /*event*/)
{
    if (!IsMimironPhase4(bot))
        return false;

    if (PlayerbotAI::IsMainTank(bot))
    {
        Unit* const focus = GetMimironPhase4TankFocus(botAI, bot);
        if (focus && AI_VALUE(Unit*, "current target") != focus)
            return Attack(focus);

        return false;
    }

    if (AI_VALUE(float, "disperse distance") != ULDUAR_MIMIRON_PHASE4_DISPERSE_DISTANCE)
        SET_AI_VALUE(float, "disperse distance", ULDUAR_MIMIRON_PHASE4_DISPERSE_DISTANCE);

    Unit* const acuFocus = GetMimironPhase4AcuTankFocus(botAI, bot);
    if (acuFocus && AI_VALUE(Unit*, "current target") != acuFocus)
        return Attack(acuFocus);

    return false;
}

bool MimironProximityMineAction::isUseful()
{
    MimironProximityMineTrigger mimironProximityMineTrigger(botAI);
    return mimironProximityMineTrigger.IsActive();
}

bool MimironProximityMineAction::Execute(Event /*event*/)
{
    Position nearest;
    if (!GetMimironCloseMine(botAI, bot, nearest))
        return false;

    float const nearestDist = bot->GetExactDist2d(nearest.GetPositionX(), nearest.GetPositionY());

    float const step = std::min(ULDUAR_MIMIRON_MINE_CLEARANCE + 1.0f - nearestDist,
                                ULDUAR_MIMIRON_MINE_MAX_STEP);
    return MoveAwayClearOfMines(nearest, std::max(step, ULDUAR_MIMIRON_MINE_MIN_STEP),
                                MovementPriority::MOVEMENT_COMBAT, false, false, "mine");
}

bool MimironBombBotAction::isUseful()
{
    MimironBombBotTrigger mimironBombBotTrigger(botAI);
    return mimironBombBotTrigger.IsActive();
}

bool MimironBombBotAction::Execute(Event /*event*/)
{
    MimironBombBotBlast const blast =
        GetMimironBombBotBlast(botAI, bot, ULDUAR_MIMIRON_BOMB_BOT_BLAST_CLEARANCE);
    if (!blast.valid)
        return false;

    if (blast.chased)
        return MoveAwayClearOfMines(blast.bomb, ULDUAR_MIMIRON_BOMB_BOT_KITE_DISTANCE,
                                    MovementPriority::MOVEMENT_COMBAT, false, false, "bombbot kite");

    float const escape = ULDUAR_MIMIRON_BOMB_BOT_BLAST_CLEARANCE + ULDUAR_MIMIRON_BOMB_BOT_STEP_MARGIN;
    return MoveAwayClearOfMines(GetMimironBombBotBlastNearest(blast, bot->GetPosition()), escape,
                                MovementPriority::MOVEMENT_COMBAT, true, false, "bombbot", escape);
}

bool MimironBombBotTauntAction::Execute(Event /*event*/)
{
    Unit* bombBot = GetMimironBombBotToTaunt(botAI, bot);
    if (!bombBot || !CastClassTaunt(botAI, bombBot))
        return false;

    AttackDirect(bombBot);
    return true;
}

bool MimironBombTankHoldAction::Execute(Event /*event*/)
{
    context->GetValue<Unit*>("current target")->Set(nullptr);
    bot->AttackStop();
    return true;
}

bool MimironBombTankFormAction::Execute(Event /*event*/)
{
    for (char const* form : {"dire bear form", "bear form"})
    {
        if (botAI->CanCastSpell(form, bot) && botAI->CastSpell(form, bot))
            return true;
    }

    return false;
}

bool MimironPetControlAction::isUseful()
{
    MimironPetControlTrigger mimironPetControlTrigger(botAI);
    return mimironPetControlTrigger.IsActive();
}

bool MimironPetControlAction::Execute(Event /*event*/)
{
    if (IsMimironPhase4(bot))
    {
        if (Unit* focus = GetMimironPhase4Focus(botAI, bot, true))
            CommandPetAttack(botAI, focus);
        else
            StopPet(botAI);

        return false;
    }

    if (IsMimironAcuGrounded(botAI))
    {
        if (Unit* aerialCommandUnit = GetUldUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT))
        {
            CommandPetAttack(botAI, aerialCommandUnit);
            return false;
        }
    }

    for (uint32 entry : {NPC_ASSAULT_BOT, NPC_JUNK_BOT, NPC_BOMB_BOT})
    {
        Unit* nearest = nullptr;
        float nearestDist = 0.0f;

        for (ObjectGuid const& guid : AI_VALUE(GuidVector, "possible targets no los"))
        {
            Unit* unit = botAI->GetUnit(guid);
            if (!unit || !unit->IsAlive() || unit->GetEntry() != entry)
                continue;

            float const dist = bot->GetExactDist2d(unit);
            if (!nearest || dist < nearestDist)
            {
                nearest = unit;
                nearestDist = dist;
            }
        }

        if (nearest)
        {
            CommandPetAttack(botAI, nearest);
            return false;
        }
    }

    StopPet(botAI);

    return false;
}

bool MimironApproachTargetAction::Execute(Event /*event*/)
{
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || !target->IsAlive())
        return false;

    Position approach;
    if (!GetMimironTargetApproach(botAI, bot, target, GetMimironApproachRange(botAI, bot), approach))
        return false;

    if (TryMoveTo(bot->GetMapId(), approach.GetPositionX(), approach.GetPositionY(),
                  approach.GetPositionZ(), false, false, false, true,
                  MovementPriority::MOVEMENT_COMBAT, true) != MoveOutcome::Issued)
        return false;

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "mimiron.close", "round");

    return true;
}

bool MimironDodgeFlamesAction::isUseful()
{
    MimironDodgeFlamesTrigger mimironDodgeFlamesTrigger(botAI);
    return mimironDodgeFlamesTrigger.IsActive();
}

bool MimironDodgeFlamesAction::Execute(Event /*event*/)
{
    MimironFirefighterHazards const hazards = GetMimironFirefighterHazards(botAI);

    std::vector<Position> cluster;
    for (Position const& node : hazards.flames)
        if (bot->GetExactDist2d(node.GetPositionX(), node.GetPositionY()) < ULDUAR_MIMIRON_FLAMES_BURN_RADIUS)
            cluster.push_back(node);

    if (cluster.empty())
        return false;

    float cx = 0.0f, cy = 0.0f;
    for (Position const& node : cluster)
    {
        cx += node.GetPositionX();
        cy += node.GetPositionY();
    }
    cx /= cluster.size();
    cy /= cluster.size();

    Position const centre(cx, cy, bot->GetPositionZ());
    float spread = 0.0f;
    for (Position const& node : cluster)
        spread = std::max(spread, centre.GetExactDist2d(node.GetPositionX(), node.GetPositionY()));

    std::optional<float> toward;
    Position slot;
    if (GetMimironSpreadSlot(botAI, bot, slot) &&
        bot->GetExactDist2d(slot.GetPositionX(), slot.GetPositionY()) > ULDUAR_MIMIRON_SPREAD_TOLERANCE)
        toward = bot->GetAngle(slot.GetPositionX(), slot.GetPositionY());
    else if (Unit* target = AI_VALUE(Unit*, "current target"))
        toward = bot->GetAngle(target);

    for (size_t rung = 0; rung < std::size(ULDUAR_MIMIRON_FLAMES_STEP_LADDER); ++rung)
    {
        float const step = ULDUAR_MIMIRON_FLAMES_STEP_LADDER[rung];
        bool const last = rung + 1 == std::size(ULDUAR_MIMIRON_FLAMES_STEP_LADDER);

        char what[16];
        snprintf(what, sizeof(what), "flames+%.0f", step);

        float const hop = std::min(ULDUAR_MIMIRON_FLAMES_RADIUS + spread + step,
                                   ULDUAR_MIMIRON_FLAMES_MAX_HOP);

        if (MoveAwayClearOfMines(centre, hop, MovementPriority::MOVEMENT_FORCED, last, true, what, 0.0f,
                                 toward))
        {
            NoteMimironFireDodge(bot);
            return true;
        }
    }

    return false;
}

bool MimironFireBotAction::isUseful()
{
    MimironFireBotTrigger mimironFireBotTrigger(botAI);
    return mimironFireBotTrigger.IsActive();
}

bool MimironFireBotAction::Execute(Event /*event*/)
{
    MimironFirefighterHazards const hazards = GetMimironFirefighterHazards(botAI);

    for (Position const& lane : hazards.sprayLanes)
    {
        float const dx = bot->GetPositionX() - lane.GetPositionX();
        float const dy = bot->GetPositionY() - lane.GetPositionY();
        float const facing = lane.GetOrientation();
        float const ahead = dx * std::cos(facing) + dy * std::sin(facing);
        float const side = dy * std::cos(facing) - dx * std::sin(facing);
        if (ahead < 0.0f || ahead >= ULDUAR_MIMIRON_FIREBOT_SPRAY_LENGTH ||
            std::fabs(side) >= ULDUAR_MIMIRON_FIREBOT_SPRAY_HALF_WIDTH)
            continue;

        float const out = side >= 0.0f ? 1.0f : -1.0f;
        for (float const across : {1.0f, -1.0f})
        {
            float const step = ULDUAR_MIMIRON_FIREBOT_SPRAY_HALF_WIDTH + ULDUAR_MIMIRON_FIREBOT_SPRAY_SIDESTEP -
                               across * std::fabs(side);
            float const dir = across * out;
            Position const dest(bot->GetPositionX() - dir * step * std::sin(facing),
                                bot->GetPositionY() + dir * step * std::cos(facing), bot->GetPositionZ());

            if (MoveTowardClearOfMines(dest, MovementPriority::MOVEMENT_FORCED, across < 0.0f, true, "spray",
                                       true, false))
            {
                NoteMimironFireDodge(bot);
                return true;
            }
        }

        return false;
    }

    Position const* nearest = nullptr;
    float nearestDist = ULDUAR_MIMIRON_FIREBOT_SIREN_CLEARANCE;
    for (Position const& fireBot : hazards.fireBots)
    {
        float const dist = bot->GetExactDist2d(fireBot.GetPositionX(), fireBot.GetPositionY());
        if (dist < nearestDist)
        {
            nearest = &fireBot;
            nearestDist = dist;
        }
    }

    if (!nearest ||
        IsMimironSpotFireBotSafe(bot, hazards, bot->GetPosition(), ULDUAR_MIMIRON_FIREBOT_SIREN_CLEARANCE,
                                 ULDUAR_MIMIRON_FIREBOT_SPRAY_HALF_WIDTH))
        return false;

    return MoveAwayClearOfMines(*nearest, ULDUAR_MIMIRON_FIREBOT_SIREN_FLEE - nearestDist,
                                MovementPriority::MOVEMENT_FORCED, true, false, "siren",
                                ULDUAR_MIMIRON_FIREBOT_SIREN_FLEE);
}

bool MimironFrostBombAction::isUseful()
{
    MimironFrostBombTrigger mimironFrostBombTrigger(botAI);
    return mimironFrostBombTrigger.IsActive();
}

bool MimironFrostBombAction::Execute(Event /*event*/)
{
    Creature* frostBomb = bot->FindNearestCreature(NPC_FROST_BOMB, ULDUAR_MIMIRON_FROST_BOMB_RADIUS);
    if (!frostBomb)
        return false;

    float const gap = ULDUAR_MIMIRON_FROST_BOMB_CLEARANCE - bot->GetExactDist2d(frostBomb);
    return MoveAwayClearOfMines(frostBomb, gap, MovementPriority::MOVEMENT_FORCED, true, true,
                                "frostbomb", ULDUAR_MIMIRON_FROST_BOMB_CLEARANCE, true);
}

std::vector<std::pair<uint32, Unit*>> MimironSetDpsPriorityAction::BuildPriorityList()
{
    Unit* leviathanMkII = nullptr;
    Unit* vx001 = nullptr;
    Unit* aerialCommandUnit = nullptr;
    std::vector<Unit*> bombBots;
    std::vector<Unit*> assaultBots;
    std::vector<Unit*> junkBots;
    std::vector<Unit*> fireBots;

    for (ObjectGuid const& guid :
         botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets no los")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        switch (unit->GetEntry())
        {
            case NPC_LEVIATHAN_MKII:      leviathanMkII = unit; break;
            case NPC_VX001:               vx001 = unit; break;
            case NPC_AERIAL_COMMAND_UNIT: aerialCommandUnit = unit; break;
            case NPC_BOMB_BOT:            bombBots.push_back(unit); break;
            case NPC_ASSAULT_BOT:         assaultBots.push_back(unit); break;
            case NPC_JUNK_BOT:            junkBots.push_back(unit); break;
            case NPC_EMERGENCY_FIRE_BOT:  fireBots.push_back(unit); break;
            default: break;
        }
    }

    Unit* currentTarget = AI_VALUE(Unit*, "current target");

    std::vector<std::pair<uint32, Unit*>> priority;
    if (PlayerbotAI::IsRangedDps(bot))
    {
        std::vector<Unit*> reachable;
        for (Unit* bombBot : bombBots)
            if (bot->GetExactDist2d(bombBot) <= sPlayerbotAIConfig.SpellDistance)
                reachable.push_back(bombBot);

        priority.emplace_back(NPC_BOMB_BOT, SelectByEntry(currentTarget, NPC_BOMB_BOT, reachable));
    }

    if (aerialCommandUnit && (PlayerbotAI::IsMelee(bot) || PlayerbotAI::IsRangedDps(bot)) &&
        IsMimironAcuGrounded(botAI))
        priority.emplace_back(NPC_AERIAL_COMMAND_UNIT, aerialCommandUnit);

    if (PlayerbotAI::IsMelee(bot) && (junkBots.size() >= ULDUAR_MIMIRON_JUNK_BOT_PILE ||
                                (currentTarget && currentTarget->GetEntry() == NPC_JUNK_BOT)))
        priority.emplace_back(NPC_JUNK_BOT, SelectByEntry(currentTarget, NPC_JUNK_BOT, junkBots));

    priority.emplace_back(NPC_ASSAULT_BOT, SelectByEntry(currentTarget, NPC_ASSAULT_BOT, assaultBots));

    priority.emplace_back(NPC_JUNK_BOT, SelectByEntry(currentTarget, NPC_JUNK_BOT, junkBots));

    if (!PlayerbotAI::IsHeal(bot) && IsMimironFireBotCullActive(botAI, bot))
        priority.emplace_back(NPC_EMERGENCY_FIRE_BOT,
                              SelectByEntry(currentTarget, NPC_EMERGENCY_FIRE_BOT, fireBots));

    if (IsMimironPhase4(bot))
    {
        if (Unit* focus = GetMimironPhase4Focus(botAI, bot, PlayerbotAI::IsMelee(bot)))
            priority.emplace_back(focus->GetEntry(), focus);

        return priority;
    }

    for (Unit* mech : {leviathanMkII, vx001, aerialCommandUnit})
        if (mech)
            priority.emplace_back(mech->GetEntry(), mech);

    return priority;
}

Unit* MimironSetDpsPriorityAction::SelectByEntry(Unit* currentTarget, uint32 entry,
                                                std::vector<Unit*> const& candidates) const
{
    Unit* selected = nullptr;
    if (currentTarget && currentTarget->IsAlive() && currentTarget->GetEntry() == entry)
        selected = currentTarget;

    constexpr float switchMargin = 10.0f;
    for (Unit* candidate : candidates)
    {
        if (!candidate || candidate == selected)
            continue;

        if (!selected)
        {
            selected = candidate;
            continue;
        }

        if (candidate->GetExactDist2d(bot) + switchMargin < selected->GetExactDist2d(bot))
            selected = candidate;
    }

    return selected;
}

bool MimironSetDpsPriorityAction::IsAllowedTarget(Unit* unit) const
{
    if (!unit || !unit->IsAlive())
        return false;

    switch (unit->GetEntry())
    {
        case NPC_BOMB_BOT:
            if (PlayerbotAI::IsMelee(bot))
                return false;

            return unit->GetExactDist2d(bot) >= ULDUAR_MIMIRON_BOMB_BOT_RADIUS ||
                   GetMimironBombBotChasing(botAI, bot) == unit;

        case NPC_AERIAL_COMMAND_UNIT:
        {
            if (!PlayerbotAI::IsMelee(bot))
                return true;

            if (!IsMimironPhase4(bot))
                return IsMimironAcuGrounded(botAI);

            return GetUldUnitByEntry(botAI, NPC_LEVIATHAN_MKII) == nullptr ||
                   GetUldUnitByEntry(botAI, NPC_VX001) == nullptr ||
                   GetMimironPhase4Focus(botAI, bot, true) == unit;
        }

        case NPC_EMERGENCY_FIRE_BOT:
            return !IsMimironFireBotProtected(botAI, bot, unit);

        default:
            return true;
    }
}

char const* MimironSetDpsPriorityAction::DescribeTargetRule(Unit* unit)
{
    if (!unit)
        return "none";

    switch (unit->GetEntry())
    {
        case NPC_BOMB_BOT:            return "bombbot";
        case NPC_ASSAULT_BOT:         return "assaultbot";
        case NPC_EMERGENCY_FIRE_BOT:  return "firebot";
        case NPC_JUNK_BOT:            return "junkbot";
        case NPC_AERIAL_COMMAND_UNIT: return "acu";
        case NPC_VX001:               return "vx001";
        case NPC_LEVIATHAN_MKII:      return "mkii";
        default:                      return "other";
    }
}

Unit* MimironSetDpsPriorityAction::ResolveTarget(Unit* currentTarget, bool allowFallback)
{
    std::vector<std::pair<uint32, Unit*>> const priority = BuildPriorityList();

    Unit* target = nullptr;
    for (auto const& candidate : priority)
    {
        if (IsAllowedTarget(candidate.second))
        {
            target = candidate.second;
            break;
        }
    }

    auto const priorityIndex = [&](Unit* unit) -> size_t
    {
        if (!IsAllowedTarget(unit))
            return priority.size();

        for (size_t index = 0; index < priority.size(); ++index)
        {
            if (priority[index].first == unit->GetEntry())
                return index;
        }

        return priority.size();
    };

    bool held = false;
    if (currentTarget && IsAllowedTarget(currentTarget) &&
        priorityIndex(currentTarget) <= priorityIndex(target))
    {
        held = target != currentTarget;
        target = currentTarget;
    }

    if (target)
    {
        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "mimiron.dpsrule",
                                 held ? std::string("held:") + DescribeTargetRule(target)
                                      : std::string(DescribeTargetRule(target)));

        return target;
    }

    if (!allowFallback)
        return nullptr;

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "mimiron.dpsrule", "fallback");

    return AI_VALUE(Unit*, "dps target");
}

bool MimironSetDpsPriorityAction::Execute(Event /*event*/)
{
    Unit* currentTarget = AI_VALUE(Unit*, "current target");

    if (IsMimironPhase4(bot) && !PlayerbotAI::IsHeal(bot) &&
        !GetMimironPhase4Focus(botAI, bot, PlayerbotAI::IsMelee(bot)))
    {
        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "mimiron.dpsrule", "p4hold");

        bot->AttackStop();
        return true;
    }

    bool const phase3Wait =
        PlayerbotAI::IsMelee(bot) && !PlayerbotAI::IsHeal(bot) && IsMimironAcuAirborne(botAI, bot);

    Unit* target = ResolveTarget(currentTarget, !phase3Wait);
    if (!target)
    {
        if (!phase3Wait)
            return false;

        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "mimiron.dpsrule", "p3hold");

        bot->AttackStop();
        return true;
    }

    if (PlayerbotAI::IsMelee(bot) && !bot->IsWithinLOSInMap(target))
        return ApproachOutOfSight(target);

    bool needsAttack = currentTarget != target;
    if (PlayerbotAI::IsMelee(bot))
        needsAttack = needsAttack || !bot->HasUnitState(UNIT_STATE_MELEE_ATTACKING);

    return needsAttack ? Attack(target) : false;
}

bool MimironSetDpsPriorityAction::ApproachOutOfSight(Unit* target)
{
    Position slot;
    MimironP3Wx2LaserBarrageTrigger barrage(botAI);
    if (GetMimironSpreadSlot(botAI, bot, slot) || barrage.IsActive() || IsMimironNearFrostBomb(botAI) ||
        IsMimironNearRocket(botAI) || IsMimironHeldByFire(botAI))
        return false;

    Position approach;
    if (!GetMimironSightApproach(botAI, bot, target, GetMimironApproachRange(botAI, bot), approach))
        return false;

    if (TryMoveTo(bot->GetMapId(), approach.GetPositionX(), approach.GetPositionY(),
                  approach.GetPositionZ(), false, false, false, true,
                  MovementPriority::MOVEMENT_COMBAT, true) != MoveOutcome::Issued)
        return false;

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "mimiron.close", "sight");

    return true;
}

bool MimironPlasmaBlastDefensiveAction::isUseful()
{
    MimironPlasmaBlastDefensiveTrigger mimironPlasmaBlastDefensiveTrigger(botAI);
    return mimironPlasmaBlastDefensiveTrigger.IsActive();
}

Unit* MimironPlasmaBlastDefensiveAction::PlasmaVictim()
{
    Creature* cannon = bot->FindNearestCreature(NPC_LEVIATHAN_MKII_CANNON, ULDUAR_MIMIRON_ROOM_SEARCH_RANGE);
    if (!cannon)
        return nullptr;

    Spell* spell = GetMimironPlasmaBlastCast(cannon);
    if (!spell)
        return nullptr;

    Unit* victim = spell->m_targets.GetUnitTarget();
    return victim && victim->IsAlive() ? victim : nullptr;
}

bool MimironPlasmaBlastDefensiveAction::TankHasDefensive(Unit* victim)
{
    Player* tank = victim ? victim->ToPlayer() : nullptr;
    if (!tank)
        return false;

    PlayerbotAI* tankAI = GET_PLAYERBOT_AI(tank);
    return tankAI && NextTankDefensive(tankAI, tank, nullptr) != 0;
}

bool MimironPlasmaBlastDefensiveAction::Execute(Event /*event*/)
{
    Unit* victim = PlasmaVictim();
    if (!victim)
        return false;

    if (victim == bot)
    {
        uint32 const defensive = NextTankDefensive(botAI, bot, "mimiron.plasma");
        if (!defensive)
            return false;

        return ClaimMimironPlasmaWindow(bot) && botAI->CastSpell(defensive, bot);
    }

    if (TankHasDefensive(victim))
        return false;

    for (char const* external : HEALER_EXTERNALS)
    {
        if (!botAI->CanCastSpell(external, victim))
            continue;

        if (!ClaimMimironPlasmaWindow(bot))
            return false;

        if (RaidObs::Active())
            RaidObs::NoteDerived(bot, "mimiron.plasma", external);

        return botAI->CastSpell(external, victim);
    }

    return false;
}

Player* MimironRedirectThreatAction::GetRedirectTank()
{
    if (!GetUldUnitByEntry(botAI, NPC_LEVIATHAN_MKII) ||
        GetUldUnitByEntry(botAI, NPC_VX001) ||
        GetUldUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT))
        return nullptr;

    Unit* mainTank = AI_VALUE(Unit*, "main tank");
    return mainTank ? mainTank->ToPlayer() : nullptr;
}

Unit* MimironRedirectThreatAction::GetThreatDumpTarget()
{
    return GetUldUnitByEntry(botAI, NPC_LEVIATHAN_MKII);
}

bool MimironSlowBombBotAction::isUseful()
{
    MimironSlowBombBotTrigger mimironSlowBombBotTrigger(botAI);
    return mimironSlowBombBotTrigger.IsActive();
}

bool MimironSlowBombBotAction::Execute(Event event)
{
    std::string const spell = GetMimironBombBotSnare(bot);
    if (spell.empty())
        return false;

    return botAI->DoSpecificAction(spell, event, true);
}

bool MimironMagneticCoreAction::isUseful()
{
    MimironMagneticCoreTrigger mimironMagneticCoreTrigger(botAI);
    return mimironMagneticCoreTrigger.IsActive();
}

void MimironMagneticCoreAction::NoteCoreStep(char const* step)
{
    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "mimiron.corestep", step);
}

bool MimironMagneticCoreAction::Execute(Event /*event*/)
{
    Unit* aerialCommandUnit = GetUldUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT);
    if (!aerialCommandUnit)
    {
        NoteCoreStep("no-acu");
        return false;
    }

    uint32 const held = bot->GetItemCount(ITEM_MIMIRON_MAGNETIC_CORE, false);
    Item* core = bot->GetItemByEntry(ITEM_MIMIRON_MAGNETIC_CORE);

    uint32 const remaining = core ? core->GetUInt32Value(ITEM_FIELD_DURATION) : 0;
    bool const expiring = remaining && remaining * IN_MILLISECONDS <= ULDUAR_MIMIRON_CORE_HOLD_EXPIRY_MS;
    bool const acuLow = aerialCommandUnit->GetHealthPct() <= ULDUAR_MIMIRON_CORE_HOLD_RELEASE_PCT;

    bool const ready = IsMimironCoreUseReady(botAI, bot);

    if (!core && ready)
        chaining = false;

    auto const waitUnderAcu = [&](char const* step)
    {
        NoteCoreStep(step);
        if (bot->GetExactDist2d(aerialCommandUnit) <= ULDUAR_MIMIRON_CORE_USE_RANGE)
            return false;

        return MoveTo(aerialCommandUnit->GetMapId(), aerialCommandUnit->GetPositionX(),
                      aerialCommandUnit->GetPositionY(), bot->GetPositionZ(), false, false, false,
                      true, MovementPriority::MOVEMENT_COMBAT, true);
    };

    bool const spendHeldNow = core && (expiring || acuLow || (chaining && ready));
    if (held < ULDUAR_MIMIRON_CORE_BANK && !spendHeldNow)
    {
        Creature* corpse = GetMimironCoreCorpse(bot);
        if (!corpse && !core)
        {
            NoteCoreStep("no-corpse");
            return false;
        }

        if (corpse)
        {
            if (bot->GetExactDist2d(corpse) > ULDUAR_MIMIRON_CORE_LOOT_RANGE)
            {
                NoteCoreStep(core ? "walk-corpse-second" : "walk-corpse");
                return MoveTo(corpse->GetMapId(), corpse->GetPositionX(), corpse->GetPositionY(),
                              corpse->GetPositionZ(), false, false, false, true,
                              MovementPriority::MOVEMENT_COMBAT, true);
            }

            if (TakeMimironCore(bot, corpse))
            {
                NoteCoreStep(core ? "loot-second" : "loot");
                return true;
            }

            if (!core)
            {
                NoteCoreStep("bags-full");
                return false;
            }
        }
    }

    if (!core)
    {
        NoteCoreStep("no-corpse");
        return false;
    }

    if (!ready)
        return waitUnderAcu("pending");

    char const* release = nullptr;
    if (chaining)
        release = "chain";
    else if (held < ULDUAR_MIMIRON_CORE_BANK && !acuLow)
    {
        if (expiring)
            release = "hold-expiring";
        else if (bot->FindNearestCreature(NPC_ASSAULT_BOT, ULDUAR_MIMIRON_CORE_SEARCH_RANGE, true))
            release = "hold-bot-up";
        else
            return waitUnderAcu("hold");
    }

    if (bot->GetExactDist2d(aerialCommandUnit) > ULDUAR_MIMIRON_CORE_USE_RANGE)
    {
        NoteCoreStep("walk-acu");
        return MoveTo(aerialCommandUnit->GetMapId(), aerialCommandUnit->GetPositionX(),
                      aerialCommandUnit->GetPositionY(), bot->GetPositionZ(), false, false, false,
                      true, MovementPriority::MOVEMENT_COMBAT, true);
    }

    if (bot->CanUseItem(core) != EQUIP_ERR_OK || bot->IsNonMeleeSpellCast(false))
    {
        NoteCoreStep("blocked");
        return false;
    }

    uint32 spellId = 0;
    for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        if (core->GetTemplate()->Spells[i].SpellId > 0)
        {
            spellId = core->GetTemplate()->Spells[i].SpellId;
            break;
        }
    }

    if (!spellId)
    {
        NoteCoreStep("blocked");
        return false;
    }

    if (release)
        NoteCoreStep(release);

    NoteCoreStep("use");

    uint8 const bagIndex = core->GetBagSlot();
    uint8 const slot = core->GetSlot();
    constexpr uint8 castCount = 0;
    constexpr uint32 glyphIndex = 0;
    constexpr uint8 castFlags = 0;

    WorldPacket packet(CMSG_USE_ITEM);
    packet << bagIndex;
    packet << slot;
    packet << castCount;
    packet << spellId;
    packet << core->GetGUID();
    packet << glyphIndex;
    packet << castFlags;
    packet << (uint32)TARGET_FLAG_NONE;

    bot->GetSession()->HandleUseItemOpcode(packet);
    NoteMimironCoreSpent(bot);
    chaining = true;
    return true;
}

bool MimironFrostResistanceAction::Execute(Event /*event*/)
{
    return botAI->DoSpecificAction("frost resistance aura", Event(), true);
}
