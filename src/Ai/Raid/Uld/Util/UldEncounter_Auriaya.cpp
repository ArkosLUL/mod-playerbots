/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldEncounter_Auriaya.h"

#include "AttackersValue.h"
#include "CellImpl.h"
#include "Creature.h"
#include "EncounterHelpers.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Group.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "UldScripts.h"
#include "Unit.h"

#include <cmath>
#include <limits>
#include <list>
#include <vector>

using namespace EncounterHelpers;

namespace
{
// The check behind "possible targets no los", with the entry tested first.
struct UnfriendlyOfEntryInRangeCheck
{
    Acore::AnyUnfriendlyUnitInObjectRangeCheck inRange;
    uint32 entry;

    bool operator()(Unit* unit) { return unit->GetEntry() == entry && inRange(unit); }
};

// What "possible targets no los" holds for one entry, in the same order. Reading the value itself
// rebuilds and copies the whole list on every call, with IsPossibleTarget on every hostile in sight.
std::vector<Unit*> CollectPossibleTargetsByEntry(Player* bot, uint32 entry)
{
    float const range = sPlayerbotAIConfig.sightDistance;

    std::vector<Unit*> matches;
    UnfriendlyOfEntryInRangeCheck check{Acore::AnyUnfriendlyUnitInObjectRangeCheck(bot, bot, range), entry};
    Acore::UnitListSearcher<UnfriendlyOfEntryInRangeCheck> searcher(bot, matches, check);
    Cell::VisitObjects(bot, searcher, range);

    std::vector<Unit*> targets;
    for (Unit* unit : matches)
        if (AttackersValue::IsPossibleTarget(unit, bot, range))
            targets.push_back(unit);

    return targets;
}
}  // namespace

// Auriaya's lane. She spawns at (1956.2, 49.32, 411.36) facing (-0.955, 0.296), which points down
// the room and directly away from the corridor at +x, so the fight walks that bearing in 10 yd steps
// as her void zones pile up. Tank spots sit 5/15/25 yd out, the raid points 15 yd behind each. All
// six are inside the floor at x 1909-1956, y 43-82.
const Position ULDUAR_AURIAYA_MAINTANK_SPOTS[ULDUAR_AURIAYA_STATION_COUNT] = {
    Position(1951.42f, 50.79f, 411.36f), Position(1941.87f, 53.75f, 411.36f),
    Position(1932.32f, 56.71f, 411.36f)};
const Position ULDUAR_AURIAYA_NOMINAL_RAID_POINTS[ULDUAR_AURIAYA_STATION_COUNT] = {
    Position(1937.10f, 55.23f, 411.36f), Position(1927.54f, 58.19f, 411.36f),
    Position(1917.99f, 61.15f, 411.36f)};

// Terrifying Screech repeats on a fixed 35s cycle from the pull, so the whole encounter is one long
// fear window - there is no narrower slice worth reserving Tremor Totem for.
bool AuriayaFearWindowActive(PlayerbotAI* botAI) { return AuriayaEncounterActive(botAI); }

Unit* GetAuriaya(PlayerbotAI* botAI)
{
    std::vector<Unit*> const found = CollectPossibleTargetsByEntry(botAI->GetBot(), NPC_AURIAYA);
    return found.empty() ? nullptr : found.front();
}

bool AuriayaEncounterActive(PlayerbotAI* botAI) { return GetAuriaya(botAI) != nullptr; }

bool IsAuriayaEngaged(PlayerbotAI* botAI)
{
    Unit* boss = GetAuriaya(botAI);
    return boss && boss->IsInCombat();
}

Unit* GetAuriayaFocusTarget(PlayerbotAI* botAI)
{
    if (Unit* sentry = GetFirstAliveUnitByEntry(botAI, NPC_AURIAYA_SANCTUM_SENTRY))
        return sentry;

    // Between lives the Defender lies feigned at 1 HP and unselectable, so IsAlive() alone would
    // keep the raid pointed at something it cannot hit.
    Unit* defender = GetFirstAliveUnitByEntry(botAI, NPC_AURIAYA_FERAL_DEFENDER);

    return IsDownOrFeigning(defender) ? nullptr : defender;
}

Unit* GetAuriayaLooseSentry(PlayerbotAI* botAI, Player* tank)
{
    for (Unit* sentry : CollectPossibleTargetsByEntry(botAI->GetBot(), NPC_AURIAYA_SANCTUM_SENTRY))
        if (sentry->GetVictim() != tank)
            return sentry;

    return nullptr;
}

std::vector<Unit*> CollectAuriayaEssencePools(WorldObject* from, float radius)
{
    std::vector<Unit*> pools;
    if (!from)
        return pools;

    std::list<Creature*> found;
    from->GetCreatureListWithEntryInGrid(found, NPC_AURIAYA_SEEPING_FERAL_ESSENCE, radius);

    for (Creature* creature : found)
        if (creature && creature->IsAlive())
            pools.push_back(creature);

    return pools;
}

// Score every station by how many pools foul it and take the lowest, ties to the lowest index. A
// count over fixed geometry can only grow while the fight runs, so the station slides west and never
// comes back east.
int GetAuriayaStationIndex(std::vector<Unit*> const& roomPools)
{
    int best = 0;
    int bestFouling = std::numeric_limits<int>::max();

    for (int i = 0; i < ULDUAR_AURIAYA_STATION_COUNT; ++i)
    {
        int fouling = 0;
        for (Unit* pool : roomPools)
        {
            if (pool->GetExactDist2d(&ULDUAR_AURIAYA_MAINTANK_SPOTS[i]) < ULDUAR_AURIAYA_STATION_FOUL_RADIUS ||
                pool->GetExactDist2d(&ULDUAR_AURIAYA_NOMINAL_RAID_POINTS[i]) < ULDUAR_AURIAYA_STATION_FOUL_RADIUS)
            {
                ++fouling;
            }
        }

        if (fouling < bestFouling)
        {
            bestFouling = fouling;
            best = i;
        }
    }

    return best;
}

bool GetAuriayaAnchor(PlayerbotAI* botAI, Player* bot, Position& out, float& tolerance)
{
    Unit* boss = GetAuriaya(botAI);
    return boss && GetAuriayaAnchor(botAI, bot, boss, nullptr, out, tolerance);
}

bool GetAuriayaAnchor(PlayerbotAI* botAI, Player* bot, Unit* boss, std::vector<Unit*> const* roomPools,
                      Position& out, float& tolerance)
{
    if (!boss)
        return false;

    if (botAI->IsMainTank(bot))
    {
        int const station =
            roomPools ? GetAuriayaStationIndex(*roomPools)
                      : GetAuriayaStationIndex(CollectAuriayaEssencePools(boss, ULDUAR_AURIAYA_ROOM_SEARCH_RADIUS));

        out = ULDUAR_AURIAYA_MAINTANK_SPOTS[station];
        tolerance = ULDUAR_AURIAYA_MAINTANK_SPOT_TOLERANCE;
        return true;
    }

    if (!botAI->IsRanged(bot))
        return false;

    // The stack has to sit inside the cone, and the cone points at whoever she is chasing. Reading
    // her victim rather than a fixed bearing is what keeps this correct when a human holds her.
    Unit* victim = boss->GetVictim();
    float bearing = victim && victim != boss
                        ? boss->GetAngle(victim)
                        : boss->GetOrientation();

    // Round the bearing off so tank drift cannot shuffle the whole raid every tick.
    bearing = std::round(bearing / ULDUAR_AURIAYA_BEARING_QUANTUM) * ULDUAR_AURIAYA_BEARING_QUANTUM;

    out = Position(boss->GetPositionX() + std::cos(bearing) * ULDUAR_AURIAYA_RAID_STANDOFF,
                   boss->GetPositionY() + std::sin(bearing) * ULDUAR_AURIAYA_RAID_STANDOFF,
                   boss->GetPositionZ());
    tolerance = botAI->IsRangedDps(bot) ? ULDUAR_AURIAYA_RANGED_SPOT_TOLERANCE
                                        : ULDUAR_AURIAYA_HEALER_SPOT_TOLERANCE;
    return true;
}
