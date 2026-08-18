/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "EoEEncounter_Malygos.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Playerbots.h"
#include "SpellAuraEffects.h"
#include "Timer.h"
#include "Vehicle.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <list>
#include <unordered_map>

namespace
{
constexpr uint32 EOE_PHASE_CACHE_MS = 500;
constexpr uint32 EOE_CREATURE_CACHE_MS = 300;

// Swept from the platform centre rather than from whichever bot refreshed it, so the shared answer
// really is position-independent. Covers the Power Spark spawn ring at r 107; everything else the
// strategy looks for lives on the platform inside r 56.
constexpr float EOE_CACHE_SWEEP_RADIUS = 120.0f;
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

struct LayoutCacheEntry
{
    uint32 at = 0;
    bool latched = false;
    MalygosP1Layout layout;
};

// Past the per-bot drake check the answer is identical for the whole instance, so the caches below
// key on the instance. thread_local needs no lock: a bot only updates on its own map thread. With
// MapUpdate.Threads > 1 a map is not pinned to one worker, so an entry can be rebuilt on another
// thread - all bots on a map still share a thread within any single tick, so they never disagree.
// Nothing is ever evicted: four maps at most, one entry per instance id per worker thread.
thread_local std::unordered_map<uint32, PhaseCacheEntry> phaseCache;
// Instance id in the high half, creature entry in the low half.
thread_local std::unordered_map<uint64, CreatureCacheEntry> creatureCache;
// Malygos' guid, so the whole strategy stops re-running the search behind GetMalygos. No window:
// the guid is resolved live on every read and dropped the moment it stops resolving to a live boss.
thread_local std::unordered_map<uint32, ObjectGuid> bossCache;
thread_local std::unordered_map<uint32, LayoutCacheEntry> layoutCache;

// AllCreaturesOfEntryInRange measures from the object it was constructed with, so a sweep anchored
// on a fixed point needs its own check.
struct CreaturesOfEntryNearPoint
{
    CreaturesOfEntryNearPoint(uint32 entry, float x, float y, float range)
        : _entry(entry), _x(x), _y(y), _range(range)
    {
    }

    bool operator()(Creature* creature) const
    {
        return creature->GetEntry() == _entry && creature->GetExactDist2d(_x, _y) <= _range;
    }

    uint32 _entry;
    float _x;
    float _y;
    float _range;
};

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

        float const centreX = MALYGOS_CENTER_POSITION.first;
        float const centreY = MALYGOS_CENTER_POSITION.second;

        std::list<Creature*> found;
        CreaturesOfEntryNearPoint check(entry, centreX, centreY, EOE_CACHE_SWEEP_RADIUS);
        // CreatureListSearcher only reads the phase mask off its first argument, so handing it the
        // bot while visiting the cells around the centre is correct.
        Acore::CreatureListSearcher<CreaturesOfEntryNearPoint> searcher(bot, found, check);
        Cell::VisitObjects(centreX, centreY, bot->GetMap(), searcher, EOE_CACHE_SWEEP_RADIUS);

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

std::pair<float, float> MalygosP1Spot(float angle, float offset)
{
    return {MALYGOS_CENTER_POSITION.first + std::cos(angle) * offset,
            MALYGOS_CENTER_POSITION.second + std::sin(angle) * offset};
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

Unit* GetMalygos(Player* bot)
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

uint8 GetMalygosPhase(Player* bot)
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

    Unit* boss = GetMalygos(bot);
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

MalygosP1Layout const& GetMalygosP1Layout(Player* bot)
{
    LayoutCacheEntry& cached = layoutCache[bot->GetInstanceId()];
    uint32 const now = getMSTime();
    if (cached.at && getMSTimeDiff(cached.at, now) >= EOE_LATCH_STALE_MS)
    {
        cached.latched = false;
    }
    cached.at = now;

    uint8 const phase = GetMalygosPhase(bot);
    if (phase == 0)
    {
        cached.latched = false;
    }
    else if (cached.latched)
    {
        return cached.layout;
    }

    // He is committed to his bearing the instant JustEngagedWith fires, so latch the first one.
    float angle = MALYGOS_LANDING_ANGLES[0];
    if (Unit* boss = GetMalygos(bot))
    {
        float const dx = boss->GetPositionX() - MALYGOS_CENTER_POSITION.first;
        float const dy = boss->GetPositionY() - MALYGOS_CENTER_POSITION.second;
        if (std::fabs(dx) > 1.0f || std::fabs(dy) > 1.0f)
        {
            float const bearing = std::atan2(dy, dx);
            float closest = std::numeric_limits<float>::max();
            for (uint8 i = 0; i < MALYGOS_LANDING_ANGLE_COUNT; ++i)
            {
                // Wrapped into [-pi, pi] so a bearing either side of the seam picks its neighbour.
                float diff = std::fabs(std::remainder(bearing - MALYGOS_LANDING_ANGLES[i], EOE_TWO_PI));
                if (diff < closest)
                {
                    closest = diff;
                    angle = MALYGOS_LANDING_ANGLES[i];
                }
            }
        }

        cached.latched = phase != 0;
    }

    cached.layout.tank = MalygosP1Spot(angle, MALYGOS_MAINTANK_OFFSET);
    cached.layout.stack = MalygosP1Spot(angle, MALYGOS_STACK_OFFSET);
    cached.layout.hunter = MalygosP1Spot(angle, MALYGOS_HUNTER_OFFSET);
    cached.layout.grip = MalygosP1Spot(angle, POWER_SPARK_GRIP_OFFSET);
    return cached.layout;
}

void GetLivePowerSparks(Player* bot, std::vector<Unit*>& out)
{
    GetEoECreatures(bot, NPC_POWER_SPARK, out);

    // Spark-specific on purpose: the P2 bubbles, the surge focus and the hover disks are all
    // unselectable from spawn, so the same filter in the creature cache would blind half of P2.
    out.erase(std::remove_if(out.begin(), out.end(), [](Unit* spark)
    {
        return spark->HasUnitFlag(UnitFlags(UNIT_FLAG_NOT_SELECTABLE | UNIT_FLAG_NON_ATTACKABLE));
    }), out.end());
}

Unit* GetNearestPowerSparkTo(PlayerbotAI* botAI, float x, float y)
{
    std::vector<Unit*> sparks;
    GetLivePowerSparks(botAI->GetBot(), sparks);

    Unit* closest = nullptr;
    float closestDist = std::numeric_limits<float>::max();
    for (Unit* spark : sparks)
    {
        float const dist = spark->GetExactDist2d(x, y);
        if (dist < closestDist)
        {
            closestDist = dist;
            closest = spark;
        }
    }
    return closest;
}

Unit* GetPowerSparkToKill(PlayerbotAI* botAI, Unit* currentTarget)
{
    Player* bot = botAI->GetBot();

    std::vector<Unit*> sparks;
    GetLivePowerSparks(bot, sparks);
    if (sparks.empty())
    {
        return nullptr;
    }

    Unit* boss = GetMalygos(bot);
    bool const ranged = botAI->IsRanged(bot);

    Unit* best = nullptr;
    float bestBossDist = std::numeric_limits<float>::max();
    for (Unit* spark : sparks)
    {
        bool inReach;
        if (ranged)
        {
            inReach = bot->IsWithinCombatRange(spark, sPlayerbotAIConfig.spellDistance);
        }
        else
        {
            inReach = bot->IsWithinMeleeRange(spark, currentTarget == spark ? POWER_SPARK_MELEE_STICKY : 0.0f);
        }
        if (!inReach)
        {
            continue;
        }

        // Nearest to handing over its buff, not nearest to the bot.
        float const bossDist = boss ? boss->GetExactDist2d(spark) : bot->GetExactDist2d(spark);
        if (bossDist < bestBossDist)
        {
            bestBossDist = bossDist;
            best = spark;
        }
    }
    return best;
}

Unit* GetPowerSparkToSnare(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot->IsClass(CLASS_DEATH_KNIGHT) || bot->GetVehicle())
    {
        return nullptr;
    }

    uint32 const chainsId = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", "chains of ice")->Get();
    if (!chainsId || !bot->HasSpell(chainsId) || bot->HasSpellCooldown(chainsId))
    {
        return nullptr;
    }

    std::vector<Unit*> sparks;
    GetLivePowerSparks(bot, sparks);

    Unit* best = nullptr;
    float bestDist = POWER_SPARK_SNARE_RADIUS;
    for (Unit* spark : sparks)
    {
        // Any caster's: a second DK re-snaring costs a rune and buys nothing.
        if (spark->HasAura(chainsId))
        {
            continue;
        }

        float const dist = bot->GetExactDist2d(spark);
        if (dist < bestDist)
        {
            bestDist = dist;
            best = spark;
        }
    }
    return best;
}

bool IsOnPowerSparkGripDuty(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!bot->IsClass(CLASS_DEATH_KNIGHT) || bot->GetVehicle())
    {
        return false;
    }

    uint32 const gripId = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", "death grip")->Get();
    if (!gripId || !bot->HasSpell(gripId) || bot->HasSpellCooldown(gripId))
    {
        return false;
    }

    Unit* boss = GetMalygos(bot);
    if (!boss)
    {
        return false;
    }

    std::pair<float, float> const& grip = GetMalygosP1Layout(bot).grip;
    if (boss->GetExactDist2d(grip.first, grip.second) < POWER_SPARK_GRIP_SAFE_BOSS_DISTANCE)
    {
        return false;
    }

    Unit* spark = GetNearestPowerSparkTo(botAI, grip.first, grip.second);
    return spark && spark->GetExactDist2d(grip.first, grip.second) <= POWER_SPARK_GRIP_ENGAGE_RADIUS;
}

float GetBubbleShrinkFactor(Unit* bubble)
{
    if (!bubble)
    {
        return 0.0f;
    }

    Aura* aura = bubble->GetAura(SPELL_ARCANE_OVERLOAD_AURA);
    if (!aura)
    {
        // Applied from creature_template_addon at spawn, so a missing aura means brand new.
        return 1.0f;
    }

    // Only the periodic effect counts ticks; the others sit at 0.
    uint32 ticks = 0;
    for (uint8 i = 0; i < MAX_SPELL_EFFECTS; ++i)
    {
        if (AuraEffect const* effect = aura->GetEffect(i))
        {
            ticks = std::max(ticks, effect->GetTickNumber());
        }
    }

    float factor = 1.0f - BUBBLE_SHRINK_PER_TICK * static_cast<float>(ticks + 1);
    return std::max(0.0f, factor);
}

bool IsSafelySheltered(Player* bot)
{
    if (!bot->HasAura(SPELL_ARCANE_OVERLOAD_PROTECTION))
    {
        return false;
    }

    Unit* current = GetNearestEoECreature(bot, NPC_ARCANE_OVERLOAD, BUBBLE_SEARCH_RADIUS);
    return current && GetBubbleShrinkFactor(current) >= BUBBLE_MIN_USABLE_FACTOR;
}

Unit* FindFreeHoverDisk(Player* bot)
{
    std::vector<Unit*> disks;
    GetEoECreatures(bot, NPC_HOVER_DISK, disks);

    Unit* closest = nullptr;
    // Sentinel: anything past the distance a bot will walk can never win.
    float closestDist = 40.0f;
    for (Unit* disk : disks)
    {
        if (disk->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
        {
            continue;
        }

        Vehicle* kit = disk->GetVehicleKit();
        if (!kit || !kit->GetAvailableSeatCount())
        {
            continue;
        }

        float dist = bot->GetExactDist2d(disk);
        if (dist < closestDist)
        {
            closestDist = dist;
            closest = disk;
        }
    }
    return closest;
}

bool IsEligibleDiskRider(Player* bot)
{
    return !PlayerbotAI::IsTank(bot) && PlayerbotAI::IsDps(bot) && !PlayerbotAI::IsRanged(bot);
}

bool AnyScionAlive(Player* bot)
{
    return AnyEoECreature(bot, NPC_SCION_OF_ETERNITY);
}
