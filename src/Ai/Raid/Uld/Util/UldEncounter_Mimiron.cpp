/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldEncounter_Mimiron.h"

#include "BossAuraTriggers.h"
#include "Creature.h"
#include "EncounterHelpers.h"
#include "Group.h"
#include "Item.h"
#include "LootMgr.h"
#include "Map.h"
#include "PathGenerator.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "RaidInstanceState.h"
#include "RaidObs.h"
#include "ServerFacade.h"
#include "SpellAuras.h"
#include "ThreatManager.h"
#include "Timer.h"
#include "UldEncounterGate.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "UldTargets.h"
#include "Unit.h"
#include "Vehicle.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <list>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace EncounterHelpers;

const Position ULDUAR_MIMIRON_ROOM_CENTER = Position(2744.65f, 2569.46f, 364.32f);
const Position ULDUAR_MIMIRON_PHASE3_STAGE = Position(2762.65f, 2569.46f, 364.31f);
const Position ULDUAR_MIMIRON_PHASE4_TANK_SPOT = Position(2744.5754f, 2570.8657f, 364.3138f);
const Position ULDUAR_MIMIRON_PHASE1_TANK_SPOT = Position(2691.5762f, 2568.5315f, 364.3138f);
const Position ULDUAR_MIMIRON_PHASE1_STACK_SPOTS[ULDUAR_MIMIRON_PHASE1_STACK_COUNT] = {
    Position(2716.0465f, 2579.9422f, 364.3138f),
    Position(2717.6562f, 2561.5434f, 364.3138f),
};

namespace
{
std::list<Creature*> FindMimironFloorUnits(Player* bot, std::vector<uint32> const& entries)
{
    std::list<Creature*> units;
    if (bot)
        bot->GetCreatureListWithEntryInGrid(units, entries, sPlayerbotAIConfig.SightDistance);

    return units;
}

Position MimironOrbitAhead(Position const& now, float seconds)
{
    float const dx = now.GetPositionX() - ULDUAR_MIMIRON_ROOM_CENTER.GetPositionX();
    float const dy = now.GetPositionY() - ULDUAR_MIMIRON_ROOM_CENTER.GetPositionY();
    float const radius = std::sqrt(dx * dx + dy * dy);
    if (radius < 1.0f)
        return now;

    float const turned = -ULDUAR_MIMIRON_DB_TARGET_SPEED * seconds / radius;
    float const c = std::cos(turned);
    float const sn = std::sin(turned);

    return Position(ULDUAR_MIMIRON_ROOM_CENTER.GetPositionX() + dx * c - dy * sn,
                    ULDUAR_MIMIRON_ROOM_CENTER.GetPositionY() + dx * sn + dy * c,
                    now.GetPositionZ());
}
}

float GetMimironSpinningUpSeconds(Unit* vx001)
{
    if (!vx001)
        return -1.0f;

    Spell* spinningUp = vx001->FindCurrentSpellBySpellId(SPELL_SPINNING_UP);
    if (!spinningUp)
        return -1.0f;

    return std::max(0.0f, static_cast<float>(spinningUp->GetCastTimeRemaining()) / IN_MILLISECONDS);
}

namespace
{
MimironBarrageWindow ReadMimironBarrageWindow(Player* bot, Unit* vx001)
{
    MimironBarrageWindow window;
    if (!bot || !vx001)
        return window;

    float fire = ULDUAR_MIMIRON_BARRAGE_FIRE_SECONDS;
    float const spinning = GetMimironSpinningUpSeconds(vx001);
    if (spinning >= 0.0f)
        window.untilLive = spinning;
    else if (Aura* barrage = vx001->GetAura(SPELL_P3WX2_LASER_BARRAGE_AURA_1))
        fire = static_cast<float>(barrage->GetDuration()) / IN_MILLISECONDS;
    else
        return window;

    window.valid = true;

    Creature* dbTarget = bot->FindNearestCreature(NPC_MIMIRON_DB_TARGET, ULDUAR_MIMIRON_DB_TARGET_SEARCH_RANGE);
    if (!dbTarget)
    {
        window.lead = vx001->GetOrientation();
        return window;
    }

    Position const dbNow = dbTarget->GetPosition();
    Position const ignition = MimironOrbitAhead(dbNow, window.untilLive);
    Position const finish = MimironOrbitAhead(dbNow, window.untilLive + fire);

    window.lead = vx001->GetAngle(ignition.GetPositionX(), ignition.GetPositionY());

    float const tail = vx001->GetAngle(finish.GetPositionX(), finish.GetPositionY());
    window.sweep = Position::NormalizeOrientation(window.lead - tail);
    window.rate = fire > 0.0f ? window.sweep / fire : 0.0f;

    return window;
}
}

bool IsMimironSpotMineSafe(Player* bot, Position const& dest, float clearance)
{
    if (!bot)
        return true;

    for (Creature* mine : FindMimironFloorUnits(bot, {NPC_PROXIMITY_MINE}))
    {
        if (!mine->IsAlive())
            continue;

        if (dest.GetExactDist2d(mine->GetPositionX(), mine->GetPositionY()) < clearance)
            return false;
    }

    return true;
}

namespace
{
MimironFirefighterHazards ReadMimironFirefighterHazards(PlayerbotAI* botAI)
{
    MimironFirefighterHazards hazards;
    if (!botAI || !IsMimironHardModeActive(botAI))
        return hazards;

    std::vector<Position> spread;
    for (Creature* unit :
         FindMimironFloorUnits(botAI->GetBot(), {NPC_FLAMES_SPREAD, NPC_FROST_BOMB, NPC_EMERGENCY_FIRE_BOT}))
    {
        if (!unit->IsAlive())
            continue;

        switch (unit->GetEntry())
        {
            case NPC_FLAMES_SPREAD:
                spread.push_back(unit->GetPosition());
                if (unit->HasAura(SPELL_MIMIRON_FLAMES_AURA))
                    hazards.flames.push_back(unit->GetPosition());
                break;
            case NPC_FROST_BOMB:
                hazards.bombs.push_back(unit->GetPosition());
                break;
            case NPC_EMERGENCY_FIRE_BOT:
                hazards.fireBots.push_back(unit->GetPosition());
                break;
            default:
                break;
        }
    }

    for (Position const& fireBot : hazards.fireBots)
    {
        hazards.sprayLanes.push_back(fireBot);

        Position const* target = nullptr;
        float targetDist = 0.0f;
        for (Position const& flame : spread)
        {
            float const dist = fireBot.GetExactDist2d(flame.GetPositionX(), flame.GetPositionY());
            if (!target || dist < targetDist)
            {
                target = &flame;
                targetDist = dist;
            }
        }

        if (!target)
            continue;

        float const facing = fireBot.GetAngle(target->GetPositionX(), target->GetPositionY());
        float const walk = std::max(targetDist - ULDUAR_MIMIRON_FIREBOT_SPRAY_REACH, 0.0f);
        hazards.sprayLanes.emplace_back(fireBot.GetPositionX() + walk * std::cos(facing),
                                        fireBot.GetPositionY() + walk * std::sin(facing),
                                        fireBot.GetPositionZ(), facing);
    }

    return hazards;
}
}

bool IsMimironSpotFireSafe(MimironFirefighterHazards const& hazards, Position const& dest)
{
    for (Position const& node : hazards.flames)
        if (dest.GetExactDist2d(node.GetPositionX(), node.GetPositionY()) < ULDUAR_MIMIRON_FLAMES_RADIUS)
            return false;

    return true;
}

bool IsMimironSpotBurning(MimironFirefighterHazards const& hazards, Position const& pos)
{
    for (Position const& node : hazards.flames)
        if (pos.GetExactDist2d(node.GetPositionX(), node.GetPositionY()) < ULDUAR_MIMIRON_FLAMES_BURN_RADIUS)
            return true;

    return false;
}

bool IsMimironSpotBombSafe(MimironFirefighterHazards const& hazards, Position const& dest)
{
    for (Position const& bomb : hazards.bombs)
        if (dest.GetExactDist2d(bomb.GetPositionX(), bomb.GetPositionY()) < ULDUAR_MIMIRON_FROST_BOMB_STAND_RADIUS)
            return false;

    return true;
}

bool IsMimironSpotInFireBotSpray(MimironFirefighterHazards const& hazards, Position const& dest,
                                 float halfWidth)
{
    for (Position const& lane : hazards.sprayLanes)
    {
        float const dx = dest.GetPositionX() - lane.GetPositionX();
        float const dy = dest.GetPositionY() - lane.GetPositionY();

        float const facing = lane.GetOrientation();
        float const ahead = dx * std::cos(facing) + dy * std::sin(facing);
        float const side = dy * std::cos(facing) - dx * std::sin(facing);
        if (ahead >= 0.0f && ahead < ULDUAR_MIMIRON_FIREBOT_SPRAY_LENGTH && std::fabs(side) < halfWidth)
            return true;
    }

    return false;
}

bool IsMimironSpotFireBotSafe(Player* bot, MimironFirefighterHazards const& hazards, Position const& dest,
                              float sirenRadius, float sprayHalfWidth)
{
    if (hazards.fireBots.empty())
        return true;

    bool const silenced = bot && (PlayerbotAI::IsCaster(bot) || PlayerbotAI::IsHeal(bot)) &&
                          bot->GetMap()->Is25ManRaid();

    if (silenced)
        for (Position const& fireBot : hazards.fireBots)
            if (dest.GetExactDist2d(fireBot.GetPositionX(), fireBot.GetPositionY()) < sirenRadius)
                return false;

    return !IsMimironSpotInFireBotSpray(hazards, dest, sprayHalfWidth);
}

bool IsMimironSpotShockSafe(PlayerbotAI* botAI, Position const& dest)
{
    Unit* leviathanMkII = botAI ? GetUldUnitByEntry(botAI, NPC_LEVIATHAN_MKII) : nullptr;
    if (!leviathanMkII || !leviathanMkII->FindCurrentSpellBySpellId(SPELL_SHOCK_BLAST))
        return true;

    return leviathanMkII->GetExactDist2d(dest.GetPositionX(), dest.GetPositionY()) >=
           ULDUAR_MIMIRON_SHOCK_BLAST_SAFE_DIST;
}

float GetMimironHolderRadius(PlayerbotAI* botAI, Player* bot)
{
    Unit* leviathanMkII = botAI && bot ? GetUldUnitByEntry(botAI, NPC_LEVIATHAN_MKII) : nullptr;
    if (!leviathanMkII || leviathanMkII->GetVictim() != bot || !MimironPhase1Active(botAI))
        return 0.0f;

    float const spot = ULDUAR_MIMIRON_ROOM_CENTER.GetExactDist2d(ULDUAR_MIMIRON_PHASE1_TANK_SPOT) +
                       ULDUAR_MIMIRON_HOLDER_RADIUS_SLACK;
    return std::max(spot, bot->GetExactDist2d(ULDUAR_MIMIRON_ROOM_CENTER.GetPositionX(),
                                              ULDUAR_MIMIRON_ROOM_CENTER.GetPositionY()));
}

bool IsMimironSpotHolderSafe(float holderRadius, Position const& dest)
{
    return holderRadius <= 0.0f ||
           ULDUAR_MIMIRON_ROOM_CENTER.GetExactDist2d(dest.GetPositionX(), dest.GetPositionY()) <= holderRadius;
}

Unit* GetMimironUnreachableMech(PlayerbotAI* botAI, Player* bot)
{
    Unit* leviathanMkII = botAI && bot ? GetUldUnitByEntry(botAI, NPC_LEVIATHAN_MKII) : nullptr;
    Creature* chaser = leviathanMkII ? leviathanMkII->ToCreature() : nullptr;
    if (!chaser || chaser->GetCannotReachTarget() != bot->GetGUID())
        return nullptr;

    return leviathanMkII;
}

namespace
{
struct MimironBombBotMarker
{
    ObjectGuid victim;
    MimironBombBotBlast blast;
};

MimironBombBotMarker ReadMimironBombBotMarker(Creature* bombBot)
{
    MimironBombBotMarker marker;
    Unit* const victim = ServerFacade::instance().GetChaseTarget(bombBot);
    marker.victim = victim ? victim->GetGUID() : ObjectGuid::Empty;

    Position const bomb = bombBot->GetPosition();
    Position const centre = victim ? victim->GetPosition() : bomb;
    float const gap = centre.GetExactDist2d(bomb);
    float const share = gap > 0.0f ? std::min(gap, ULDUAR_MIMIRON_BOMB_BOT_DETONATION_REACH) / gap : 0.0f;

    marker.blast.valid = true;
    marker.blast.bomb = bomb;
    marker.blast.centre = centre;
    marker.blast.reach = Position(centre.GetPositionX() + share * (bomb.GetPositionX() - centre.GetPositionX()),
                                  centre.GetPositionY() + share * (bomb.GetPositionY() - centre.GetPositionY()),
                                  centre.GetPositionZ());

    return marker;
}

struct MimironMarkers
{
    std::vector<Position> mines;
    std::vector<Position> rockets;
    std::vector<MimironBombBotMarker> bombBots;
    bool shock = false;
    Position shockCentre;
};

MimironMarkers ReadMimironMarkers(PlayerbotAI* botAI)
{
    MimironMarkers markers;
    for (Creature* unit :
         FindMimironFloorUnits(botAI->GetBot(), {NPC_PROXIMITY_MINE, NPC_ROCKET_STRIKE_N, NPC_BOMB_BOT}))
    {
        if (!unit->IsAlive())
            continue;

        if (unit->GetEntry() == NPC_PROXIMITY_MINE)
            markers.mines.push_back(unit->GetPosition());
        else if (unit->GetEntry() == NPC_ROCKET_STRIKE_N)
            markers.rockets.push_back(unit->GetPosition());
        else if (unit->GetEntry() == NPC_BOMB_BOT)
            markers.bombBots.push_back(ReadMimironBombBotMarker(unit));
    }

    Unit* leviathanMkII = GetUldUnitByEntry(botAI, NPC_LEVIATHAN_MKII);
    if (leviathanMkII && leviathanMkII->FindCurrentSpellBySpellId(SPELL_SHOCK_BLAST))
    {
        markers.shock = true;
        markers.shockCentre = leviathanMkII->GetPosition();
    }

    return markers;
}

struct MimironPassReads
{
    PlayerbotAI* botAI = nullptr;
    uint32 passId = 0;

    bool hazardsRead = false;
    MimironFirefighterHazards hazards;

    bool markersRead = false;
    MimironMarkers markers;

    bool burstRead = false;
    MimironRapidBurstWindow burst;

    bool barrageRead = false;
    MimironBarrageWindow barrage;
};

thread_local MimironPassReads mimironPassReads;

MimironPassReads* MimironPassReadsFor(PlayerbotAI* botAI)
{
    uint32 const passId = EncounterTriggerPassId(botAI);
    if (!passId)
        return nullptr;

    if (mimironPassReads.botAI != botAI || mimironPassReads.passId != passId)
    {
        mimironPassReads = MimironPassReads();
        mimironPassReads.botAI = botAI;
        mimironPassReads.passId = passId;
    }

    return &mimironPassReads;
}

MimironMarkers GetMimironMarkers(PlayerbotAI* botAI)
{
    MimironPassReads* pass = MimironPassReadsFor(botAI);
    if (!pass)
        return ReadMimironMarkers(botAI);

    if (!pass->markersRead)
    {
        pass->markers = ReadMimironMarkers(botAI);
        pass->markersRead = true;
    }

    return pass->markers;
}

bool IsMimironSpotStandable(Player* bot, Position const& dest, MimironMarkers const& markers,
                            MimironFirefighterHazards const& hazards)
{
    for (Position const& mine : markers.mines)
        if (dest.GetExactDist2d(mine.GetPositionX(), mine.GetPositionY()) < ULDUAR_MIMIRON_MINE_CLEARANCE)
            return false;

    for (Position const& rocket : markers.rockets)
        if (dest.GetExactDist2d(rocket.GetPositionX(), rocket.GetPositionY()) < ULDUAR_MIMIRON_ROCKET_CLEARANCE)
            return false;

    if (markers.shock && dest.GetExactDist2d(markers.shockCentre.GetPositionX(),
                                             markers.shockCentre.GetPositionY()) <
                             ULDUAR_MIMIRON_SHOCK_BLAST_SAFE_DIST)
        return false;

    for (MimironBombBotMarker const& bombBot : markers.bombBots)
        if ((!bot || bombBot.victim != bot->GetGUID()) &&
            dest.GetExactDist2d(GetMimironBombBotBlastNearest(bombBot.blast, dest)) <
                ULDUAR_MIMIRON_BOMB_BOT_BLAST_CLEARANCE)
            return false;

    return IsMimironSpotFireSafe(hazards, dest) && IsMimironSpotBombSafe(hazards, dest) &&
           IsMimironSpotFireBotSafe(bot, hazards, dest, ULDUAR_MIMIRON_FIREBOT_SIREN_STAND,
                                    ULDUAR_MIMIRON_FIREBOT_SPRAY_STAND_HALF_WIDTH);
}

uint32 CountMimironLegHazards(Position const& from, std::vector<Position> const& nodes, float radius,
                              Position const& dest)
{
    float const fromX = from.GetPositionX();
    float const fromY = from.GetPositionY();
    float const dx = dest.GetPositionX() - fromX;
    float const dy = dest.GetPositionY() - fromY;
    float const lengthSq = dx * dx + dy * dy;
    if (lengthSq <= 0.0f)
        return 0;

    uint32 passed = 0;
    for (Position const& node : nodes)
    {
        float const nx = node.GetPositionX() - fromX;
        float const ny = node.GetPositionY() - fromY;
        if (std::sqrt(nx * nx + ny * ny) < radius)
            continue;

        float const t = std::clamp((nx * dx + ny * dy) / lengthSq, 0.0f, 1.0f);
        float const ox = nx - t * dx;
        float const oy = ny - t * dy;
        if (std::sqrt(ox * ox + oy * oy) < radius)
            ++passed;
    }

    return passed;
}

bool IsMimironLegClearOf(Position const& from, std::vector<Position> const& nodes, float radius,
                         Position const& dest)
{
    return CountMimironLegHazards(from, nodes, radius, dest) == 0;
}

bool IsMimironLegFireSafe(Position const& from, MimironFirefighterHazards const& hazards,
                          Position const& dest)
{
    return IsMimironLegClearOf(from, hazards.flames, ULDUAR_MIMIRON_FLAMES_BURN_RADIUS, dest);
}

struct MimironLegBurn
{
    float yards = 0.0f;
    uint32 overlap = 0;
};

MimironLegBurn MeasureMimironLegBurn(Position const& from, MimironFirefighterHazards const& hazards,
                                     Position const& dest)
{
    MimironLegBurn burn;
    float const fromX = from.GetPositionX();
    float const fromY = from.GetPositionY();
    float const dx = dest.GetPositionX() - fromX;
    float const dy = dest.GetPositionY() - fromY;
    float const length = std::sqrt(dx * dx + dy * dy);
    if (length <= 0.0f)
        return burn;

    float const ux = dx / length;
    float const uy = dy / length;
    float const radius = ULDUAR_MIMIRON_FLAMES_BURN_RADIUS;

    std::vector<std::pair<float, int>> ends;
    for (Position const& node : hazards.flames)
    {
        float const nx = node.GetPositionX() - fromX;
        float const ny = node.GetPositionY() - fromY;
        float const along = nx * ux + ny * uy;
        float const side = ny * ux - nx * uy;
        float const halfSq = radius * radius - side * side;
        if (halfSq <= 0.0f)
            continue;

        float const half = std::sqrt(halfSq);
        float const enter = std::max(along - half, 0.0f);
        float const leave = std::min(along + half, length);
        if (leave <= enter)
            continue;

        ends.emplace_back(enter, 1);
        ends.emplace_back(leave, -1);
    }

    // On a tie leaves sort before enters, so two burns that only touch don't count as stacked.
    std::sort(ends.begin(), ends.end(), [](auto const& a, auto const& b)
              { return a.first != b.first ? a.first < b.first : a.second < b.second; });

    int depth = 0;
    float openedAt = 0.0f;
    for (auto const& [at, step] : ends)
    {
        if (depth == 0 && step > 0)
            openedAt = at;

        depth += step;
        burn.overlap = std::max(burn.overlap, static_cast<uint32>(std::max(depth, 0)));
        if (depth == 0 && step < 0)
            burn.yards += at - openedAt;
    }

    return burn;
}
}

std::vector<Position> GetMimironMines(PlayerbotAI* botAI)
{
    return botAI ? GetMimironMarkers(botAI).mines : std::vector<Position>();
}

bool IsMimironLegMineSafe(Position const& from, std::vector<Position> const& mines, Position const& dest)
{
    return IsMimironLegClearOf(from, mines, ULDUAR_MIMIRON_MINE_LEG_CLEARANCE, dest);
}

bool IsMimironPathMineSafe(Player* bot, std::vector<Position> const& mines, Position const& dest)
{
    if (!bot || mines.empty())
        return true;

    Position const from = bot->GetPosition();
    float const midX = 0.5f * (from.GetPositionX() + dest.GetPositionX());
    float const midY = 0.5f * (from.GetPositionY() + dest.GetPositionY());
    float const reach = 0.5f * from.GetExactDist2d(dest.GetPositionX(), dest.GetPositionY()) +
                        ULDUAR_MIMIRON_MINE_LEG_CLEARANCE + ULDUAR_MIMIRON_PATH_BEND_PAD;
    bool const mineNear = std::any_of(mines.begin(), mines.end(), [&](Position const& mine)
                                      { return mine.GetExactDist2d(midX, midY) < reach; });
    if (!mineNear)
        return true;

    PathGenerator path(bot);
    if (!path.CalculatePath(dest.GetPositionX(), dest.GetPositionY(), dest.GetPositionZ()) ||
        (path.GetPathType() & PATHFIND_NOPATH))
        return true;

    Movement::PointsArray const& points = path.GetPath();
    if (points.size() <= 2)
        return true;

    for (size_t i = 1; i < points.size(); ++i)
    {
        Position const start(points[i - 1].x, points[i - 1].y, points[i - 1].z);
        Position const end(points[i].x, points[i].y, points[i].z);
        if (!IsMimironLegMineSafe(start, mines, end))
            return false;
    }

    return true;
}

MimironBarrageWindow GetMimironBarrageWindow(Player* bot, Unit* vx001)
{
    PlayerbotAI* botAI = bot ? GET_PLAYERBOT_AI(bot) : nullptr;
    MimironPassReads* pass = botAI ? MimironPassReadsFor(botAI) : nullptr;
    if (!pass)
        return ReadMimironBarrageWindow(bot, vx001);

    if (!pass->barrageRead)
    {
        pass->barrage = ReadMimironBarrageWindow(bot, vx001);
        pass->barrageRead = true;
    }

    return pass->barrage;
}

MimironFirefighterHazards GetMimironFirefighterHazards(PlayerbotAI* botAI)
{
    MimironPassReads* pass = MimironPassReadsFor(botAI);
    if (!pass)
        return ReadMimironFirefighterHazards(botAI);

    if (!pass->hazardsRead)
    {
        pass->hazards = ReadMimironFirefighterHazards(botAI);
        pass->hazardsRead = true;
    }

    return pass->hazards;
}

bool IsMimironWalkFireSafe(Player* bot, MimironFirefighterHazards const& hazards, Position const& dest)
{
    return !bot || IsMimironLegFireSafe(bot->GetPosition(), hazards, dest);
}

bool IsMimironWalkMineSafe(Player* bot, std::vector<Position> const& mines, Position const& dest)
{
    if (!bot)
        return true;

    for (Position const& mine : mines)
        if (dest.GetExactDist2d(mine.GetPositionX(), mine.GetPositionY()) < ULDUAR_MIMIRON_MINE_CLEARANCE)
            return false;

    return IsMimironLegMineSafe(bot->GetPosition(), mines, dest);
}

bool IsMimironSpotSafe(Player* bot, Position const& dest)
{
    PlayerbotAI* botAI = bot ? GET_PLAYERBOT_AI(bot) : nullptr;
    if (!botAI)
        return true;

    return IsMimironSpotStandable(bot, dest, GetMimironMarkers(botAI), GetMimironFirefighterHazards(botAI));
}

bool IsMimironSpotRocketSafe(PlayerbotAI* botAI, Position const& dest)
{
    if (!botAI)
        return true;

    MimironMarkers const markers = GetMimironMarkers(botAI);
    for (Position const& rocket : markers.rockets)
        if (dest.GetExactDist2d(rocket.GetPositionX(), rocket.GetPositionY()) < ULDUAR_MIMIRON_ROCKET_CLEARANCE)
            return false;

    return true;
}

MimironBombBotBlast GetMimironBombBotBlast(PlayerbotAI* botAI, Player* bot, float within)
{
    MimironBombBotBlast blast;
    if (!botAI || !bot || !IsNearMimironRoom(bot))
        return blast;

    float nearest = within;
    for (MimironBombBotMarker const& bombBot : GetMimironMarkers(botAI).bombBots)
    {
        if (bombBot.victim == bot->GetGUID())
        {
            if (PlayerbotAI::IsRangedDps(bot) ||
                bot->GetExactDist2d(bombBot.blast.bomb) >= ULDUAR_MIMIRON_BOMB_BOT_RADIUS)
                continue;

            blast = bombBot.blast;
            blast.chased = true;
            return blast;
        }

        float const dist = bot->GetExactDist2d(GetMimironBombBotBlastNearest(bombBot.blast, bot->GetPosition()));
        if (dist < nearest)
        {
            nearest = dist;
            blast = bombBot.blast;
        }
    }

    return blast;
}

std::vector<MimironBombBotBlast> GetMimironBombBotBlasts(PlayerbotAI* botAI, Player* bot)
{
    std::vector<MimironBombBotBlast> blasts;
    if (!botAI || !bot)
        return blasts;

    for (MimironBombBotMarker const& bombBot : GetMimironMarkers(botAI).bombBots)
        if (bombBot.victim != bot->GetGUID())
            blasts.push_back(bombBot.blast);

    return blasts;
}

Position GetMimironBombBotBlastNearest(MimironBombBotBlast const& blast, Position const& at)
{
    float const fromX = blast.centre.GetPositionX();
    float const fromY = blast.centre.GetPositionY();
    float const dx = blast.reach.GetPositionX() - fromX;
    float const dy = blast.reach.GetPositionY() - fromY;
    float const lengthSq = dx * dx + dy * dy;
    float t = 0.0f;
    if (lengthSq > 0.0f)
        t = std::clamp(((at.GetPositionX() - fromX) * dx + (at.GetPositionY() - fromY) * dy) / lengthSq, 0.0f, 1.0f);

    return Position(fromX + t * dx, fromY + t * dy, blast.centre.GetPositionZ());
}

bool IsMimironSpotBombBotSafe(std::vector<MimironBombBotBlast> const& blasts, Position const& dest)
{
    for (MimironBombBotBlast const& blast : blasts)
        if (dest.GetExactDist2d(GetMimironBombBotBlastNearest(blast, dest)) < ULDUAR_MIMIRON_BOMB_BOT_BLAST_CLEARANCE)
            return false;

    return true;
}

float GetMimironApproachRange(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot)
        return 0.0f;

    return PlayerbotAI::IsMelee(bot) ? sPlayerbotAIConfig.MeleeDistance : botAI->GetRange("spell");
}

namespace
{
bool FindMimironApproachSpot(PlayerbotAI* botAI, Player* bot, Unit* target, float range, bool roundFire,
                             Position& out)
{
    if (!botAI || !bot || !target)
        return false;

    if (bot->IsWithinCombatRange(target, range))
        return false;

    MimironFirefighterHazards const hazards = GetMimironFirefighterHazards(botAI);
    if (roundFire && hazards.flames.empty())
        return false;

    MimironMarkers const markers = GetMimironMarkers(botAI);
    Unit* vx001 = GetUldUnitByEntry(botAI, NPC_VX001);
    MimironRapidBurstWindow const burst =
        vx001 ? GetMimironRapidBurstWindow(botAI, bot, vx001) : MimironRapidBurstWindow();
    auto const standable = [&](Position const& spot)
    {
        return IsMimironSpotStandable(bot, spot, markers, hazards) &&
               IsMimironSpotRapidBurstSafe(vx001, burst, spot) &&
               (roundFire || IsMimironLegMineSafe(bot->GetPosition(), markers.mines, spot));
    };

    float const stop = range + bot->GetCombatReach() + target->GetCombatReach();
    float const radius = std::max(1.0f, stop - ULDUAR_MIMIRON_APPROACH_INSET);

    float const towardBot = target->GetAngle(bot->GetPositionX(), bot->GetPositionY());
    for (uint32 step = 0; step < ULDUAR_MIMIRON_STAND_IN_BEARINGS; ++step)
    {
        float const turn = static_cast<float>((step + 1) / 2) * ULDUAR_MIMIRON_STAND_IN_BEARING_STEP;
        float const bearing = towardBot + (step % 2 ? turn : -turn);
        Position const candidate(target->GetPositionX() + radius * std::cos(bearing),
                                 target->GetPositionY() + radius * std::sin(bearing),
                                 bot->GetPositionZ());
        if (!standable(candidate))
            continue;

        if (roundFire && step == 0)
            return false;

        out = candidate;
        return true;
    }

    return false;
}
}

bool GetMimironTargetApproach(PlayerbotAI* botAI, Player* bot, Unit* target, float range,
                              Position& out)
{
    return FindMimironApproachSpot(botAI, bot, target, range, true, out);
}

bool GetMimironSightApproach(PlayerbotAI* botAI, Player* bot, Unit* target, float range,
                             Position& out)
{
    return FindMimironApproachSpot(botAI, bot, target, range, false, out);
}

std::vector<MimironApproach> GetMimironSlotApproaches(PlayerbotAI* botAI, Player* bot, Position const& slot,
                                                      MimironFirefighterHazards const& hazards)
{
    std::vector<MimironApproach> approaches;
    if (!botAI || !bot)
        return approaches;

    MimironMarkers const markers = GetMimironMarkers(botAI);
    Unit* vx001 = GetUldUnitByEntry(botAI, NPC_VX001);
    MimironRapidBurstWindow const burst =
        vx001 ? GetMimironRapidBurstWindow(botAI, bot, vx001) : MimironRapidBurstWindow();
    MimironBarrageWindow const barrage =
        vx001 ? GetMimironBarrageWindow(bot, vx001) : MimironBarrageWindow();

    float const speed = bot->GetSpeed(MOVE_RUN);
    auto const coneClear = [&](Position const& spot)
    {
        float const travel = speed > 0.0f
                                 ? bot->GetExactDist2d(spot.GetPositionX(), spot.GetPositionY()) / speed
                                 : 0.0f;
        return IsMimironSpotBarrageSafe(vx001, barrage, spot, travel);
    };

    auto const standable = [&](Position const& spot)
    {
        return IsMimironSpotStandable(bot, spot, markers, hazards) &&
               IsMimironSpotRapidBurstSafe(vx001, burst, spot) && coneClear(spot);
    };

    Position goal = slot;
    char const* how = "direct";
    if (!IsMimironSpotStandable(bot, slot, markers, hazards) || !coneClear(slot))
    {
        if (bot->GetExactDist2d(slot.GetPositionX(), slot.GetPositionY()) <=
                ULDUAR_MIMIRON_SLOT_SUBSTITUTE_RADIUS &&
            IsMimironSpotStandable(bot, bot->GetPosition(), markers, hazards))
            return approaches;

        if (!vx001 && GetUldUnitByEntry(botAI, NPC_LEVIATHAN_MKII) &&
            !GetUldUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT))
            return approaches;

        bool const meleeRing = vx001 && !PlayerbotAI::IsRanged(bot) && IsMimironPhase2(botAI);
        MimironBurstSector const sector =
            meleeRing ? GetMimironOtherGroupSector(botAI, bot, vx001) : MimironBurstSector();
        float const meleeReach =
            vx001 ? vx001->GetCombatReach() + ULDUAR_MIMIRON_PHASE2_MELEE_REACH_MARGIN : 0.0f;

        std::vector<Position> others;
        if (Group* group = meleeRing ? nullptr : bot->GetGroup())
        {
            for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
            {
                Player* member = ref->GetSource();
                if (member && member != bot && member->IsAlive() && member->IsInMap(bot))
                    others.push_back(member->GetPosition());
            }
        }

        float const towardBot = slot.GetAngle(bot->GetPositionX(), bot->GetPositionY());
        bool found = false;
        for (float radius = ULDUAR_MIMIRON_STAND_IN_RING_STEP;
             radius <= ULDUAR_MIMIRON_SLOT_SUBSTITUTE_RADIUS_WIDE + 0.01f && !found;
             radius += ULDUAR_MIMIRON_STAND_IN_RING_STEP)
        {
            for (uint32 step = 0; step < ULDUAR_MIMIRON_STAND_IN_BEARINGS && !found; ++step)
            {
                float const turn = static_cast<float>((step + 1) / 2) * ULDUAR_MIMIRON_STAND_IN_BEARING_STEP;
                float const bearing = towardBot + (step % 2 ? turn : -turn);
                Position const candidate(slot.GetPositionX() + radius * std::cos(bearing),
                                         slot.GetPositionY() + radius * std::sin(bearing),
                                         slot.GetPositionZ());
                if (!standable(candidate))
                    continue;

                if (meleeRing)
                {
                    float const reach = candidate.GetExactDist2d(vx001);
                    if (reach < ULDUAR_MIMIRON_BARRAGE_MELEE_RING_MIN || reach > meleeReach ||
                        IsMimironSpotInSector(vx001, sector, candidate))
                        continue;
                }

                bool crowded = false;
                for (Position const& other : others)
                {
                    if (candidate.GetExactDist2d(other.GetPositionX(), other.GetPositionY()) <
                        ULDUAR_MIMIRON_DISPERSE_DISTANCE)
                    {
                        crowded = true;
                        break;
                    }
                }

                if (!crowded)
                {
                    goal = candidate;
                    found = true;
                }
            }
        }

        if (!found)
        {
            if (RaidObs::Active())
                RaidObs::NoteDerived(bot, "mimiron.approach", "none");

            return approaches;
        }

        how = "substitute";
    }

    Position const from = bot->GetPosition();
    bool const directFireSafe = IsMimironLegFireSafe(from, hazards, goal);
    bool const directMineSafe = IsMimironLegMineSafe(from, markers.mines, goal);
    if (directFireSafe && directMineSafe)
    {
        approaches.push_back({goal, how, 0.0f});
        return approaches;
    }

    if (!directFireSafe && directMineSafe &&
        bot->GetHealthPct() >= ULDUAR_MIMIRON_FLAMES_CROSS_HEALTH_PCT)
    {
        MimironLegBurn const burn = MeasureMimironLegBurn(from, hazards, goal);
        if (burn.overlap <= 1 && burn.yards <= ULDUAR_MIMIRON_FLAMES_CROSS_MAX)
            approaches.push_back({goal, "cross", 0.0f, burn.yards});
    }

    auto const legSafe = [&](Position const& start, Position const& end)
    { return IsMimironLegFireSafe(start, hazards, end) && IsMimironLegMineSafe(start, markers.mines, end); };

    float const distance = from.GetExactDist2d(goal.GetPositionX(), goal.GetPositionY());
    float const bearing = from.GetAngle(goal.GetPositionX(), goal.GetPositionY());
    std::vector<std::pair<Position, float>> waypoints;
    for (float degrees : ULDUAR_MIMIRON_DETOUR_TURNS_DEG)
    {
        float const turn = degrees * static_cast<float>(M_PI) / 180.0f;
        float const leg = 0.5f * distance / std::cos(turn);
        for (float sign : {1.0f, -1.0f})
        {
            Position const waypoint(from.GetPositionX() + leg * std::cos(bearing + sign * turn),
                                    from.GetPositionY() + leg * std::sin(bearing + sign * turn),
                                    from.GetPositionZ());
            if (!standable(waypoint))
                continue;

            waypoints.emplace_back(waypoint, sign * turn);
            if (legSafe(from, waypoint) && legSafe(waypoint, goal))
                approaches.push_back({waypoint, "detour", sign * turn});
        }
    }

    if (directFireSafe)
    {
        if (bot->GetHealthPct() >= ULDUAR_MIMIRON_MINE_CROSS_HEALTH_PCT &&
            CountMimironLegHazards(from, markers.mines, ULDUAR_MIMIRON_MINE_LEG_CLEARANCE, goal) <= 1)
            approaches.push_back({goal, how, 0.0f, 0.0f, true});
        else if (approaches.empty() && RaidObs::Active())
            RaidObs::NoteDerived(bot, "mimiron.approach", "none mines");
    }

    if (!approaches.empty() || distance <= ULDUAR_MIMIRON_SLOT_STRANDED_DISTANCE ||
        bot->GetHealthPct() < ULDUAR_MIMIRON_FLAMES_CROSS_HEALTH_PCT || MimironPhase1Active(botAI))
        return approaches;

    std::optional<MimironApproach> leastBurn;
    if (directMineSafe)
        leastBurn = MimironApproach{goal, "stranded", 0.0f, MeasureMimironLegBurn(from, hazards, goal).yards};

    for (auto const& [waypoint, turn] : waypoints)
    {
        if (!IsMimironLegMineSafe(from, markers.mines, waypoint) ||
            !IsMimironLegMineSafe(waypoint, markers.mines, goal))
            continue;

        float const yards = MeasureMimironLegBurn(from, hazards, waypoint).yards +
                            MeasureMimironLegBurn(waypoint, hazards, goal).yards;
        if (!leastBurn || yards < leastBurn->burn)
            leastBurn = MimironApproach{waypoint, "stranded", turn, yards};
    }

    if (leastBurn)
        approaches.push_back(*leastBurn);

    return approaches;
}

bool IsMimironSpotBarrageSafe(Unit* vx001, MimironBarrageWindow const& window, Position const& dest,
                              float travelSeconds)
{
    if (!vx001 || !window.valid)
        return true;

    if (vx001->GetExactDist2d(dest.GetPositionX(), dest.GetPositionY()) < ULDUAR_MIMIRON_BARRAGE_BOUNDARY)
        return false;

    float const clearance = ULDUAR_MIMIRON_BARRAGE_HALF_ANGLE + ULDUAR_MIMIRON_BARRAGE_MARGIN;
    float const twoPi = 2.0f * static_cast<float>(M_PI);

    float const rate = window.untilLive > 0.0f ? 0.0f : window.rate;
    float const grown =
        std::min(window.sweep + rate * std::max(travelSeconds, 0.0f), twoPi - 2.0f * clearance);

    float const cw = Position::NormalizeOrientation(
        window.lead - vx001->GetAngle(dest.GetPositionX(), dest.GetPositionY()));

    return cw > grown + clearance && cw < twoPi - clearance;
}

std::string GetMimironBombBotSnare(Player* bot)
{
    if (!bot)
        return "";

    switch (bot->getClass())
    {
        case CLASS_HUNTER:  return "concussive shot";
        case CLASS_SHAMAN:  return "frost shock";
        case CLASS_WARLOCK: return "curse of exhaustion";
        default:            return "";
    }
}

Unit* GetMimironBombBotChasing(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot)
        return nullptr;

    for (ObjectGuid const& guid : botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get())
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || unit->GetEntry() != NPC_BOMB_BOT)
            continue;

        if (ServerFacade::instance().GetChaseTarget(unit) == bot)
            return unit;
    }

    return nullptr;
}

float GetMimironBombBotApproach(Player* bot, Unit* bombBot)
{
    if (!bot || !bombBot)
        return 0.0f;

    if (Unit* victim = ServerFacade::instance().GetChaseTarget(bombBot))
        return bombBot->GetExactDist2d(victim);

    return bombBot->GetExactDist2d(bot);
}

bool IsMimironAcuGrounded(PlayerbotAI* botAI)
{
    if (!botAI)
        return false;

    Unit* aerialCommandUnit = GetUldUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT);
    return aerialCommandUnit && aerialCommandUnit->HasAura(SPELL_MIMIRON_MAGNETIC_CORE_AURA);
}

bool IsMimironAcuAirborne(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot)
        return false;

    return GetUldUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT) && !IsMimironPhase4(bot) &&
           !IsMimironAcuGrounded(botAI);
}

Unit* GetMimironRingFocus(PlayerbotAI* botAI)
{
    if (!botAI)
        return nullptr;

    if (Unit* vx001 = GetUldUnitByEntry(botAI, NPC_VX001))
        return vx001;

    if (Unit* leviathanMkII = GetUldUnitByEntry(botAI, NPC_LEVIATHAN_MKII))
        return leviathanMkII;

    return GetUldUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT);
}

bool IsNearMimironRoom(Player* bot)
{
    return bot && bot->GetMapId() == ULDUAR_MAP_ID &&
           bot->GetExactDist2d(ULDUAR_MIMIRON_ROOM_CENTER) <= ULDUAR_MIMIRON_STAGING_SEARCH_RANGE;
}

Unit* GetMimironStagingFocus(Player* bot)
{
    if (!bot)
        return nullptr;

    Creature* vx001 = bot->FindNearestCreature(NPC_VX001, ULDUAR_MIMIRON_STAGING_SEARCH_RANGE);
    Creature* aerialCommandUnit =
        bot->FindNearestCreature(NPC_AERIAL_COMMAND_UNIT, ULDUAR_MIMIRON_STAGING_SEARCH_RANGE);

    if (vx001 && vx001->GetVehicleBase())
        return vx001;

    if (aerialCommandUnit)
        return aerialCommandUnit;

    return vx001;
}

namespace
{
enum MimironTracedPhase : uint32
{
    MIMIRON_TRACE_NONE = 0,
    MIMIRON_TRACE_MKII = 1,
    MIMIRON_TRACE_VX001 = 2,
    MIMIRON_TRACE_ACU = 3,
    MIMIRON_TRACE_ALL = 4,
    MIMIRON_TRACE_HANDOVER = 5,
};

struct MimironObsState
{
    RaidObs::ObsValue<uint32> phase{"mimiron.phase"};

    RaidObs::ObsValue<bool> acuGrounded{"mimiron.core"};

    RaidObs::ObsValue<ObjectGuid> coreCarrier{"mimiron.carrier"};

    RaidObs::ObsValue<ObjectGuid> unreachable{"mimiron.unreach"};

    uint32 scanMs = 0;
};

RaidInstanceState<MimironObsState> mimironObsStates;

MimironObsState& MimironObsStateFor(Player* bot) { return mimironObsStates.For(bot->GetInstanceId()); }

struct MimironFightState
{
    bool tankDragReady = false;
    uint32 tankHoldStartedMs = 0;

    uint8 stackAnchor = 0;
    uint32 stackPickedMs = 0;
    uint32 stackScanMs = 0;

    float campTurn = 0.0f;
    uint32 campTurnScanMs = 0;
    uint32 campTurnRaisedMs = 0;

    ObjectGuid plasmaClaimedBy;
    uint32 plasmaWindowMs = 0;

    std::unordered_set<ObjectGuid> coreCorpses;

    uint32 coreSpentMs = 0;
    ObjectGuid playerCoreCarrier;

    std::vector<ObjectGuid> keptFireBots;
    uint32 fireBotScanMs = 0;

    float wedgeBearing = 0.0f;
    uint32 wedgeAimedMs = 0;

    uint32 shiftFocusEntry = 0;
    float shiftX = 0.0f;
    float shiftY = 0.0f;

    Position tankSpot;
    uint32 tankSpotPickedMs = 0;
    uint32 tankSpotScanMs = 0;
};

RaidInstanceState<MimironFightState> mimironFightStates;

RaidInstanceState<std::unordered_map<ObjectGuid, uint32>> mimironFireDodges;

std::unordered_map<ObjectGuid, uint32>& MimironFireDodgesFor(Player* bot)
{
    return mimironFireDodges.For(bot->GetInstanceId());
}

MimironFightState& MimironFightStateFor(Player* bot) { return mimironFightStates.For(bot->GetInstanceId()); }

struct MimironHandoverState
{
    RaidObs::ObsValue<MimironHandoverStep> step{"mimiron.handover"};
    uint32 scanMs = 0;

    bool picked = false;
    float bearing = 0.0f;
};

RaidInstanceState<MimironHandoverState> mimironHandoverStates;

MimironHandoverState& MimironHandoverStateFor(Player* bot) { return mimironHandoverStates.For(bot->GetInstanceId()); }

MimironHandoverStep CachedMimironHandoverStep(Player* bot)
{
    MimironHandoverState& state = MimironHandoverStateFor(bot);
    if (!state.scanMs || GetMSTimeDiffToNow(state.scanMs) > ULDUAR_MIMIRON_HANDOVER_STALE_MS)
        return MimironHandoverStep::None;

    return state.step;
}

MimironHandoverStep ReadMimironHandoverStep(PlayerbotAI* botAI, Player* bot)
{
    if (!IsMimironHardModeActive(botAI) || !BossStateGateLive(botAI, ULD_BOSS_MIMIRON))
        return MimironHandoverStep::None;

    if (bot->FindNearestCreature(NPC_AERIAL_COMMAND_UNIT, ULDUAR_MIMIRON_STAGING_SEARCH_RANGE))
        return MimironHandoverStep::None;

    Creature* leviathanMkII = bot->FindNearestCreature(NPC_LEVIATHAN_MKII, ULDUAR_MIMIRON_STAGING_SEARCH_RANGE);
    if (!leviathanMkII || !leviathanMkII->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) ||
        leviathanMkII->GetHealthPct() >= ULDUAR_MIMIRON_SPENT_HEALTH_PCT)
        return MimironHandoverStep::None;

    Creature* vx001 = bot->FindNearestCreature(NPC_VX001, ULDUAR_MIMIRON_STAGING_SEARCH_RANGE);
    if (!vx001)
        return MimironHandoverStep::Hold;

    if (!vx001->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE) || vx001->GetVehicleBase() ||
        vx001->GetHealthPct() < ULDUAR_MIMIRON_SPENT_HEALTH_PCT)
        return MimironHandoverStep::None;

    if (Vehicle* kit = vx001->GetVehicleKit())
    {
        for (int8 seat : {0, 1})
        {
            Unit* passenger = kit->GetPassenger(seat);
            if (passenger && passenger->GetEntry() == NPC_MIMIRON)
                return MimironHandoverStep::WalkIn;
        }
    }

    return MimironHandoverStep::Late;
}

float PickMimironHoldBearing(PlayerbotAI* botAI, Player* bot)
{
    MimironFirefighterHazards const hazards = GetMimironFirefighterHazards(botAI);
    float const middle = ULDUAR_MIMIRON_HOLD_OUTER_RADIUS -
                         0.5f * ULDUAR_MIMIRON_HOLD_ROW_STEP * static_cast<float>(ULDUAR_MIMIRON_HOLD_ROWS - 1);

    float bestBearing = 0.0f;
    uint32 bestFire = std::numeric_limits<uint32>::max();
    float bestReach = 0.0f;
    for (uint32 i = 0; i < ULDUAR_MIMIRON_HOLD_BEARINGS; ++i)
    {
        float const bearing = 2.0f * static_cast<float>(M_PI) * static_cast<float>(i) /
                              static_cast<float>(ULDUAR_MIMIRON_HOLD_BEARINGS);
        Position const centre(ULDUAR_MIMIRON_ROOM_CENTER.GetPositionX() + middle * std::cos(bearing),
                              ULDUAR_MIMIRON_ROOM_CENTER.GetPositionY() + middle * std::sin(bearing),
                              ULDUAR_MIMIRON_ROOM_CENTER.GetPositionZ());

        uint32 fire = 0;
        for (Position const& flame : hazards.flames)
            if (centre.GetExactDist2d(flame.GetPositionX(), flame.GetPositionY()) <= ULDUAR_MIMIRON_HOLD_CLEAR_RADIUS)
                ++fire;

        float const reach = centre.GetExactDist2d(ULDUAR_MIMIRON_PHASE1_TANK_SPOT);
        if (fire < bestFire || (fire == bestFire && reach < bestReach))
        {
            bestBearing = bearing;
            bestFire = fire;
            bestReach = reach;
        }
    }

    if (RaidObs::Active())
    {
        char line[32];
        snprintf(line, sizeof(line), "%.0f deg %u fire", bestBearing * 180.0f / static_cast<float>(M_PI), bestFire);
        RaidObs::NoteDerived(bot, "mimiron.hold", line);
    }

    return bestBearing;
}

void UpdateMimironHandover(PlayerbotAI* botAI, Player* bot)
{
    MimironHandoverState& state = MimironHandoverStateFor(bot);
    state.scanMs = getMSTime();

    MimironHandoverStep const step = ReadMimironHandoverStep(botAI, bot);
    state.step = step;
    if (step == MimironHandoverStep::None)
    {
        state.picked = false;
        return;
    }

    if (!state.picked)
    {
        state.bearing = PickMimironHoldBearing(botAI, bot);
        state.picked = true;
    }
}

void ResetMimironHandover(Player* bot)
{
    MimironHandoverState& state = MimironHandoverStateFor(bot);
    state.step = MimironHandoverStep::None;
    state.picked = false;
}

void TickMimironObs(PlayerbotAI* botAI, Player* bot)
{
    if (!bot || bot->GetMapId() != ULDUAR_MAP_ID)
        return;

    MimironObsState& state = MimironObsStateFor(bot);
    if (state.scanMs && GetMSTimeDiffToNow(state.scanMs) < ULDUAR_MIMIRON_OBS_SCAN_INTERVAL_MS)
        return;

    state.scanMs = getMSTime();

    Unit* leviathanMkII = GetUldUnitByEntry(botAI, NPC_LEVIATHAN_MKII);
    Unit* vx001 = GetUldUnitByEntry(botAI, NPC_VX001);
    Unit* aerialCommandUnit = GetUldUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT);

    if (!leviathanMkII && !vx001 && !aerialCommandUnit)
    {
        state.phase = GetMimironStagingFocus(bot) ? MIMIRON_TRACE_HANDOVER : MIMIRON_TRACE_NONE;
        state.acuGrounded = false;
        state.coreCarrier = ObjectGuid::Empty;
        state.unreachable = ObjectGuid::Empty;
        ResetMimironFightState(bot);
        UpdateMimironHandover(botAI, bot);
        return;
    }

    ResetMimironHandover(bot);

    if (leviathanMkII && vx001 && aerialCommandUnit)
        state.phase = MIMIRON_TRACE_ALL;
    else if (aerialCommandUnit)
        state.phase = MIMIRON_TRACE_ACU;
    else if (vx001)
        state.phase = MIMIRON_TRACE_VX001;
    else
        state.phase = MIMIRON_TRACE_MKII;

    state.acuGrounded = IsMimironAcuGrounded(botAI);

    if (!RaidObs::Active())
        return;

    Player* carrier = GetMimironCoreCarrier(botAI);
    state.coreCarrier = carrier ? carrier->GetGUID() : ObjectGuid::Empty;

    Creature* chaser = leviathanMkII ? leviathanMkII->ToCreature() : nullptr;
    state.unreachable = chaser ? chaser->GetCannotReachTarget() : ObjectGuid::Empty;

    if (!vx001)
        return;

    MimironBarrageWindow const window = GetMimironBarrageWindow(bot, vx001);
    if (!window.valid)
        return;

    char params[96];
    snprintf(params, sizeof(params), "\"lead\":%.2f,\"sweep\":%.2f,\"rate\":%.2f,\"live\":%.1f",
             window.lead, window.sweep, window.rate, window.untilLive);

    RaidObs::NoteHazard(bot->GetMap(),
                        window.untilLive > 0.0f ? SPELL_SPINNING_UP : SPELL_P3WX2_LASER_BARRAGE_AURA_1,
                        vx001->GetPosition(), "sweep", params, ULDUAR_MIMIRON_OBS_SCAN_INTERVAL_MS);
}
}

void MimironTick(PlayerbotAI* botAI) { TickMimironObs(botAI, botAI->GetBot()); }

MimironHandoverStep GetMimironHandoverStep(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot || !IsNearMimironRoom(bot) || GetMimironRingFocus(botAI))
        return MimironHandoverStep::None;

    return CachedMimironHandoverStep(bot);
}

bool IsMimironEngaged(PlayerbotAI* botAI)
{
    Unit* leviathanMkII = GetUldUnitByEntry(botAI, NPC_LEVIATHAN_MKII);
    Unit* vx001 = GetUldUnitByEntry(botAI, NPC_VX001);
    Unit* aerialCommandUnit = GetUldUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT);

    for (Unit* construct : {leviathanMkII, vx001, aerialCommandUnit})
        if (construct && construct->IsInCombat())
            return true;

    return false;
}

bool IsMimironPhase4(Player* bot)
{
    PlayerbotAI* botAI = bot ? GET_PLAYERBOT_AI(bot) : nullptr;
    if (!botAI)
        return false;

    if (Unit* vx001 = GetUldUnitByEntry(botAI, NPC_VX001))
        return vx001->GetVehicleBase() != nullptr;

    if (Unit* aerialCommandUnit = GetUldUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT))
        return aerialCommandUnit->GetVehicleBase() != nullptr;

    if (Unit* leviathanMkII = GetUldUnitByEntry(botAI, NPC_LEVIATHAN_MKII))
        if (Vehicle* kit = leviathanMkII->GetVehicleKit())
            if (Unit* seated = kit->GetPassenger(3))
                return seated->GetEntry() == NPC_VX001;

    return false;
}

bool MimironPhase1Active(PlayerbotAI* botAI)
{
    return !IsMimironPhase4(botAI->GetBot()) && GetUldUnitByEntry(botAI, NPC_LEVIATHAN_MKII) &&
           !GetUldUnitByEntry(botAI, NPC_VX001) && !GetUldUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT);
}

bool IsMimironPhase2(PlayerbotAI* botAI)
{
    if (!botAI || GetUldUnitByEntry(botAI, NPC_LEVIATHAN_MKII))
        return false;

    Unit* vx001 = GetUldUnitByEntry(botAI, NPC_VX001);
    return vx001 && !vx001->GetVehicleBase();
}

bool IsMimironPhase3(PlayerbotAI* botAI)
{
    return botAI && GetUldUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT) &&
           !GetUldUnitByEntry(botAI, NPC_LEVIATHAN_MKII) && !GetUldUnitByEntry(botAI, NPC_VX001);
}

Player* GetMimironFrostResistancePaladin(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot || !IsMimironHardModeActive(botAI) || !IsMimironPhase3(botAI))
        return nullptr;

    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    Player* fire = nullptr;
    Player* firstAlive = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member->getClass() != CLASS_PALADIN)
            continue;

        if (!firstAlive && member->IsAlive())
            firstAlive = member;

        PlayerbotAI* memberAI = GET_PLAYERBOT_AI(member);
        if (!fire && memberAI && memberAI->HasStrategy("rfire", BotState::BOT_STATE_COMBAT))
            fire = member;
    }

    if (!fire)
        fire = firstAlive;

    static uint32 const ranks[] = {SPELL_FROST_RESISTANCE_AURA_RANK_5, SPELL_FROST_RESISTANCE_AURA_RANK_4,
                                   SPELL_FROST_RESISTANCE_AURA_RANK_3, SPELL_FROST_RESISTANCE_AURA_RANK_2,
                                   SPELL_FROST_RESISTANCE_AURA_RANK_1};

    Player* tank = nullptr;
    Player* dps = nullptr;
    Player* healer = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == fire || !member->IsAlive() || member->getClass() != CLASS_PALADIN ||
            member->GetMapId() != bot->GetMapId() || !GET_PLAYERBOT_AI(member))
            continue;

        bool knows = false;
        for (uint32 rank : ranks)
            if (member->HasActiveSpell(rank))
            {
                knows = true;
                break;
            }

        if (!knows)
            continue;

        if (PlayerbotAI::IsTank(member))
        {
            if (!tank)
                tank = member;
        }
        else if (PlayerbotAI::IsHeal(member))
        {
            if (!healer)
                healer = member;
        }
        else if (!dps)
        {
            dps = member;
        }
    }

    if (tank)
        return tank;

    return dps ? dps : healer;
}

Unit* GetMimironPhase4Focus(PlayerbotAI* botAI, Player* bot, bool melee)
{
    if (!botAI || !bot || !IsMimironPhase4(bot))
        return nullptr;

    std::vector<Unit*> parts;
    for (uint32 entry : {NPC_LEVIATHAN_MKII, NPC_VX001, NPC_AERIAL_COMMAND_UNIT})
        if (Unit* part = GetUldUnitByEntry(botAI, entry))
            parts.push_back(part);

    if (parts.empty())
        return nullptr;

    auto const highest = [](std::vector<Unit*> const& candidates) -> Unit*
    {
        auto const band = [](Unit* unit)
        { return static_cast<int32>(unit->GetHealthPct() / ULDUAR_MIMIRON_PHASE4_FOCUS_BAND_PCT); };

        Unit* best = nullptr;
        for (Unit* candidate : candidates)
            if (!best || band(candidate) > band(best))
                best = candidate;

        return best;
    };

    if (parts.size() < 3)
        return highest(parts);

    std::vector<Unit*> allowed;
    for (Unit* part : parts)
    {
        if (part->GetEntry() == NPC_AERIAL_COMMAND_UNIT && (melee || !PlayerbotAI::IsRangedDps(bot)))
            continue;

        allowed.push_back(part);
    }

    if (allowed.empty())
        return nullptr;

    bool levelled = true;
    for (Unit* part : parts)
        if (part->GetHealthPct() > ULDUAR_MIMIRON_PHASE4_HOLD_PCT)
            levelled = false;

    if (levelled)
        return highest(allowed);

    float leader = 0.0f;
    for (Unit* part : parts)
        leader = std::max(leader, part->GetHealthPct());

    float const floorPct =
        std::max(ULDUAR_MIMIRON_PHASE4_HOLD_PCT, leader - ULDUAR_MIMIRON_PHASE4_CONVERGE_PCT);

    std::vector<Unit*> aboveFloor;
    for (Unit* part : allowed)
        if (part->GetHealthPct() > floorPct)
            aboveFloor.push_back(part);

    if (!aboveFloor.empty())
        return highest(aboveFloor);

    if (melee)
    {
        Unit* aerialCommandUnit = nullptr;
        float pairLow = 100.0f;
        for (Unit* part : parts)
        {
            if (part->GetEntry() == NPC_AERIAL_COMMAND_UNIT)
                aerialCommandUnit = part;
            else
                pairLow = std::min(pairLow, part->GetHealthPct());
        }

        if (aerialCommandUnit && aerialCommandUnit->GetHealthPct() > floorPct &&
            pairLow >= leader - 2.0f * ULDUAR_MIMIRON_PHASE4_CONVERGE_PCT)
            return aerialCommandUnit;
    }

    return nullptr;
}

Unit* GetMimironPhase4TankFocus(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot || !IsMimironPhase4(bot))
        return nullptr;

    if (Unit* leviathanMkII = GetUldUnitByEntry(botAI, NPC_LEVIATHAN_MKII))
        return leviathanMkII;

    return GetMimironPhase4Focus(botAI, bot, true);
}

Spell* GetMimironPlasmaBlastCast(Unit* cannon)
{
    if (!cannon)
        return nullptr;

    if (Spell* spell = cannon->FindCurrentSpellBySpellId(SPELL_MIMIRON_PLASMA_BLAST))
        return spell;

    return cannon->FindCurrentSpellBySpellId(SPELL_MIMIRON_PLASMA_BLAST_25);
}

bool IsMimironTankAnchorSlot(PlayerbotAI* botAI, Player* bot)
{
    return botAI && bot && PlayerbotAI::IsMainTank(bot) &&
           GetUldUnitByEntry(botAI, NPC_LEVIATHAN_MKII) != nullptr;
}

float GetMimironPhase1DisperseDistance(PlayerbotAI* /*botAI*/)
{
    return ULDUAR_MIMIRON_DISPERSE_DISTANCE;
}

void ResetMimironFightState(Player* bot)
{
    if (!bot)
        return;

    MimironFightState& state = MimironFightStateFor(bot);
    state = MimironFightState();
}

void NoteMimironFireDodge(Player* bot)
{
    if (!bot)
        return;

    MimironFireDodgesFor(bot)[bot->GetGUID()] = getMSTime();
}

bool IsMimironFireHoldActive(Player* bot)
{
    if (!bot)
        return false;

    std::unordered_map<ObjectGuid, uint32> const& dodges = MimironFireDodgesFor(bot);
    auto const it = dodges.find(bot->GetGUID());
    return it != dodges.end() && GetMSTimeDiffToNow(it->second) < ULDUAR_MIMIRON_FLAMES_HOLD_MS;
}

bool IsMimironNearFrostBomb(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!IsMimironHardModeActive(botAI) || !IsNearMimironRoom(bot))
        return false;

    float const hold = ULDUAR_MIMIRON_FROST_BOMB_CLEARANCE + ULDUAR_MIMIRON_FROST_BOMB_HOLD_MARGIN;
    Creature* bomb = bot->FindNearestCreature(NPC_FROST_BOMB, hold + ULDUAR_MIMIRON_HOLD_SEARCH_PAD);
    return bomb && bot->GetExactDist2d(bomb) < hold;
}

bool IsMimironNearRocket(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!IsNearMimironRoom(bot))
        return false;

    float const hold = ULDUAR_MIMIRON_ROCKET_CLEARANCE + ULDUAR_MIMIRON_ROCKET_HOLD_MARGIN;
    Creature* rocket = bot->FindNearestCreature(NPC_ROCKET_STRIKE_N, hold + ULDUAR_MIMIRON_HOLD_SEARCH_PAD);
    return rocket && bot->GetExactDist2d(rocket) < hold;
}

bool IsMimironNearBombBotBlast(PlayerbotAI* botAI)
{
    MimironBombBotBlast const blast = GetMimironBombBotBlast(
        botAI, botAI->GetBot(), ULDUAR_MIMIRON_BOMB_BOT_BLAST_CLEARANCE + ULDUAR_MIMIRON_BOMB_BOT_HOLD_MARGIN);
    return blast.valid && !blast.chased;
}

bool IsMimironHeldByFire(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!IsMimironHardModeActive(botAI) || !IsNearMimironRoom(bot))
        return false;

    return IsMimironFireHoldActive(bot) ||
           IsMimironSpotInFireBotSpray(GetMimironFirefighterHazards(botAI), bot->GetPosition(),
                                       ULDUAR_MIMIRON_FIREBOT_SPRAY_HALF_WIDTH);
}

bool ClaimMimironPlasmaWindow(Player* bot)
{
    if (!bot)
        return false;

    MimironFightState& state = MimironFightStateFor(bot);
    if (state.plasmaWindowMs &&
        GetMSTimeDiffToNow(state.plasmaWindowMs) < ULDUAR_MIMIRON_PLASMA_WINDOW_MS)
        return state.plasmaClaimedBy == bot->GetGUID();

    state.plasmaWindowMs = getMSTime();
    state.plasmaClaimedBy = bot->GetGUID();
    return true;
}

bool IsMimironTankDragReady(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot)
        return false;

    MimironFightState& state = MimironFightStateFor(bot);
    if (state.tankDragReady)
        return true;

    Unit* leviathanMkII = GetUldUnitByEntry(botAI, NPC_LEVIATHAN_MKII);
    if (!leviathanMkII)
        return false;

    if (!state.tankHoldStartedMs)
        state.tankHoldStartedMs = getMSTime();
    else if (GetMSTimeDiffToNow(state.tankHoldStartedMs) >= ULDUAR_MIMIRON_TANK_HOLD_MAX_MS)
    {
        state.tankDragReady = true;
        return true;
    }

    if (leviathanMkII->GetVictim() != bot)
        return false;

    ThreatManager& mgr = leviathanMkII->GetThreatMgr();
    float const tankThreat = mgr.GetThreat(bot);
    if (tankThreat <= 0.0f)
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    float highest = 0.0f;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive() || PlayerbotAI::IsTank(member))
            continue;

        highest = std::max(highest, mgr.GetThreat(member));
    }

    if (tankThreat < highest * ULDUAR_MIMIRON_TANK_THREAT_LEAD)
        return false;

    state.tankDragReady = true;
    return true;
}

Position const& GetMimironPhase1StackAnchor(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot)
        return ULDUAR_MIMIRON_PHASE1_STACK_SPOTS[0];

    MimironFightState& state = MimironFightStateFor(bot);

    if (state.stackScanMs &&
        GetMSTimeDiffToNow(state.stackScanMs) < ULDUAR_MIMIRON_OBS_SCAN_INTERVAL_MS)
        return ULDUAR_MIMIRON_PHASE1_STACK_SPOTS[state.stackAnchor];

    state.stackScanMs = getMSTime();

    if (state.stackPickedMs && GetMSTimeDiffToNow(state.stackPickedMs) < ULDUAR_MIMIRON_STACK_HOLD_MS)
        return ULDUAR_MIMIRON_PHASE1_STACK_SPOTS[state.stackAnchor];

    MimironFirefighterHazards const hazards = GetMimironFirefighterHazards(botAI);

    uint32 counts[ULDUAR_MIMIRON_PHASE1_STACK_COUNT] = {};
    for (uint8 i = 0; i < ULDUAR_MIMIRON_PHASE1_STACK_COUNT; ++i)
    {
        Position const& spot = ULDUAR_MIMIRON_PHASE1_STACK_SPOTS[i];
        for (Position const& flame : hazards.flames)
        {
            if (spot.GetExactDist2d(flame.GetPositionX(), flame.GetPositionY()) <=
                ULDUAR_MIMIRON_STACK_FIRE_RADIUS)
                ++counts[i];
        }
    }

    uint8 const live = state.stackAnchor;
    if (counts[live] <= ULDUAR_MIMIRON_STACK_FIRE_LIMIT)
        return ULDUAR_MIMIRON_PHASE1_STACK_SPOTS[live];

    uint8 best = live;
    for (uint8 i = 0; i < ULDUAR_MIMIRON_PHASE1_STACK_COUNT; ++i)
        if (counts[i] + ULDUAR_MIMIRON_STACK_FIRE_MARGIN <= counts[best])
            best = i;

    if (best == live)
        return ULDUAR_MIMIRON_PHASE1_STACK_SPOTS[live];

    state.stackAnchor = best;
    state.stackPickedMs = getMSTime();

    if (RaidObs::Active())
    {
        char line[48];
        snprintf(line, sizeof(line), "%u:%u -> %u:%u", uint32(live), counts[live], uint32(best),
                 counts[best]);
        RaidObs::NoteDerived(bot, "mimiron.stack", line);
    }

    return ULDUAR_MIMIRON_PHASE1_STACK_SPOTS[best];
}

namespace
{
MimironRapidBurstWindow ReadMimironRapidBurstWindow(PlayerbotAI* botAI, Player* bot, Unit* vx001)
{
    MimironRapidBurstWindow window;
    if (!botAI || !bot || !vx001 || !vx001->IsAlive() || vx001->GetVehicleBase())
        return window;

    std::list<Creature*> aims;
    vx001->GetCreatureListWithEntryInGrid(aims, NPC_BURST_TARGET, ULDUAR_MIMIRON_ROOM_SEARCH_RANGE);
    for (Creature* aim : aims)
    {
        if (aim && aim->HasAura(SPELL_MIMIRON_RAPID_BURST, vx001->GetGUID()))
        {
            window.valid = true;
            window.centreline = vx001->GetAngle(aim->GetPositionX(), aim->GetPositionY());
            return window;
        }
    }

    if (vx001->HasAura(SPELL_MIMIRON_RAPID_BURST))
    {
        window.valid = true;
        window.centreline = vx001->GetOrientation();
    }

    return window;
}
}

MimironRapidBurstWindow GetMimironRapidBurstWindow(PlayerbotAI* botAI, Player* bot, Unit* vx001)
{
    MimironPassReads* pass = MimironPassReadsFor(botAI);
    if (!pass)
        return ReadMimironRapidBurstWindow(botAI, bot, vx001);

    if (!pass->burstRead)
    {
        pass->burst = ReadMimironRapidBurstWindow(botAI, bot, vx001);
        pass->burstRead = true;
    }

    return pass->burst;
}

bool IsMimironSpotRapidBurstSafe(Unit* vx001, MimironRapidBurstWindow const& window,
                                 Position const& dest)
{
    if (!vx001 || !window.valid)
        return true;

    float const bearing = vx001->GetAngle(dest.GetPositionX(), dest.GetPositionY());
    float off = Position::NormalizeOrientation(bearing - window.centreline);
    if (off > static_cast<float>(M_PI))
        off -= 2.0f * static_cast<float>(M_PI);

    return std::fabs(off) > ULDUAR_MIMIRON_RAPID_BURST_HALF_ANGLE;
}

Player* GetMimironCoreCarrier(PlayerbotAI* botAI)
{
    if (!botAI)
        return nullptr;

    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    if (!group)
        return nullptr;

    MimironFightState& state = MimironFightStateFor(bot);

    Player* melee = nullptr;
    Player* fallback = nullptr;
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || !member->IsInMap(bot))
            continue;

        if (IsRealPlayer(member))
        {
            if (member->GetGUID() == state.playerCoreCarrier ||
                member->HasItemCount(ITEM_MIMIRON_MAGNETIC_CORE, 1, false))
            {
                state.playerCoreCarrier = member->GetGUID();
                return member;
            }

            continue;
        }

        if (!fallback)
            fallback = member;

        if (!melee && !PlayerbotAI::IsRanged(member) && !PlayerbotAI::IsTank(member))
            melee = member;
    }

    return melee ? melee : fallback;
}

namespace
{
enum : int32
{
    MIMIRON_CORE_NOT_IN_LOOT = -1,
    MIMIRON_CORE_ALREADY_TAKEN = -2
};

int32 MimironCoreLootSlot(Creature* corpse)
{
    int32 result = MIMIRON_CORE_NOT_IN_LOOT;
    for (size_t i = 0; i < corpse->loot.items.size(); ++i)
    {
        LootItem const& item = corpse->loot.items[i];
        if (item.itemid != ITEM_MIMIRON_MAGNETIC_CORE)
            continue;

        if (!item.is_looted)
            return static_cast<int32>(i);

        result = MIMIRON_CORE_ALREADY_TAKEN;
    }

    return result;
}
}

Creature* GetMimironCoreCorpse(Player* bot)
{
    if (!bot)
        return nullptr;

    MimironFightState& state = MimironFightStateFor(bot);

    std::list<Creature*> assaultBots;
    bot->GetCreatureListWithEntryInGrid(assaultBots, NPC_ASSAULT_BOT, ULDUAR_MIMIRON_CORE_SEARCH_RANGE);

    Creature* nearest = nullptr;
    float nearestDist = 0.0f;
    for (Creature* corpse : assaultBots)
    {
        if (!corpse || corpse->IsAlive() || state.coreCorpses.count(corpse->GetGUID()) ||
            MimironCoreLootSlot(corpse) == MIMIRON_CORE_ALREADY_TAKEN)
            continue;

        float const dist = bot->GetExactDist2d(corpse);
        if (!nearest || dist < nearestDist)
        {
            nearest = corpse;
            nearestDist = dist;
        }
    }

    return nearest;
}

bool IsMimironCoreFetchDue(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot || GetMimironCoreCarrier(botAI) != bot)
        return false;

    return bot->GetItemCount(ITEM_MIMIRON_MAGNETIC_CORE, false) < ULDUAR_MIMIRON_CORE_BANK &&
           GetMimironCoreCorpse(bot);
}

bool TakeMimironCore(Player* bot, Creature* corpse)
{
    if (!bot || !corpse)
        return false;

    ItemPosCountVec dest;
    if (bot->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, ITEM_MIMIRON_MAGNETIC_CORE, 1) != EQUIP_ERR_OK)
        return false;

    MimironFightStateFor(bot).coreCorpses.insert(corpse->GetGUID());

    int32 const slot = MimironCoreLootSlot(corpse);
    if (slot >= 0)
    {
        Loot& loot = corpse->loot;
        loot.items[slot].is_looted = true;
        --loot.unlootedCount;
        loot.NotifyItemRemoved(static_cast<uint8>(slot));

        if (loot.isLooted())
        {
            corpse->AllLootRemovedFromCorpse();
            corpse->RemoveDynamicFlag(UNIT_DYNFLAG_LOOTABLE);
            loot.clear();
        }
    }

    bot->StoreNewItem(dest, ITEM_MIMIRON_MAGNETIC_CORE, true,
                      Item::GenerateItemRandomPropertyId(ITEM_MIMIRON_MAGNETIC_CORE));
    return true;
}

bool IsMimironCoreUseReady(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot)
        return false;

    Unit* aerialCommandUnit = GetUldUnitByEntry(botAI, NPC_AERIAL_COMMAND_UNIT);
    if (!aerialCommandUnit || IsMimironAcuGrounded(botAI) ||
        !aerialCommandUnit->HasUnitMovementFlag(MOVEMENTFLAG_HOVER))
        return false;

    MimironFightState const& state = MimironFightStateFor(bot);
    if (state.coreSpentMs && GetMSTimeDiffToNow(state.coreSpentMs) < ULDUAR_MIMIRON_CORE_ARM_MS)
        return false;

    return !bot->FindNearestCreature(NPC_MAGNETIC_CORE, ULDUAR_MIMIRON_CORE_PENDING_RANGE);
}

void NoteMimironCoreSpent(Player* bot)
{
    if (bot)
        MimironFightStateFor(bot).coreSpentMs = getMSTime();
}

std::vector<ObjectGuid> GetMimironKeptFireBots(PlayerbotAI* botAI, Player* bot)
{
    if (!botAI || !bot || !IsMimironHardModeActive(botAI) || !IsNearMimironRoom(bot) ||
        !BossStateGateLive(botAI, ULD_BOSS_MIMIRON))
        return {};

    MimironFightState& state = MimironFightStateFor(bot);
    if (!state.fireBotScanMs ||
        GetMSTimeDiffToNow(state.fireBotScanMs) >= ULDUAR_MIMIRON_OBS_SCAN_INTERVAL_MS)
    {
        state.fireBotScanMs = getMSTime();

        std::list<Creature*> fireBots;
        bot->GetCreatureListWithEntryInGrid(fireBots, NPC_EMERGENCY_FIRE_BOT,
                                            ULDUAR_MIMIRON_STAGING_SEARCH_RANGE);

        std::vector<ObjectGuid> alive;
        for (Creature* fireBot : fireBots)
            if (fireBot && fireBot->IsAlive())
                alive.push_back(fireBot->GetGUID());

        std::sort(alive.begin(), alive.end());
        state.keptFireBots = alive;
    }

    return state.keptFireBots;
}

bool IsMimironFireBotProtected(PlayerbotAI* botAI, Player* bot, Unit* fireBot)
{
    if (!fireBot || fireBot->GetEntry() != NPC_EMERGENCY_FIRE_BOT)
        return false;

    std::vector<ObjectGuid> const kept = GetMimironKeptFireBots(botAI, bot);
    return std::find(kept.begin(), kept.end(), fireBot->GetGUID()) != kept.end();
}

namespace
{
Position ClampMimironAnchorToRoom(Position anchor)
{
    float const z = ULDUAR_MIMIRON_ROOM_CENTER.GetPositionZ();

    float const fromCentre =
        ULDUAR_MIMIRON_ROOM_CENTER.GetExactDist2d(anchor.GetPositionX(), anchor.GetPositionY());
    if (fromCentre > ULDUAR_MIMIRON_ROOM_RADIUS)
    {
        float const bearing =
            ULDUAR_MIMIRON_ROOM_CENTER.GetAngle(anchor.GetPositionX(), anchor.GetPositionY());
        anchor = Position(
            ULDUAR_MIMIRON_ROOM_CENTER.GetPositionX() + ULDUAR_MIMIRON_ROOM_RADIUS * cos(bearing),
            ULDUAR_MIMIRON_ROOM_CENTER.GetPositionY() + ULDUAR_MIMIRON_ROOM_RADIUS * sin(bearing), z);
    }

    return anchor;
}

uint32 MimironWedgeRows(float firstRow, uint32 maxRows, uint32 count, float halfAngle, float spacing)
{
    uint32 held = 0;
    for (uint32 rows = 1; rows <= maxRows; ++rows)
    {
        float const radius = firstRow + (rows - 1) * spacing;
        held += static_cast<uint32>(2.0f * halfAngle * radius / spacing);
        if (held >= count)
            return rows;
    }

    return maxRows;
}

void MimironWedgeSlot(float firstRow, uint32 rows, uint32 index, uint32 count, float halfAngle,
                      float spacing, float& outRadius, float& outOffset)
{
    uint32 const base = count / rows;
    uint32 const extra = count % rows;

    uint32 row = 0;
    uint32 filled = 0;
    for (; row + 1 < rows; ++row)
    {
        uint32 const size = base + (row < extra ? 1 : 0);
        if (index < filled + size)
            break;

        filled += size;
    }

    uint32 const size = base + (row < extra ? 1 : 0);
    uint32 const slot = index - filled;

    outRadius = firstRow + row * spacing;
    outOffset = size <= 1 ? 0.0f : -halfAngle + 2.0f * halfAngle * slot / (size - 1);
}

float MimironWedgeReach(Unit* focus)
{
    return std::max(sPlayerbotAIConfig.SpellDistance + focus->GetCombatReach() + DEFAULT_COMBAT_REACH -
                        ULDUAR_MIMIRON_SPREAD_RANGE_MARGIN,
                    1.0f);
}

float MimironSlotExcess(Position const& slot, Unit* focus, float reach, float shiftX, float shiftY)
{
    float const dz = focus->GetPositionZ() - slot.GetPositionZ();
    float const floor = std::sqrt(std::max(reach * reach - dz * dz, 0.0f));
    return focus->GetExactDist2d(slot.GetPositionX() + shiftX, slot.GetPositionY() + shiftY) - floor;
}

void MimironShiftIntoRange(std::vector<Position> const& slots, Unit* focus, float& shiftX, float& shiftY,
                           float inset = 0.0f)
{
    shiftX = 0.0f;
    shiftY = 0.0f;
    if (!focus)
        return;

    float const reach = std::max(MimironWedgeReach(focus) - inset, 1.0f);

    for (uint32 pass = 0; pass < 3; ++pass)
    {
        float worst = 0.0f;
        float worstX = 0.0f;
        float worstY = 0.0f;
        for (Position const& slot : slots)
        {
            float const excess = MimironSlotExcess(slot, focus, reach, shiftX, shiftY);
            if (excess > worst)
            {
                worst = excess;
                worstX = slot.GetPositionX() + shiftX;
                worstY = slot.GetPositionY() + shiftY;
            }
        }

        if (worst <= 0.0f)
            break;

        float const bearing = std::atan2(focus->GetPositionY() - worstY, focus->GetPositionX() - worstX);
        shiftX += worst * std::cos(bearing);
        shiftY += worst * std::sin(bearing);
    }
}

void MimironStickyShiftIntoRange(Player* bot, std::vector<Position> const& slots, Unit* focus, float& shiftX,
                                 float& shiftY)
{
    MimironShiftIntoRange(slots, focus, shiftX, shiftY, ULDUAR_MIMIRON_SHIFT_SLACK);
    if (!bot || !focus)
        return;

    MimironFightState& state = MimironFightStateFor(bot);
    if (state.shiftFocusEntry == focus->GetEntry())
    {
        float const reach = MimironWedgeReach(focus);
        bool const inReach = std::all_of(slots.begin(), slots.end(),
                                         [&](Position const& slot)
                                         {
                                             return MimironSlotExcess(slot, focus, reach, state.shiftX,
                                                                      state.shiftY) <= 0.0f;
                                         });
        if (inReach && std::hypot(state.shiftX - shiftX, state.shiftY - shiftY) <= ULDUAR_MIMIRON_SHIFT_SLACK)
        {
            shiftX = state.shiftX;
            shiftY = state.shiftY;
            return;
        }
    }

    state.shiftFocusEntry = focus->GetEntry();
    state.shiftX = shiftX;
    state.shiftY = shiftY;
}

std::vector<Position> MimironPhase1CampSlots(Position const& anchor, Unit* focus, uint32 count, float turn)
{
    Position const& hub = ULDUAR_MIMIRON_PHASE1_TANK_SPOT;
    float centreline = hub.GetAngle(anchor.GetPositionX(), anchor.GetPositionY());

    float toRoom = Position::NormalizeOrientation(
        hub.GetAngle(ULDUAR_MIMIRON_ROOM_CENTER.GetPositionX(), ULDUAR_MIMIRON_ROOM_CENTER.GetPositionY()) -
        centreline);
    if (toRoom > static_cast<float>(M_PI))
        toRoom -= 2.0f * static_cast<float>(M_PI);
    centreline += (toRoom >= 0.0f ? 1.0f : -1.0f) * std::min(turn, std::fabs(toRoom));

    uint32 const rows =
        MimironWedgeRows(ULDUAR_MIMIRON_PHASE1_CAMP_FIRST_ROW, ULDUAR_MIMIRON_PHASE1_CAMP_ROWS, count,
                         ULDUAR_MIMIRON_PHASE1_CAMP_HALF_ANGLE, ULDUAR_MIMIRON_PHASE1_CAMP_SPACING);

    std::vector<Position> slots;
    slots.reserve(count);
    for (uint32 i = 0; i < count; ++i)
    {
        float radius = 0.0f;
        float offset = 0.0f;
        MimironWedgeSlot(ULDUAR_MIMIRON_PHASE1_CAMP_FIRST_ROW, rows, i, count,
                         ULDUAR_MIMIRON_PHASE1_CAMP_HALF_ANGLE, ULDUAR_MIMIRON_PHASE1_CAMP_SPACING,
                         radius, offset);

        float const bearing = Position::NormalizeOrientation(centreline + offset);
        slots.emplace_back(hub.GetPositionX() + radius * std::cos(bearing),
                           hub.GetPositionY() + radius * std::sin(bearing), hub.GetPositionZ());
    }

    float shiftX = 0.0f;
    float shiftY = 0.0f;
    MimironShiftIntoRange(slots, focus, shiftX, shiftY);

    for (Position& slot : slots)
        slot.Relocate(slot.GetPositionX() + shiftX, slot.GetPositionY() + shiftY, slot.GetPositionZ());

    return slots;
}

bool MimironSlotSeesFocus(Position const& slot, Unit* focus)
{
    Position const eye(slot.GetPositionX(), slot.GetPositionY(),
                       slot.GetPositionZ() + ULDUAR_MIMIRON_CAMP_SIGHT_EYE);
    Position const hit = focus->GetHitSpherePointFor(eye);
    return focus->GetMap()->isInLineOfSight(eye.GetPositionX(), eye.GetPositionY(), eye.GetPositionZ(),
                                            hit.GetPositionX(), hit.GetPositionY(), hit.GetPositionZ(),
                                            focus->GetPhaseMask(), LINEOFSIGHT_ALL_CHECKS,
                                            VMAP::ModelIgnoreFlags::Nothing);
}

float MimironPhase1CampTurn(Player* bot, Position const& anchor, Unit* focus, uint32 count)
{
    MimironFightState& state = MimironFightStateFor(bot);
    if (!focus || (state.campTurnScanMs &&
                   GetMSTimeDiffToNow(state.campTurnScanMs) < ULDUAR_MIMIRON_OBS_SCAN_INTERVAL_MS))
        return state.campTurn;

    state.campTurnScanMs = getMSTime();

    float wanted = -1.0f;
    for (float turn = 0.0f; turn <= ULDUAR_MIMIRON_CAMP_SIGHT_MAX + 0.001f;
         turn += ULDUAR_MIMIRON_CAMP_SIGHT_STEP)
    {
        std::vector<Position> const slots = MimironPhase1CampSlots(anchor, focus, count, turn);
        if (std::all_of(slots.begin(), slots.end(),
                        [focus](Position const& slot) { return MimironSlotSeesFocus(slot, focus); }))
        {
            wanted = turn;
            break;
        }
    }

    if (wanted < 0.0f)
        return state.campTurn;

    float const before = state.campTurn;
    if (wanted > state.campTurn)
    {
        state.campTurn = wanted;
        state.campTurnRaisedMs = getMSTime();
    }
    else if (wanted < state.campTurn &&
             GetMSTimeDiffToNow(state.campTurnRaisedMs) >= ULDUAR_MIMIRON_CAMP_SIGHT_HOLD_MS)
    {
        state.campTurn = wanted;
    }

    if (state.campTurn != before && RaidObs::Active())
    {
        char line[16];
        snprintf(line, sizeof(line), "%.0f", state.campTurn * 180.0f / static_cast<float>(M_PI));
        RaidObs::NoteDerived(bot, "mimiron.campturn", line);
    }

    return state.campTurn;
}

Position MimironPhase1CampSlot(Player* bot, Position const& anchor, Unit* focus, uint32 index,
                               uint32 count)
{
    std::vector<Position> const slots =
        MimironPhase1CampSlots(anchor, focus, count, MimironPhase1CampTurn(bot, anchor, focus, count));
    return slots[std::min(index, count - 1)];
}

bool GetMimironPhase3Slot(Player* bot, Group* group, Unit* focus, Position& out, uint32& index,
                          uint32& count)
{
    if (!PlayerbotAI::IsRanged(bot))
        return false;

    index = 0;
    count = 0;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || !PlayerbotAI::IsRanged(member))
            continue;

        if (member == bot)
            index = count;

        ++count;
    }

    if (count == 0)
        return false;

    float const rangedDepth = std::max(sPlayerbotAIConfig.SpellDistance -
                                           ULDUAR_MIMIRON_SPREAD_RANGE_MARGIN -
                                           ULDUAR_MIMIRON_PHASE3_MIN_RADIUS,
                                       0.0f);
    uint32 const rows = MimironWedgeRows(
        ULDUAR_MIMIRON_PHASE3_MIN_RADIUS,
        1u + static_cast<uint32>(rangedDepth / ULDUAR_MIMIRON_PHASE3_SPACING), count,
        ULDUAR_MIMIRON_PHASE3_WEDGE_HALF_ANGLE, ULDUAR_MIMIRON_PHASE3_SPACING);

    Position const& anchor = ULDUAR_MIMIRON_ROOM_CENTER;
    float const centreline = ULDUAR_MIMIRON_ROOM_CENTER.GetAngle(
        ULDUAR_MIMIRON_PHASE3_STAGE.GetPositionX(), ULDUAR_MIMIRON_PHASE3_STAGE.GetPositionY());

    std::vector<Position> slots;
    slots.reserve(count);
    for (uint32 i = 0; i < count; ++i)
    {
        float radius = 0.0f;
        float offset = 0.0f;
        MimironWedgeSlot(ULDUAR_MIMIRON_PHASE3_MIN_RADIUS, rows, i, count,
                         ULDUAR_MIMIRON_PHASE3_WEDGE_HALF_ANGLE, ULDUAR_MIMIRON_PHASE3_SPACING, radius,
                         offset);

        float const bearing = Position::NormalizeOrientation(centreline + offset);
        slots.emplace_back(anchor.GetPositionX() + radius * std::cos(bearing),
                           anchor.GetPositionY() + radius * std::sin(bearing),
                           ULDUAR_MIMIRON_ROOM_CENTER.GetPositionZ());
    }

    float shiftX = 0.0f;
    float shiftY = 0.0f;
    MimironStickyShiftIntoRange(bot, slots, focus, shiftX, shiftY);

    if (slots.empty())
        return false;

    Position const& mine = slots[std::min(index, count - 1)];
    out = Position(mine.GetPositionX() + shiftX, mine.GetPositionY() + shiftY, mine.GetPositionZ());
    return true;
}

float GetMimironWedgeCentreline(PlayerbotAI* botAI, Player* bot)
{
    float const east = ULDUAR_MIMIRON_ROOM_CENTER.GetAngle(
        ULDUAR_MIMIRON_PHASE3_STAGE.GetPositionX(), ULDUAR_MIMIRON_PHASE3_STAGE.GetPositionY());

    MimironFightState& state = MimironFightStateFor(bot);
    float const current = state.wedgeAimedMs ? state.wedgeBearing : east;

    Unit* vx001 = GetUldUnitByEntry(botAI, NPC_VX001);
    if (!vx001)
        return current;

    MimironBarrageWindow const window = GetMimironBarrageWindow(bot, vx001);
    if (!window.valid || window.untilLive <= 0.0f)
        return current;

    if (state.wedgeAimedMs && GetMSTimeDiffToNow(state.wedgeAimedMs) < ULDUAR_MIMIRON_WEDGE_REAIM_MS)
        return current;

    auto const onWedge = [&](float bearing)
    {
        return Position(ULDUAR_MIMIRON_ROOM_CENTER.GetPositionX() +
                            ULDUAR_MIMIRON_SPREAD_RADIUS * std::cos(bearing),
                        ULDUAR_MIMIRON_ROOM_CENTER.GetPositionY() +
                            ULDUAR_MIMIRON_SPREAD_RADIUS * std::sin(bearing),
                        ULDUAR_MIMIRON_ROOM_CENTER.GetPositionZ());
    };

    bool clear = true;
    for (float edge :
         {-ULDUAR_MIMIRON_PHASE3_WEDGE_HALF_ANGLE, 0.0f, ULDUAR_MIMIRON_PHASE3_WEDGE_HALF_ANGLE})
        clear = clear && IsMimironSpotBarrageSafe(vx001, window, onWedge(current + edge), 0.0f);

    if (clear)
        return current;

    state.wedgeBearing =
        Position::NormalizeOrientation(window.lead + static_cast<float>(M_PI) - 0.5f * window.sweep);
    state.wedgeAimedMs = getMSTime();

    if (RaidObs::Active())
    {
        char line[24];
        snprintf(line, sizeof(line), "reaim %.0f",
                 state.wedgeBearing * 180.0f / static_cast<float>(M_PI));
        RaidObs::NoteDerived(bot, "mimiron.wedge", line);
    }

    return state.wedgeBearing;
}

Position GetMimironPhase4TankSpot(PlayerbotAI* botAI, Player* bot)
{
    MimironFightState& state = MimironFightStateFor(bot);
    Position const live = state.tankSpotPickedMs ? state.tankSpot : ULDUAR_MIMIRON_PHASE4_TANK_SPOT;

    if (state.tankSpotScanMs &&
        GetMSTimeDiffToNow(state.tankSpotScanMs) < ULDUAR_MIMIRON_OBS_SCAN_INTERVAL_MS)
        return live;

    state.tankSpotScanMs = getMSTime();

    if (state.tankSpotPickedMs &&
        GetMSTimeDiffToNow(state.tankSpotPickedMs) < ULDUAR_MIMIRON_PHASE4_TANK_HOLD_MS)
        return live;

    if (Unit* vx001 = GetUldUnitByEntry(botAI, NPC_VX001))
    {
        if (GetMimironSpinningUpSeconds(vx001) >= 0.0f ||
            vx001->HasAura(SPELL_P3WX2_LASER_BARRAGE_AURA_1) ||
            vx001->HasAura(SPELL_P3WX2_LASER_BARRAGE_AURA_2))
            return live;
    }

    MimironFirefighterHazards const hazards = GetMimironFirefighterHazards(botAI);
    auto const fireNear = [&hazards](Position const& spot)
    {
        uint32 count = 0;
        for (Position const& flame : hazards.flames)
            if (spot.GetExactDist2d(flame.GetPositionX(), flame.GetPositionY()) <=
                ULDUAR_MIMIRON_PHASE4_TANK_CLEAR_RADIUS)
                ++count;

        return count;
    };

    uint32 const liveFire = fireNear(live);
    if (liveFire <= ULDUAR_MIMIRON_PHASE4_TANK_FIRE_LIMIT)
        return live;

    Position best = live;
    uint32 bestFire = liveFire;
    auto const consider = [&](Position const& candidate)
    {
        uint32 const fire = fireNear(candidate);
        if (fire < bestFire ||
            (fire == bestFire && live.GetExactDist2d(candidate) < live.GetExactDist2d(best)))
        {
            best = candidate;
            bestFire = fire;
        }
    };

    consider(ULDUAR_MIMIRON_PHASE4_TANK_SPOT);
    for (float ring : ULDUAR_MIMIRON_PHASE4_TANK_RINGS)
    {
        for (uint32 i = 0; i < ULDUAR_MIMIRON_PHASE4_TANK_BEARINGS; ++i)
        {
            float const bearing = 2.0f * static_cast<float>(M_PI) * static_cast<float>(i) /
                                  static_cast<float>(ULDUAR_MIMIRON_PHASE4_TANK_BEARINGS);
            consider(Position(ULDUAR_MIMIRON_ROOM_CENTER.GetPositionX() + ring * std::cos(bearing),
                              ULDUAR_MIMIRON_ROOM_CENTER.GetPositionY() + ring * std::sin(bearing),
                              ULDUAR_MIMIRON_ROOM_CENTER.GetPositionZ()));
        }
    }

    if (bestFire + ULDUAR_MIMIRON_PHASE4_TANK_FIRE_MARGIN > liveFire)
        return live;

    state.tankSpot = best;
    state.tankSpotPickedMs = getMSTime();

    if (RaidObs::Active())
    {
        char line[48];
        snprintf(line, sizeof(line), "%.0f,%.0f %u>%u", best.GetPositionX(), best.GetPositionY(),
                 liveFire, bestFire);
        RaidObs::NoteDerived(bot, "mimiron.tankspot", line);
    }

    return best;
}

bool GetMimironHoldSlot(Player* bot, Group* group, Position& out, uint32& index, uint32& count)
{
    MimironHandoverState const& state = MimironHandoverStateFor(bot);
    if (!state.picked)
        return false;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || !member->IsInMap(bot))
            continue;

        if (member == bot)
            index = count;

        ++count;
    }

    if (count == 0)
        return false;

    uint32 const perRow = (count + ULDUAR_MIMIRON_HOLD_ROWS - 1) / ULDUAR_MIMIRON_HOLD_ROWS;
    uint32 const row = index / perRow;
    uint32 const inRow = std::min(perRow, count - row * perRow);
    float const radius = ULDUAR_MIMIRON_HOLD_OUTER_RADIUS - static_cast<float>(row) * ULDUAR_MIMIRON_HOLD_ROW_STEP;
    float const offset = (static_cast<float>(index % perRow) - 0.5f * static_cast<float>(inRow - 1)) *
                         ULDUAR_MIMIRON_HOLD_SPACING / radius;
    float const bearing = Position::NormalizeOrientation(state.bearing + offset);

    out = Position(ULDUAR_MIMIRON_ROOM_CENTER.GetPositionX() + radius * std::cos(bearing),
                   ULDUAR_MIMIRON_ROOM_CENTER.GetPositionY() + radius * std::sin(bearing),
                   ULDUAR_MIMIRON_ROOM_CENTER.GetPositionZ());
    return true;
}

bool DeriveMimironSpreadSlot(PlayerbotAI* botAI, Player* bot, Position& out, char const*& branch,
                             uint32& index, uint32& count)
{
    branch = "none";
    index = 0;
    count = 0;

    if (!botAI || !bot)
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    Unit* focus = GetMimironRingFocus(botAI);
    bool const staging = focus == nullptr;
    bool walkIn = false;
    if (staging)
    {
        MimironHandoverStep const step = CachedMimironHandoverStep(bot);
        if (step == MimironHandoverStep::Hold || step == MimironHandoverStep::Late)
        {
            branch = "hmhold";
            return GetMimironHoldSlot(bot, group, out, index, count);
        }

        walkIn = step == MimironHandoverStep::WalkIn;
        focus = GetMimironStagingFocus(bot);
    }

    if (!focus)
        return false;

    bool const phase4 = staging ? focus->GetEntry() == NPC_VX001 && focus->GetVehicleBase() != nullptr
                                : IsMimironPhase4(bot);

    if (PlayerbotAI::IsMainTank(bot) && phase4)
    {
        if (!staging)
        {
            Unit* leviathanMkII = GetUldUnitByEntry(botAI, NPC_LEVIATHAN_MKII);
            if (leviathanMkII && leviathanMkII->GetVictim() && leviathanMkII->GetVictim() != bot)
                return false;
        }

        branch = "p4tank";
        out = staging || !IsMimironHardModeActive(botAI) ? ULDUAR_MIMIRON_PHASE4_TANK_SPOT
                                                         : GetMimironPhase4TankSpot(botAI, bot);
        return true;
    }

    if (staging && !walkIn)
        return false;

    if (PlayerbotAI::IsMainTank(bot) && focus->GetEntry() == NPC_AERIAL_COMMAND_UNIT)
    {
        branch = "p3tank";
        out = ULDUAR_MIMIRON_ROOM_CENTER;
        return true;
    }

    if (focus->GetEntry() == NPC_AERIAL_COMMAND_UNIT)
    {
        branch = "p3wedge";
        return GetMimironPhase3Slot(bot, group, focus, out, index, count);
    }

    bool const phase1 = !phase4 && focus->GetEntry() == NPC_LEVIATHAN_MKII;
    bool const firefighter = IsMimironHardModeActive(botAI);

    if (PlayerbotAI::IsMainTank(bot) && phase1)
    {
        if (firefighter && !IsMimironTankDragReady(botAI, bot))
            return false;

        branch = "p1tank";
        out = firefighter ? ULDUAR_MIMIRON_PHASE1_TANK_SPOT : ULDUAR_MIMIRON_ROOM_CENTER;
        return true;
    }

    if (firefighter && focus->GetEntry() == NPC_VX001 && (phase4 || !focus->GetVehicleBase()) &&
        !PlayerbotAI::IsRanged(bot))
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (!member || !member->IsAlive() || PlayerbotAI::IsRanged(member) ||
                (phase4 && PlayerbotAI::IsMainTank(member)))
                continue;

            if (member == bot)
                index = count;

            ++count;
        }

        if (count == 0)
            return false;

        branch = phase4 ? "p4melee" : "p2melee";

        float const half = ULDUAR_MIMIRON_PHASE2_MELEE_HALF_ANGLE;
        float const offset = -half + 2.0f * half * (static_cast<float>(index) + 0.5f) / static_cast<float>(count);
        float const bearing = Position::NormalizeOrientation(GetMimironWedgeCentreline(botAI, bot) +
                                                             static_cast<float>(M_PI) + offset);
        float const radius = focus->GetCombatReach() + ULDUAR_MIMIRON_BARRAGE_MELEE_RING_MARGIN;
        out = Position(focus->GetPositionX() + radius * std::cos(bearing),
                       focus->GetPositionY() + radius * std::sin(bearing),
                       ULDUAR_MIMIRON_ROOM_CENTER.GetPositionZ());
        return true;
    }

    if (!PlayerbotAI::IsRanged(bot))
        return false;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || !PlayerbotAI::IsRanged(member) ||
            PlayerbotAI::IsMainTank(member))
            continue;

        if (member == bot)
            index = count;

        ++count;
    }

    if (count == 0)
        return false;

    if (phase1 && firefighter)
    {
        branch = "p1stack";
        out = MimironPhase1CampSlot(bot, GetMimironPhase1StackAnchor(botAI, bot), focus, index, count);
        return true;
    }

    Position const anchor = ClampMimironAnchorToRoom(Position(
        focus->GetPositionX(), focus->GetPositionY(), ULDUAR_MIMIRON_ROOM_CENTER.GetPositionZ()));

    if (firefighter)
    {
        branch = "hmwedge";

        float const rangedDepth = std::max(sPlayerbotAIConfig.SpellDistance -
                                               ULDUAR_MIMIRON_SPREAD_RANGE_MARGIN -
                                               ULDUAR_MIMIRON_PHASE3_MIN_RADIUS,
                                           0.0f);
        uint32 const rows = MimironWedgeRows(
            ULDUAR_MIMIRON_PHASE3_MIN_RADIUS,
            1u + static_cast<uint32>(rangedDepth / ULDUAR_MIMIRON_PHASE3_SPACING), count,
            ULDUAR_MIMIRON_PHASE3_WEDGE_HALF_ANGLE, ULDUAR_MIMIRON_PHASE3_SPACING);

        float radius = 0.0f;
        float offset = 0.0f;
        MimironWedgeSlot(ULDUAR_MIMIRON_PHASE3_MIN_RADIUS, rows, index, count,
                         ULDUAR_MIMIRON_PHASE3_WEDGE_HALF_ANGLE, ULDUAR_MIMIRON_PHASE3_SPACING, radius,
                         offset);

        float const bearing =
            Position::NormalizeOrientation(GetMimironWedgeCentreline(botAI, bot) + offset);

        out = Position(anchor.GetPositionX() + radius * cos(bearing),
                       anchor.GetPositionY() + radius * sin(bearing),
                       ULDUAR_MIMIRON_ROOM_CENTER.GetPositionZ());
        return true;
    }

    branch = "ring";

    float const angle = 2.0f * static_cast<float>(M_PI) * index / count;
    out = Position(anchor.GetPositionX() + ULDUAR_MIMIRON_SPREAD_RADIUS * cos(angle),
                   anchor.GetPositionY() + ULDUAR_MIMIRON_SPREAD_RADIUS * sin(angle),
                   ULDUAR_MIMIRON_ROOM_CENTER.GetPositionZ());
    return true;
}
}

MimironBurstSector GetMimironOtherGroupSector(PlayerbotAI* botAI, Player* bot, Unit* vx001)
{
    MimironBurstSector sector;
    if (!botAI || !bot || !vx001 || !IsMimironHardModeActive(botAI) || !IsMimironPhase2(botAI))
        return sector;

    bool const ranged = PlayerbotAI::IsRanged(bot);
    sector.valid = true;
    sector.centre = Position::NormalizeOrientation(GetMimironWedgeCentreline(botAI, bot) +
                                                   (ranged ? static_cast<float>(M_PI) : 0.0f));
    sector.halfWidth =
        (ranged ? ULDUAR_MIMIRON_PHASE2_MELEE_HALF_ANGLE : ULDUAR_MIMIRON_PHASE3_WEDGE_HALF_ANGLE) +
        ULDUAR_MIMIRON_RAPID_BURST_HALF_ANGLE;
    return sector;
}

bool IsMimironSpotInSector(Unit* vx001, MimironBurstSector const& sector, Position const& dest)
{
    if (!vx001 || !sector.valid)
        return false;

    float const bearing = vx001->GetAngle(dest.GetPositionX(), dest.GetPositionY());
    float off = Position::NormalizeOrientation(bearing - sector.centre);
    if (off > static_cast<float>(M_PI))
        off -= 2.0f * static_cast<float>(M_PI);

    return std::fabs(off) < sector.halfWidth;
}

bool GetMimironSpreadSlot(PlayerbotAI* botAI, Player* bot, Position& out)
{
    if (!IsNearMimironRoom(bot))
        return false;

    char const* branch = "none";
    uint32 index = 0;
    uint32 count = 0;
    bool const found = DeriveMimironSpreadSlot(botAI, bot, out, branch, index, count);

    if (RaidObs::Active())
    {
        std::string value = "none";
        if (found)
        {
            value = branch;
            if (count)
                value += " " + std::to_string(index) + "/" + std::to_string(count);

            value += " " + RaidObs::DescribeDerived(out);
        }

        RaidObs::NoteDerived(bot, "mimiron.slot", value);
    }

    return found;
}
