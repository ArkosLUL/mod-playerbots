/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "UldEncounter_Auriaya.h"

#include "Creature.h"
#include "EncounterHelpers.h"
#include "Group.h"
#include "Player.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldScripts.h"
#include "Unit.h"

#include <cmath>
#include <limits>
#include <list>
#include <vector>

using namespace EncounterHelpers;

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

Unit* GetAuriaya(PlayerbotAI* botAI) { return GetFirstAliveUnitByEntry(botAI, NPC_AURIAYA); }

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
    auto const& units = botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets no los")->Get();
    for (auto const& guid : units)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive() || unit->GetEntry() != NPC_AURIAYA_SANCTUM_SENTRY)
            continue;

        if (unit->GetVictim() != tank)
            return unit;
    }

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
int GetAuriayaStationIndex(PlayerbotAI* botAI)
{
    Unit* boss = GetAuriaya(botAI);
    if (!boss)
        return 0;

    std::vector<Unit*> const pools = CollectAuriayaEssencePools(boss, ULDUAR_AURIAYA_ROOM_SEARCH_RADIUS);

    int best = 0;
    int bestFouling = std::numeric_limits<int>::max();

    for (int i = 0; i < ULDUAR_AURIAYA_STATION_COUNT; ++i)
    {
        int fouling = 0;
        for (Unit* pool : pools)
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
    if (!boss)
        return false;

    if (botAI->IsMainTank(bot))
    {
        out = ULDUAR_AURIAYA_MAINTANK_SPOTS[GetAuriayaStationIndex(botAI)];
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
