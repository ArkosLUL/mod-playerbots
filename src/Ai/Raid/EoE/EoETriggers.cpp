/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "EoETriggers.h"
#include "EoEActions.h"
#include "InstanceScript.h"
#include "ObjectAccessor.h"
#include "SharedDefines.h"
#include "Spell.h"
#include "Timer.h"
#include "Vehicle.h"

#include <list>
#include <unordered_map>

namespace
{
constexpr uint32 EOE_PHASE_CACHE_MS = 500;
constexpr uint32 EOE_CREATURE_CACHE_MS = 300;

// Deliberately wide: the fill is anchored on whichever bot refreshed it, but its answer
// serves the whole raid.
constexpr float EOE_CACHE_SWEEP_RADIUS = 200.0f;
// Fallback when the instance script cannot hand over the boss guid.
constexpr float EOE_BOSS_FALLBACK_SWEEP = 250.0f;

struct PhaseCacheEntry
{
    uint32 at;
    uint8 phase;
};

struct CreatureCacheEntry
{
    uint32 at = 0;
    std::vector<ObjectGuid> guids;
};

// Past the per-bot drake check the answer is identical for the whole instance, so the caches below
// key on the instance. thread_local needs no lock: a bot only updates on its own map thread. With
// MapUpdate.Threads > 1 a map is not pinned to one worker, so an entry can be rebuilt on another
// thread - all bots on a map still share a thread within any single tick, so they never disagree.
thread_local std::unordered_map<uint32, PhaseCacheEntry> phaseCache;
// Instance id in the high half, creature entry in the low half.
thread_local std::unordered_map<uint64, CreatureCacheEntry> creatureCache;
// Malygos' guid, so the whole strategy stops re-running the search behind getMalygos. No window:
// the guid is resolved live on every read and dropped the moment it stops resolving to a live boss.
thread_local std::unordered_map<uint32, ObjectGuid> bossCache;
}

namespace
{
// Empty and never refreshed off the Eye of Eternity map.
std::vector<ObjectGuid> const& GetEoECreatureGuids(Player* bot, uint32 entry)
{
    static std::vector<ObjectGuid> const none;
    if (bot->GetMapId() != EOE_MAP_ID)
    {
        return none;
    }

    uint64 const key = (static_cast<uint64>(bot->GetInstanceId()) << 32) | entry;
    uint32 const now = getMSTime();
    CreatureCacheEntry& cached = creatureCache[key];
    if (!cached.at || getMSTimeDiff(cached.at, now) >= EOE_CREATURE_CACHE_MS)
    {
        cached.at = now;
        cached.guids.clear();

        std::list<Creature*> found;
        bot->GetCreatureListWithEntryInGrid(found, entry, EOE_CACHE_SWEEP_RADIUS);
        for (Creature* creature : found)
        {
            if (creature->IsAlive())
            {
                cached.guids.push_back(creature->GetGUID());
            }
        }
    }

    return cached.guids;
}
}

void GetEoECreatures(Player* bot, uint32 entry, std::vector<Unit*>& out)
{
    out.clear();
    for (ObjectGuid const& guid : GetEoECreatureGuids(bot, entry))
    {
        Unit* unit = ObjectAccessor::GetUnit(*bot, guid);
        if (unit && unit->IsAlive())
        {
            out.push_back(unit);
        }
    }
}

Unit* GetNearestEoECreature(Player* bot, uint32 entry, float maxDist)
{
    Unit* closest = nullptr;
    float closestDist = maxDist;
    for (ObjectGuid const& guid : GetEoECreatureGuids(bot, entry))
    {
        Unit* unit = ObjectAccessor::GetUnit(*bot, guid);
        if (!unit || !unit->IsAlive())
        {
            continue;
        }

        float dist = bot->GetExactDist2d(unit);
        if (dist <= closestDist)
        {
            closestDist = dist;
            closest = unit;
        }
    }
    return closest;
}

bool AnyEoECreature(Player* bot, uint32 entry)
{
    for (ObjectGuid const& guid : GetEoECreatureGuids(bot, entry))
    {
        Unit* unit = ObjectAccessor::GetUnit(*bot, guid);
        if (unit && unit->IsAlive())
        {
            return true;
        }
    }
    return false;
}

Unit* MalygosTrigger::getMalygos(Player* bot)
{
    ObjectGuid& cached = bossCache[bot->GetInstanceId()];
    if (!cached.IsEmpty())
    {
        if (Unit* boss = ObjectAccessor::GetUnit(*bot, cached))
        {
            if (boss->IsAlive())
            {
                return boss;
            }
        }

        cached.Clear();
    }

    // The instance script already holds the guid, so ask it before falling back to a grid sweep.
    if (InstanceScript* instance = bot->GetInstanceScript())
    {
        // Guid lookup has no liveness filter of its own, unlike the search below it. A dead boss
        // ends the lookup here rather than paying for the sweep to tell us the same thing.
        if (Creature* boss = instance->GetCreature(EOE_DATA_MALYGOS))
        {
            if (!boss->IsAlive())
            {
                return nullptr;
            }

            cached = boss->GetGUID();
            return boss;
        }
    }

    Unit* boss = bot->FindNearestCreature(NPC_MALYGOS, EOE_BOSS_FALLBACK_SWEEP, true);
    if (boss)
    {
        cached = boss->GetGUID();
    }
    return boss;
}

uint8 MalygosTrigger::getPhase(Player* bot)
{
    if (bot->GetMapId() != EOE_MAP_ID)
    {
        return 0;
    }

    // Riding a Skytalon is the one per-bot part of the answer, so it is asked every time and has
    // to come before the shared cache.
    Unit* drake = bot->GetVehicleBase();
    if (drake && drake->GetEntry() == NPC_WYRMREST_SKYTALON)
    {
        return 3;
    }

    uint32 const now = getMSTime();
    PhaseCacheEntry& cached = phaseCache[bot->GetInstanceId()];
    if (cached.at && getMSTimeDiff(cached.at, now) < EOE_PHASE_CACHE_MS)
    {
        return cached.phase;
    }
    cached.at = now;
    cached.phase = 0;

    Unit* boss = getMalygos(bot);
    if (!boss || !boss->IsInCombat())
    {
        return 0;
    }

    // P2: Malygos is airborne/untargetable while the disc adds are up.
    if (AnyEoECreature(bot, NPC_NEXUS_LORD) || AnyEoECreature(bot, NPC_SCION_OF_ETERNITY))
    {
        cached.phase = 2;
        return 2;
    }

    if (!boss->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE))
    {
        cached.phase = 1;
        return 1;
    }

    // P1->P2 / P2->P3 transition, and the pull intro.
    cached.phase = 4;
    return 4;
}

bool MalygosTrigger::IsActive()
{
    uint8 phase = getPhase(bot);
    return phase == 1 || phase == 2 || phase == 4;
}

bool PowerSparkTrigger::IsActive()
{
    if (MalygosTrigger::getPhase(bot) != 1)
    {
        return false;
    }

    return AnyEoECreature(bot, NPC_POWER_SPARK);
}

bool MalygosBubbleTrigger::IsActive()
{
    if (MalygosTrigger::getPhase(bot) != 2)
    {
        return false;
    }

    // Disk riders are already immune to both Arcane Overload and Surge of Power.
    if (bot->GetVehicle())
    {
        return false;
    }

    // Fires again once the current bubble is nearly spent, so the bot moves before it despawns.
    if (IsSafelySheltered(bot))
    {
        return false;
    }

    if (!botAI->IsRanged(bot) && !botAI->IsHeal(bot))
    {
        // Melee and tanks owe the raid a dead Nexus Lord and then a disk ride first; the disk half
        // only counts for bots MalygosFreeDiskTrigger will actually let board.
        if (GetNearestEoECreature(bot, NPC_NEXUS_LORD, BUBBLE_SEARCH_RADIUS))
        {
            return false;
        }
        if (IsEligibleDiskRider(bot) && AnyScionAlive(bot) && FindFreeHoverDisk(bot))
        {
            return false;
        }
    }

    return GetNearestEoECreature(bot, NPC_ARCANE_OVERLOAD, BUBBLE_SEARCH_RADIUS) != nullptr;
}

bool MalygosFreeDiskTrigger::IsActive()
{
    if (MalygosTrigger::getPhase(bot) != 2)
    {
        return false;
    }
    if (bot->GetVehicle())
    {
        return false;
    }
    if (!IsEligibleDiskRider(bot))
    {
        return false;
    }

    // Don't climb back onto a disk the bot just got off because the Scions are dead.
    if (!AnyScionAlive(bot))
    {
        return false;
    }

    return FindFreeHoverDisk(bot) != nullptr;
}

bool MalygosOnDiskTrigger::IsActive()
{
    Unit* vehicleBase = bot->GetVehicleBase();
    return vehicleBase && vehicleBase->GetEntry() == NPC_HOVER_DISK;
}

bool SurgeOfPowerTrigger::IsActive()
{
    if (MalygosTrigger::getPhase(bot) != 2)
    {
        return false;
    }

    if (GetNearestEoECreature(bot, NPC_SURGE_OF_POWER, EOE_SURGE_SEARCH_RADIUS))
    {
        return true;
    }

    Unit* boss = MalygosTrigger::getMalygos(bot);
    return boss && bool(boss->FindCurrentSpellBySpellId(SPELL_SURGE_OF_POWER_P2));
}

bool MalygosDrakeFlightTrigger::IsActive()
{
    Unit* drake = bot->GetVehicleBase();
    return drake && drake->GetEntry() == NPC_WYRMREST_SKYTALON;
}

bool DrakeSurgeTrigger::IsActive()
{
    if (MalygosTrigger::getPhase(bot) != 3)
    {
        return false;
    }

    // The healer rotation reads the same helper, so the two cannot disagree about who is hit.
    return IsDrakeSurgeTarget(botAI);
}
