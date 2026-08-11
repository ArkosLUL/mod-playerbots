/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "EoEActions.h"
#include "CreatureAI.h"
#include "EoETriggers.h"
#include "Playerbots.h"
#include "RaidBossHelpers.h"
#include "ServerFacade.h"
#include "SpellAuraEffects.h"
#include "Timer.h"
#include "Vehicle.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <vector>

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

void GetStaticFields(Player* bot, std::vector<Unit*>& fields)
{
    GetEoECreatures(bot, NPC_STATIC_FIELD, fields);
}

bool IsClearOfStaticFields(float x, float y, std::vector<Unit*> const& fields, float safeRadius)
{
    for (Unit* field : fields)
    {
        if (field->GetExactDist2d(x, y) < safeRadius)
        {
            return false;
        }
    }
    return true;
}

namespace
{
constexpr float TWO_PI = 2.0f * static_cast<float>(M_PI);

// Instance ids are recycled, so a latch with no window eventually hands a fresh pull the previous
// tenant's state. Far longer than any encounter, so it can never expire mid-fight.
constexpr uint32 EOE_LATCH_STALE_MS = 5 * MINUTE * IN_MILLISECONDS;

// Heading the flight holds on the ring, latched per instance. It only ever advances.
struct DrakeStackAngle
{
    uint32 at = 0;
    float angle = DRAKE_STACK_ANGLE;
};

thread_local std::unordered_map<uint32, DrakeStackAngle> stackAngleCache;
}

bool GetDrakeStackPoint(Player* bot, std::vector<Unit*> const& fields, float& x, float& y, float& z)
{
    if (!bot->GetVehicleBase())
    {
        return false;
    }

    Unit* boss = MalygosTrigger::getMalygos(bot);
    if (!boss)
    {
        return false;
    }

    float const bossX = boss->GetPositionX();
    float const bossY = boss->GetPositionY();
    z = MALYGOS_P3_BOSS_Z;

    DrakeStackAngle& cachedAngle = stackAngleCache[bot->GetInstanceId()];
    uint32 const nowMs = getMSTime();
    if (cachedAngle.at && getMSTimeDiff(cachedAngle.at, nowMs) >= EOE_LATCH_STALE_MS)
    {
        cachedAngle.angle = DRAKE_STACK_ANGLE;
    }
    cachedAngle.at = nowMs;

    float& held = cachedAngle.angle;

    x = bossX + std::cos(held) * DRAKE_STACK_RADIUS;
    y = bossY + std::sin(held) * DRAKE_STACK_RADIUS;

    if (IsClearOfStaticFields(x, y, fields, STATIC_FIELD_CLEARANCE))
    {
        return true;
    }

    // The dodge stays on the ring, so it can neither close on the boss nor lose range. It must
    // not sweep backwards, and must not restart from DRAKE_STACK_ANGLE - either one reverses
    // the whole formation when a field behind the flight expires.
    float const step = TWO_PI / DRAKE_RING_HEADINGS;

    float bestClearance = 0.0f;
    float bestAngle = held;
    float bestX = x;
    float bestY = y;

    for (uint8 i = 1; i <= DRAKE_RING_HEADINGS; ++i)
    {
        float const angle = held + static_cast<float>(i) * step;
        float const cx = bossX + std::cos(angle) * DRAKE_STACK_RADIUS;
        float const cy = bossY + std::sin(angle) * DRAKE_STACK_RADIUS;

        float clearance = std::numeric_limits<float>::max();
        for (Unit* field : fields)
        {
            clearance = std::min(clearance, field->GetExactDist2d(cx, cy));
        }

        if (clearance >= STATIC_FIELD_CLEARANCE)
        {
            held = std::remainder(angle, TWO_PI);
            x = cx;
            y = cy;
            return true;
        }

        if (clearance > bestClearance)
        {
            bestClearance = clearance;
            bestAngle = angle;
            bestX = cx;
            bestY = cy;
        }
    }

    // Boxed in: take the roomiest heading rather than sit in the field.
    held = std::remainder(bestAngle, TWO_PI);
    x = bestX;
    y = bestY;
    return true;
}

void GetDrakeApproachPoint(Unit* drake, Unit* boss, float destX, float destY, float& x, float& y)
{
    x = destX;
    y = destY;
    if (!boss)
    {
        return;
    }

    float const bossX = boss->GetPositionX();
    float const bossY = boss->GetPositionY();

    float const from = std::atan2(drake->GetPositionY() - bossY, drake->GetPositionX() - bossX);
    float const to = std::atan2(destY - bossY, destX - bossX);
    float delta = std::remainder(to - from, TWO_PI);

    // Forward-only is a dodge rule, and it only holds once the drake is on the ring, where the short
    // way back runs through the field the stack just left. A drake still on its way to the ring is
    // dodging nothing, and P3 spawns every drake on top of Malygos, where its heading off him is
    // noise - sending those the long way round splits the flight into two arrival waves. A point less
    // than a hop behind is drift off the stack rather than a dodge either way, so it flies straight
    // back to it.
    bool const onRing =
        std::fabs(drake->GetExactDist2d(bossX, bossY) - DRAKE_STACK_RADIUS) <= DRAKE_STACK_TOLERANCE;
    if (onRing && delta < -DRAKE_APPROACH_ARC)
    {
        delta += TWO_PI;
    }

    if (std::fabs(delta) <= DRAKE_APPROACH_ARC)
    {
        return;
    }

    float const angle = from + std::copysign(DRAKE_APPROACH_ARC, delta);
    x = bossX + std::cos(angle) * DRAKE_STACK_RADIUS;
    y = bossY + std::sin(angle) * DRAKE_STACK_RADIUS;
}

namespace
{
struct LayoutCacheEntry
{
    uint32 at = 0;
    bool latched = false;
    MalygosP1Layout layout;
};

// Keyed on the instance and thread_local: a bot is only updated from its own map's thread.
thread_local std::unordered_map<uint32, LayoutCacheEntry> layoutCache;

std::pair<float, float> MalygosP1Spot(float angle, float offset)
{
    return {MALYGOS_CENTER_POSITION.first + std::cos(angle) * offset,
            MALYGOS_CENTER_POSITION.second + std::sin(angle) * offset};
}
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

    uint8 const phase = MalygosTrigger::getPhase(bot);
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
    if (Unit* boss = MalygosTrigger::getMalygos(bot))
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
                float diff = std::fabs(std::remainder(bearing - MALYGOS_LANDING_ANGLES[i],
                                                      2.0f * static_cast<float>(M_PI)));
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

Unit* GetNearestPowerSpark(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Unit* closest = nullptr;
    float closestDist = std::numeric_limits<float>::max();

    GuidVector targets = botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets no los")->Get();
    for (auto& target : targets)
    {
        Unit* unit = botAI->GetUnit(target);
        if (!unit || unit->GetEntry() != NPC_POWER_SPARK)
        {
            continue;
        }

        float dist = bot->GetExactDist2d(unit);
        if (dist < closestDist)
        {
            closestDist = dist;
            closest = unit;
        }
    }
    return closest;
}

Unit* GetPowerSparkToKill(PlayerbotAI* botAI, Unit* currentTarget)
{
    Player* bot = botAI->GetBot();

    std::vector<Unit*> sparks;
    GetEoECreatures(bot, NPC_POWER_SPARK, sparks);
    if (sparks.empty())
    {
        return nullptr;
    }

    Unit* boss = MalygosTrigger::getMalygos(bot);
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

    Unit* boss = MalygosTrigger::getMalygos(bot);
    if (!boss)
    {
        return false;
    }

    std::pair<float, float> const& grip = GetMalygosP1Layout(bot).grip;
    if (boss->GetExactDist2d(grip.first, grip.second) < POWER_SPARK_GRIP_SAFE_BOSS_DISTANCE)
    {
        return false;
    }

    Unit* spark = GetNearestPowerSpark(botAI);
    return spark && spark->GetExactDist2d(grip.first, grip.second) <= POWER_SPARK_GRIP_ENGAGE_RADIUS;
}

void GetDrakeHealerGuids(PlayerbotAI* botAI, std::vector<ObjectGuid>& out)
{
    out.clear();

    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    if (!group)
    {
        if (botAI->IsHeal(bot))
        {
            out.push_back(bot->GetGUID());
        }
        return;
    }

    uint8 const wanted =
        bot->GetRaidDifficulty() == RAID_DIFFICULTY_25MAN_NORMAL ? DRAKE_HEALERS_25MAN : DRAKE_HEALERS_10MAN;

    std::vector<ObjectGuid> healers;
    std::vector<ObjectGuid> others;
    for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* member = itr->GetSource();
        if (!member)
        {
            continue;
        }

        (botAI->IsHeal(member) ? healers : others).push_back(member->GetGUID());
    }

    // Guid order is identical on every bot, so the flight agrees on the roster without talking.
    std::sort(healers.begin(), healers.end());
    std::sort(others.begin(), others.end());

    for (size_t i = 0; i < wanted && i < healers.size(); ++i)
    {
        out.push_back(healers[i]);
    }

    if (out.size() >= wanted)
    {
        return;
    }

    size_t const shortfall = wanted - out.size();
    for (size_t i = 0; i < shortfall && i < others.size(); ++i)
    {
        out.push_back(others[i]);
    }
}

bool IsDrakeHealer(PlayerbotAI* botAI, std::vector<ObjectGuid> const& healers)
{
    return std::find(healers.begin(), healers.end(), botAI->GetBot()->GetGUID()) != healers.end();
}

void GetDrakeFlight(Player* bot, std::vector<Unit*>& drakes)
{
    drakes.clear();

    Group* group = bot->GetGroup();
    if (!group)
    {
        Unit* own = bot->GetVehicleBase();
        if (own && own->GetEntry() == NPC_WYRMREST_SKYTALON)
        {
            drakes.push_back(own);
        }
        return;
    }

    for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* member = itr->GetSource();
        if (!member)
        {
            continue;
        }

        Unit* drake = member->GetVehicleBase();
        if (!drake || !drake->IsAlive() || drake->GetEntry() != NPC_WYRMREST_SKYTALON)
        {
            continue;
        }

        drakes.push_back(drake);
    }
}

uint32 DrakeAuraRemainingMs(Unit* drake, uint32 spellId)
{
    if (!drake)
    {
        return 0;
    }

    Aura* aura = drake->GetAura(spellId);
    if (!aura)
    {
        return 0;
    }

    // Negative is a permanent aura, which none of these are.
    int32 const remaining = aura->GetDuration();
    return remaining > 0 ? static_cast<uint32>(remaining) : 0;
}

uint8 GetDrakeHealerRank(PlayerbotAI* botAI, std::vector<ObjectGuid> const& healers)
{
    Player* bot = botAI->GetBot();
    Unit* own = bot->GetVehicleBase();
    Group* group = bot->GetGroup();
    if (!own || !group)
    {
        return 0;
    }

    ObjectGuid const ownGuid = bot->GetGUID();
    uint32 const ownEnergy = own->GetPower(POWER_ENERGY);

    uint8 rank = 0;
    for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* member = itr->GetSource();
        if (!member || member->GetGUID() == ownGuid)
        {
            continue;
        }

        if (std::find(healers.begin(), healers.end(), member->GetGUID()) == healers.end())
        {
            continue;
        }

        Unit* drake = member->GetVehicleBase();
        if (!drake || !drake->IsAlive() || drake->GetEntry() != NPC_WYRMREST_SKYTALON)
        {
            continue;
        }

        uint32 const energy = drake->GetPower(POWER_ENERGY);
        if (energy > ownEnergy || (energy == ownEnergy && member->GetGUID() < ownGuid))
        {
            ++rank;
        }
    }
    return rank;
}

bool IsDrakeSurgeTarget(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Unit* drake = bot->GetVehicleBase();
    if (!drake)
    {
        return false;
    }

    Unit* boss = MalygosTrigger::getMalygos(bot);
    if (!boss)
    {
        return false;
    }

    Creature* bossCreature = boss->ToCreature();
    if (!bossCreature || !bossCreature->AI())
    {
        return false;
    }

    // Both P3 surges (57407, and 60936 in 25-man) are DoCastAOE with no unit target, so there is no
    // spell target to read - the boss publishes its victims in guid slots instead.
    for (uint8 i = 0; i < EOE_NUM_MAX_SURGE_TARGETS; ++i)
    {
        if (bossCreature->AI()->GetGUID(EOE_DATA_FIRST_SURGE_TARGET_GUID + i) == drake->GetGUID())
        {
            return true;
        }
    }
    return false;
}

namespace
{
// Both come back -1 when there is no power cost worth checking, which callers read as yes.
bool GetDrakeSpellCost(Unit* drake, uint32 spellId, int32& cost, int32& available)
{
    cost = -1;
    available = -1;

    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
    {
        return false;
    }

    // POWER_HEALTH is -2, which would index the power array out of bounds.
    if (spellInfo->PowerType >= static_cast<uint32>(MAX_POWERS))
    {
        return true;
    }

    int32 const needed = spellInfo->CalcPowerCost(drake, spellInfo->GetSchoolMask());
    if (needed <= 0)
    {
        return true;
    }

    cost = needed;
    available = static_cast<int32>(drake->GetPower(Powers(spellInfo->PowerType)));
    return true;
}
}

bool DrakeCanAfford(Unit* drake, uint32 spellId)
{
    int32 cost = 0;
    int32 available = 0;
    if (!GetDrakeSpellCost(drake, spellId, cost, available))
    {
        return false;
    }

    return cost < 0 || available >= cost;
}

bool DrakeCanAffordWithShield(Unit* drake, uint32 spellId)
{
    int32 cost = 0;
    int32 available = 0;
    if (!GetDrakeSpellCost(drake, spellId, cost, available))
    {
        return false;
    }
    if (cost < 0)
    {
        return true;
    }

    int32 shieldCost = 0;
    int32 shieldAvailable = 0;
    GetDrakeSpellCost(drake, SPELL_FLAME_SHIELD, shieldCost, shieldAvailable);

    return available >= cost + std::max(shieldCost, 0);
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

bool MalygosPositionAction::Execute(Event /*event*/)
{
    // Disk riders steer their vehicle instead; never drag them off it.
    if (bot->GetVehicle())
    {
        return false;
    }

    uint8 phase = MalygosTrigger::getPhase(bot);
    Unit* boss = MalygosTrigger::getMalygos(bot);

    // getPhase reports the pull intro as 4, the same value the real transitions carry. He is
    // untouchable throughout it, so full health is what tells them apart.
    bool const intro = phase == 4 && boss && boss->IsFullHealth();

    if (phase == 1 || intro)
    {
        // Whoever Malygos is chewing on behaves as the tank, assigned or not - but not in the intro,
        // where he is pacified and his victim is only whoever pulled.
        bool isBossTank = botAI->IsMainTank(bot) || (!intro && boss && boss->GetVictim() == bot);

        // This action owns the walking in both directions; PullPowerSparkAction only casts.
        bool const gripDuty = !isBossTank && IsOnPowerSparkGripDuty(botAI);
        MalygosP1Layout const& layout = GetMalygosP1Layout(bot);
        bool const hunter = botAI->IsRangedDps(bot) && bot->IsClass(CLASS_HUNTER);
        bool const onStack = !isBossTank && !gripDuty && !hunter;
        std::pair<float, float> const& spot = isBossTank ? layout.tank
                                              : gripDuty ? layout.grip
                                              : hunter   ? layout.hunter
                                                         : layout.stack;
        float const tolerance = gripDuty ? POWER_SPARK_GRIP_TOLERANCE : MALYGOS_P1_POSITION_TOLERANCE;

        float spotX = spot.first;
        float spotY = spot.second;

        // Slides continuously; a spot that snaps between two positions is what sets a raid bouncing.
        if (onStack && !intro && boss)
        {
            float const bossDist = boss->GetExactDist2d(spotX, spotY);
            if (bossDist > MALYGOS_MELEE_HOLD_DISTANCE)
            {
                float const bossX = boss->GetPositionX();
                float const bossY = boss->GetPositionY();
                spotX = bossX + (spotX - bossX) / bossDist * MALYGOS_MELEE_HOLD_DISTANCE;
                spotY = bossY + (spotY - bossY) / bossDist * MALYGOS_MELEE_HOLD_DISTANCE;
            }
        }

        if (bot->GetDistance2d(spotX, spotY) > tolerance)
        {
            return MoveTo(EOE_MAP_ID, spotX, spotY, bot->GetPositionZ(),
                false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
        }

        // Hand the tick back either way, or this outranks the rotation and the bot stops attacking.
        if (isBossTank && boss)
        {
            ServerFacade::instance().SetFacingTo(bot, boss);
        }
        return false;
    }
    else if (phase == 2 || phase == 4)
    {
        float const cx = MALYGOS_CENTER_POSITION.first;
        float const cy = MALYGOS_CENTER_POSITION.second;
        float const safeRadius = MALYGOS_ANTIFALL_RADIUS;

        float dist = bot->GetDistance2d(cx, cy);
        if (dist > safeRadius)
        {
            float target = safeRadius - MALYGOS_ANTIFALL_INSET;
            float tx = cx;
            float ty = cy;
            if (dist > 0.01f)
            {
                tx = cx + (bot->GetPositionX() - cx) / dist * target;
                ty = cy + (bot->GetPositionY() - cy) / dist * target;
            }
            return MoveTo(EOE_MAP_ID, tx, ty, bot->GetPositionZ(),
                false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
        }
        return false;
    }

    return false;
}

bool MalygosTargetAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "malygos");
    uint8 phase = MalygosTrigger::getPhase(bot);

    if (phase == 1)
    {
        if (botAI->IsHeal(bot))
        {
            return false;
        }
        if (!boss)
        {
            return false;
        }

        Unit* currentTarget = AI_VALUE(Unit*, "current target");

        // The tank never peels: dropping Malygos swings his Arcane Breath cone through the raid.
        Unit* newTarget = boss;
        bool const isBossTank = botAI->IsMainTank(bot) || boss->GetVictim() == bot;
        if (!isBossTank && botAI->IsDps(bot))
        {
            if (Unit* spark = GetPowerSparkToKill(botAI, currentTarget))
            {
                newTarget = spark;
            }
        }

        if (!currentTarget || currentTarget->GetGUID() != newTarget->GetGUID())
        {
            return Attack(newTarget);
        }
    }
    else if (phase == 2)
    {
        // Runs before the early returns below so a disk rider's ghoul is covered too.
        if (bot->GetGuardianPet())
        {
            if (Unit* petTarget = GetNearestEoECreature(bot, NPC_NEXUS_LORD))
            {
                CommandPetAttack(botAI, petTarget);
            }
            else
            {
                StopPet(botAI);
            }
        }

        if (botAI->IsHeal(bot))
        {
            return false;
        }

        if (bot->GetVehicle())
        {
            return false;
        }

        Unit* nexusLord = nullptr;
        Unit* scionOfEternity = nullptr;
        Unit* anyLord = nullptr;
        Unit* anyScion = nullptr;

        // IsWithinCombatRange, not a flat 2d distance: the Scions sit 20-30y up. Melee get no gate.
        bool const gateByReach = botAI->IsRanged(bot);
        float const reach = sPlayerbotAIConfig.spellDistance;
        float closestLord = std::numeric_limits<float>::max();
        float closestScion = std::numeric_limits<float>::max();
        float closestAnyLord = std::numeric_limits<float>::max();
        float closestAnyScion = std::numeric_limits<float>::max();

        GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
        for (auto& target : targets)
        {
            Unit* unit = botAI->GetUnit(target);
            if (!unit)
            {
                continue;
            }

            float dist = bot->GetExactDist(unit);
            bool inReach = !gateByReach || bot->IsWithinCombatRange(unit, reach);

            if (unit->GetEntry() == NPC_NEXUS_LORD)
            {
                if (dist < closestAnyLord)
                {
                    closestAnyLord = dist;
                    anyLord = unit;
                }
                if (inReach && dist < closestLord)
                {
                    closestLord = dist;
                    nexusLord = unit;
                }
            }
            else if (unit->GetEntry() == NPC_SCION_OF_ETERNITY)
            {
                if (dist < closestAnyScion)
                {
                    closestAnyScion = dist;
                    anyScion = unit;
                }
                if (inReach && dist < closestScion)
                {
                    closestScion = dist;
                    scionOfEternity = unit;
                }
            }
        }

        if (!nexusLord && !scionOfEternity)
        {
            nexusLord = anyLord;
            scionOfEternity = anyScion;
        }

        Unit* newTarget = nexusLord;
        if (!newTarget && botAI->IsRangedDps(bot))
        {
            newTarget = scionOfEternity;
        }
        if (!newTarget)
        {
            return false;
        }

        Unit* currentTarget = AI_VALUE(Unit*, "current target");

        // Swapped by GUID, not entry: entry alone welded bots to an add they could never reach.
        if (currentTarget && currentTarget->IsAlive() && currentTarget->GetEntry() == newTarget->GetEntry() &&
            (!gateByReach || bot->IsWithinCombatRange(currentTarget, reach)))
        {
            return false;
        }

        if (!currentTarget || currentTarget->GetGUID() != newTarget->GetGUID())
        {
            return Attack(newTarget);
        }
    }

    return false;
}

bool PullPowerSparkAction::isUseful()
{
    if (!IsOnPowerSparkGripDuty(botAI))
    {
        return false;
    }

    // Cast from the spot or not at all; the walk belongs to MalygosPositionAction.
    std::pair<float, float> const& grip = GetMalygosP1Layout(bot).grip;
    if (bot->GetDistance2d(grip.first, grip.second) > POWER_SPARK_GRIP_TOLERANCE)
    {
        return false;
    }

    Unit* spark = GetNearestPowerSpark(botAI);
    return spark && botAI->CanCastSpell("death grip", spark);
}

bool PullPowerSparkAction::Execute(Event /*event*/)
{
    Unit* spark = GetNearestPowerSpark(botAI);
    if (!spark)
    {
        return false;
    }

    return botAI->CastSpell("death grip", spark);
}

bool KillPowerSparkAction::isUseful()
{
    // Only a spark reachable standing still - nobody walks in P1. DK grips are PullPowerSparkAction.
    if (!botAI->IsDps(bot) || botAI->IsMainTank(bot))
    {
        return false;
    }

    Unit* boss = MalygosTrigger::getMalygos(bot);
    if (boss && boss->GetVictim() == bot)
    {
        return false;
    }

    return GetPowerSparkToKill(botAI, AI_VALUE(Unit*, "current target")) != nullptr;
}

bool KillPowerSparkAction::Execute(Event /*event*/)
{
    Unit* currentTarget = AI_VALUE(Unit*, "current target");

    Unit* spark = GetPowerSparkToKill(botAI, currentTarget);
    if (!spark)
    {
        return false;
    }

    if (!currentTarget || currentTarget->GetGUID() != spark->GetGUID())
    {
        return Attack(spark);
    }
    return false;
}

Unit* MalygosSpellstealAction::GetHastedLord()
{
    if (!bot->IsClass(CLASS_MAGE) || bot->GetVehicle())
    {
        return nullptr;
    }
    if (MalygosTrigger::getPhase(bot) != 2)
    {
        return nullptr;
    }

    Unit* best = nullptr;
    float closest = std::numeric_limits<float>::max();

    GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
    for (auto& target : targets)
    {
        Unit* unit = botAI->GetUnit(target);
        if (!unit || unit->GetEntry() != NPC_NEXUS_LORD || !unit->IsAlive())
        {
            continue;
        }
        if (!unit->HasAura(SPELL_HASTE))
        {
            continue;
        }

        if (!bot->IsWithinCombatRange(unit, sPlayerbotAIConfig.spellDistance))
        {
            continue;
        }

        float dist = bot->GetExactDist(unit);
        if (dist < closest)
        {
            closest = dist;
            best = unit;
        }
    }
    return best;
}

bool MalygosSpellstealAction::isUseful()
{
    Unit* lord = GetHastedLord();
    return lord && botAI->CanCastSpell("spellsteal", lord);
}

bool MalygosSpellstealAction::Execute(Event /*event*/)
{
    Unit* lord = GetHastedLord();
    if (!lord)
    {
        return false;
    }

    return botAI->CastSpell("spellsteal", lord);
}

bool MalygosSeekBubbleAction::Execute(Event /*event*/)
{
    if (bot->GetVehicle())
    {
        return false;
    }

    if (IsSafelySheltered(bot))
    {
        return false;
    }

    // Arcane Overload is non-attackable, so it never shows up in "possible targets".
    std::vector<Unit*> found;
    GetEoECreatures(bot, NPC_ARCANE_OVERLOAD, found);

    std::vector<Unit*> bubbles;
    for (Unit* bubble : found)
    {
        if (bot->GetExactDist2d(bubble) <= BUBBLE_SEARCH_RADIUS)
        {
            bubbles.push_back(bubble);
        }
    }
    if (bubbles.empty())
    {
        assignedBubbleGuid.Clear();
        return false;
    }

    std::sort(bubbles.begin(), bubbles.end(), [](Unit* a, Unit* b)
    {
        return a->GetGUID().GetRawValue() < b->GetGUID().GetRawValue();
    });

    std::vector<Unit*> usable;
    for (Unit* bubble : bubbles)
    {
        if (GetBubbleShrinkFactor(bubble) >= BUBBLE_MIN_USABLE_FACTOR)
        {
            usable.push_back(bubble);
        }
    }

    if (usable.empty())
    {
        usable.push_back(*std::max_element(bubbles.begin(), bubbles.end(), [](Unit* a, Unit* b)
        {
            return GetBubbleShrinkFactor(a) < GetBubbleShrinkFactor(b);
        }));
    }

    Unit* target = nullptr;
    if (!assignedBubbleGuid.IsEmpty())
    {
        for (Unit* bubble : usable)
        {
            if (bubble->GetGUID() == assignedBubbleGuid)
            {
                target = bubble;
                break;
            }
        }
    }

    if (!target)
    {
        uint32 index = static_cast<uint32>(std::max(0, botAI->GetGroupSlotIndex(bot)));
        target = usable[index % usable.size()];

        if (bot->GetExactDist2d(target) > BUBBLE_SEARCH_RADIUS / 2.0f)
        {
            for (Unit* bubble : usable)
            {
                if (bot->GetExactDist2d(bubble) < bot->GetExactDist2d(target))
                {
                    target = bubble;
                }
            }
        }
        assignedBubbleGuid = target->GetGUID();
    }

    return MoveTo(EOE_MAP_ID, target->GetPositionX(), target->GetPositionY(), bot->GetPositionZ(),
        false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
}

bool MalygosBoardDiskAction::Execute(Event /*event*/)
{
    Unit* disk = FindFreeHoverDisk(bot);
    if (!disk)
    {
        return false;
    }

    return EnterVehicle(disk, true);
}

bool MalygosRideDiskAction::isPossible()
{
    Unit* vehicleBase = bot->GetVehicleBase();
    return vehicleBase && vehicleBase->GetEntry() == NPC_HOVER_DISK;
}

bool MalygosRideDiskAction::Execute(Event /*event*/)
{
    Unit* disk = bot->GetVehicleBase();
    if (!disk)
    {
        return false;
    }

    Unit* scion = nullptr;
    float closestDist = std::numeric_limits<float>::max();
    std::vector<Unit*> scions;
    GetEoECreatures(bot, NPC_SCION_OF_ETERNITY, scions);
    for (Unit* candidate : scions)
    {
        float dist = disk->GetExactDist(candidate);
        if (dist < closestDist)
        {
            closestDist = dist;
            scion = candidate;
        }
    }

    MotionMaster* mm = disk->GetMotionMaster();

    // The core keeps the P2 summons until both add types are dead, so the ride is ended by hand.
    // Fly back down first; stepping off at Scion altitude is a long drop.
    if (!scion)
    {
        if (disk->GetPositionZ() > MALYGOS_PLATFORM_Z + 5.0f)
        {
            // Stamped over the approach in progress rather than waiting for POINT to clear.
            if (!descending || mm->GetCurrentMovementGeneratorType() != POINT_MOTION_TYPE)
            {
                mm->Clear(false);
                mm->MovePoint(0, MALYGOS_CENTER_POSITION.first, MALYGOS_CENTER_POSITION.second,
                              MALYGOS_PLATFORM_Z, FORCED_MOVEMENT_NONE, 0.f, 0.f,
                              /*generatePath*/ false, /*forceDestination*/ true);
                disk->SendMovementFlagUpdate();
                descending = true;
            }
            return true;
        }

        Vehicle* myVehicle = bot->GetVehicle();
        VehicleSeatEntry const* seat = myVehicle ? myVehicle->GetSeatForPassenger(bot) : nullptr;
        if (!seat || !seat->CanEnterOrExit())
        {
            return false;
        }

        WorldPacket p;
        bot->GetSession()->HandleRequestVehicleExit(p);
        return true;
    }

    descending = false;

    float const reach = DISK_APPROACH_REACH;

    if (closestDist > reach + DISK_APPROACH_TOLERANCE)
    {
        if (mm->GetCurrentMovementGeneratorType() == POINT_MOTION_TYPE)
        {
            return true;
        }

        float dx = disk->GetPositionX() - scion->GetPositionX();
        float dy = disk->GetPositionY() - scion->GetPositionY();
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 0.01f)
        {
            dx = std::cos(disk->GetOrientation());
            dy = std::sin(disk->GetOrientation());
            len = 1.0f;
        }

        float tx = scion->GetPositionX() + dx / len * reach;
        float ty = scion->GetPositionY() + dy / len * reach;

        // Straight 3d spline: a generated path is 2d and drops the destination onto the platform.
        mm->Clear(false);
        mm->MovePoint(0, tx, ty, scion->GetPositionZ(), FORCED_MOVEMENT_NONE, 0.f, 0.f,
                      /*generatePath*/ false, /*forceDestination*/ true);
        disk->SendMovementFlagUpdate();
        return true;
    }

    mm->MoveIdle();
    disk->SetFacingToObject(scion);

    Unit* currentTarget = AI_VALUE(Unit*, "current target");
    if (!currentTarget || currentTarget->GetGUID() != scion->GetGUID())
    {
        return Attack(scion);
    }

    return false;
}

bool AvoidSurgeOfPowerAction::Execute(Event /*event*/)
{
    // Riders and sheltered bots are already covered; peeling would only walk them out of it.
    if (bot->GetVehicle() || IsSafelySheltered(bot))
    {
        return false;
    }

    Unit* surge = GetNearestEoECreature(bot, NPC_SURGE_OF_POWER, EOE_SURGE_SEARCH_RADIUS);

    // The beam runs from Malygos through the surge focus; stepping off that line clears it.
    if (surge && bot->GetExactDist2d(surge) < SURGE_BEAM_CLEAR_DISTANCE)
    {
        return MoveAway(surge, SURGE_BEAM_SIDESTEP);
    }

    return false;
}

bool EoEFlyDrakeAction::isPossible()
{
    Unit* vehicleBase = bot->GetVehicleBase();
    return (vehicleBase && vehicleBase->GetEntry() == NPC_WYRMREST_SKYTALON);
}
bool EoEFlyDrakeAction::Execute(Event /*event*/)
{
    Unit* drake = bot->GetVehicleBase();
    if (!drake)
    {
        return false;
    }

    MotionMaster* mm = drake->GetMotionMaster();

    if (!stackCalcAtMs || GetMSTimeDiffToNow(stackCalcAtMs) >= DRAKE_STACK_RECALC_MS)
    {
        std::vector<Unit*> fields;
        GetStaticFields(bot, fields);
        stackValid = GetDrakeStackPoint(bot, fields, stackX, stackY, stackZ);
        stackCalcAtMs = getMSTime();
    }

    if (stackValid)
    {
        Unit* boss = MalygosTrigger::getMalygos(bot);

        if (drake->GetExactDist(stackX, stackY, stackZ) > DRAKE_STACK_TOLERANCE)
        {
            bool const sameSpot =
                issued && std::fabs(stackX - issuedX) < DRAKE_DESTINATION_EPSILON &&
                std::fabs(stackY - issuedY) < DRAKE_DESTINATION_EPSILON;

            // A finished leg takes the point generator with it, so not-POINT means the next leg is due.
            if (!sameSpot || mm->GetCurrentMovementGeneratorType() != POINT_MOTION_TYPE)
            {
                float legX, legY;
                GetDrakeApproachPoint(drake, boss, stackX, stackY, legX, legY);

                // Straight 3d spline, same reason as the hover disks.
                drake->SetCanFly(true);
                mm->Clear(false);
                mm->MovePoint(0, legX, legY, stackZ, FORCED_MOVEMENT_NONE, 0.f, 0.f,
                              /*generatePath*/ false, /*forceDestination*/ true);
                drake->SendMovementFlagUpdate();
                issuedX = stackX;
                issuedY = stackY;
                issued = true;
            }
            return true;
        }

        issued = false;
        if (mm->GetCurrentMovementGeneratorType() != IDLE_MOTION_TYPE)
        {
            mm->Clear(false);
            mm->MoveIdle();
            drake->SendMovementFlagUpdate();
        }

        // Everything a drake casts is aimed at the boss or itself, so healers can stay pinned too.
        if (boss)
        {
            if (!drake->HasInArc(CAST_ANGLE_IN_FRONT, boss))
            {
                drake->SetFacingToObject(boss);
            }
        }

        // Parked - hand the tick to the drake rotation.
        return false;
    }

    // No boss in reach yet: the flight is still forming up, so fan out behind the raid leader.
    Player* master = botAI->GetMaster();
    if (!master)
    {
        return false;
    }
    Unit* masterVehicle = master->GetVehicleBase();
    if (!masterVehicle)
    {
        return false;
    }

    if (drake->GetExactDist(masterVehicle) > DRAKE_FORMUP_RADIUS)
    {
        uint8 const numPlayers = bot->GetRaidDifficulty() == RAID_DIFFICULTY_25MAN_NORMAL ? 25 : 10;
        // 3/4 of a circle, with a 90 deg frontal cone left clear
        float const quarter = static_cast<float>(M_PI_2);
        float const angle = static_cast<float>(botAI->GetGroupSlotIndex(bot)) * (TWO_PI - quarter) /
                                static_cast<float>(numPlayers) + quarter;
        drake->SetCanFly(true);
        mm->MoveFollow(masterVehicle, DRAKE_FORMUP_SPREAD, angle);
        drake->SendMovementFlagUpdate();
        return true;
    }
    return false;
}

bool EoEDrakeAttackAction::isPossible()
{
    Unit* vehicleBase = bot->GetVehicleBase();
    return (vehicleBase && vehicleBase->GetEntry() == NPC_WYRMREST_SKYTALON);
}

bool EoEDrakeAttackAction::Execute(Event /*event*/)
{
    Unit* drake = bot->GetVehicleBase();
    if (!drake)
    {
        return false;
    }

    // Derived once: the roster walk sorts the whole group, and the heal branch reads it twice.
    std::vector<ObjectGuid> healers;
    GetDrakeHealerGuids(botAI, healers);

    // Healers only ever cast on their own drake, so they need no boss at all.
    if (IsDrakeHealer(botAI, healers))
    {
        return DrakeHealAction(drake, healers);
    }

    Unit* boss = AI_VALUE2(Unit*, "find target", "malygos");
    if (!boss)
    {
        GuidVector npcs = AI_VALUE(GuidVector, "possible targets");
        for (auto& npc : npcs)
        {
            Unit* unit = botAI->GetUnit(npc);
            if (!unit || unit->GetEntry() != NPC_MALYGOS)
            {
                continue;
            }

            boss = unit;
            break;
        }
    }
    if (!boss)
    {
        return false;
    }

    return DrakeDpsAction(drake, boss);
}

bool EoEDrakeAttackAction::CastDrakeSpellAction(Unit* drake, Unit* target, uint32 spellId)
{
    if (!botAI->CanCastVehicleSpell(spellId, target) || !botAI->CastVehicleSpell(spellId, target))
    {
        return false;
    }

    drake->AddSpellCooldown(spellId, 0, 0);
    return true;
}

bool EoEDrakeAttackAction::DrakeDpsAction(Unit* drake, Unit* target)
{
    // Closing the gap belongs to EoEFlyDrakeAction alone: two owners fight over the destination,
    // and MoveForwards has no answer for a point in mid-air.
    if (drake->GetExactDist(target) > DRAKE_ATTACK_RANGE)
    {
        return false;
    }

    uint8 comboPoints = drake->GetComboPoints(target);

    if (IsDrakeSurgeTarget(botAI))
    {
        // Fixated: spend the bank while the bar still covers the shield, rebuild, then let it climb.
        if (comboPoints >= DRAKE_ENGULF_COMBO && DrakeCanAffordWithShield(drake, SPELL_ENGULF_IN_FLAMES))
        {
            return CastDrakeSpellAction(drake, target, SPELL_ENGULF_IN_FLAMES);
        }

        // At zero the shield has nothing to spend, so that point is worth going under the reserve.
        if (comboPoints < DRAKE_SHIELD_RESERVE_COMBO &&
            (!comboPoints || DrakeCanAffordWithShield(drake, SPELL_FLAME_SPIKE)))
        {
            return CastDrakeSpellAction(drake, target, SPELL_FLAME_SPIKE);
        }

        return false;
    }

    if (comboPoints >= DRAKE_ENGULF_COMBO)
    {
        return CastDrakeSpellAction(drake, target, SPELL_ENGULF_IN_FLAMES);
    }
    else
    {
        return CastDrakeSpellAction(drake, target, SPELL_FLAME_SPIKE);
    }
}

bool EoEDrakeAttackAction::DrakeHealAction(Unit* drake, std::vector<ObjectGuid> const& healers)
{
    // Both stay on our own drake: combo points are held for one target at a time, so chasing the
    // lowest resets the count. DrakeCanAfford stands in for the BAD_TARGETS-reporting check.
    if (IsDrakeSurgeTarget(botAI))
    {
        uint8 const comboPoints = drake->GetComboPoints();
        if (comboPoints >= DRAKE_LIFE_BURST_COMBO && DrakeCanAffordWithShield(drake, SPELL_LIFE_BURST))
        {
            return botAI->CastVehicleSpell(SPELL_LIFE_BURST, drake);
        }

        if (comboPoints < DRAKE_SHIELD_RESERVE_COMBO &&
            (!comboPoints || DrakeCanAffordWithShield(drake, SPELL_REVIVIFY)))
        {
            return botAI->CastVehicleSpell(SPELL_REVIVIFY, drake);
        }

        return false;
    }

    if (drake->GetComboPoints() < DRAKE_LIFE_BURST_COMBO)
    {
        if (!DrakeCanAfford(drake, SPELL_REVIVIFY))
        {
            return false;
        }

        return botAI->CastVehicleSpell(SPELL_REVIVIFY, drake);
    }

    bool const canBurst = DrakeCanAfford(drake, SPELL_LIFE_BURST);

    // Life Burst is a flat heal, so the worst drake decides this, not the raid-wide total.
    std::vector<Unit*> flight;
    GetDrakeFlight(bot, flight);

    uint8 worstPct = 100;
    uint32 worstMissing = 0;
    for (Unit* other : flight)
    {
        uint32 const max = other->GetMaxHealth();
        if (!max)
        {
            continue;
        }

        uint8 const pct = static_cast<uint8>(other->GetHealth() * 100 / max);
        if (pct < worstPct)
        {
            worstPct = pct;
            worstMissing = max - other->GetHealth();
        }
    }

    if (canBurst && worstPct <= DRAKE_BURST_EMERGENCY_PCT)
    {
        return botAI->CastVehicleSpell(SPELL_LIFE_BURST, drake);
    }

    // The buff Life Burst leaves on its caster doubles as a record of who burst and when - true
    // for real players too, and a cast that quietly failed leaves no trace to mislead the rest.
    bool recentBurst = false;
    for (Unit* other : flight)
    {
        if (other == drake)
        {
            continue;
        }

        if (DrakeAuraRemainingMs(other, SPELL_LIFE_BURST) > DRAKE_LIFE_BURST_BUFF_MS - DRAKE_BURST_STAGGER_MS)
        {
            recentBurst = true;
            break;
        }
    }

    if (canBurst && !recentBurst)
    {
        if (DrakeAuraRemainingMs(drake, SPELL_LIFE_BURST) < DRAKE_LIFE_BURST_REFRESH_MS)
        {
            return botAI->CastVehicleSpell(SPELL_LIFE_BURST, drake);
        }

        if (worstPct <= DRAKE_BURST_HEALTH_PCT)
        {
            uint32 const wanted = (worstMissing + DRAKE_LIFE_BURST_HEAL - 1) / DRAKE_LIFE_BURST_HEAL;
            if (GetDrakeHealerRank(botAI, healers) < wanted)
            {
                return botAI->CastVehicleSpell(SPELL_LIFE_BURST, drake);
            }
        }
    }

    // Revivify costs exactly one global's regen, so without this floor the bar never reaches 50.
    if (drake->GetPower(POWER_ENERGY) >= DRAKE_HOLD_ENERGY_FLOOR)
    {
        return botAI->CastVehicleSpell(SPELL_REVIVIFY, drake);
    }

    return false;
}

bool DrakeSurgeShieldAction::isPossible()
{
    Unit* vehicleBase = bot->GetVehicleBase();
    return (vehicleBase && vehicleBase->GetEntry() == NPC_WYRMREST_SKYTALON);
}

bool DrakeSurgeShieldAction::Execute(Event /*event*/)
{
    Unit* drake = bot->GetVehicleBase();
    if (!drake)
    {
        return false;
    }

    if (drake->HasAura(SPELL_FLAME_SHIELD))
    {
        return false;
    }

    uint32 const now = getMSTime();
    if (!lastSeenMs || getMSTimeDiff(lastSeenMs, now) > DRAKE_FIXATE_GAP_MS)
    {
        fixateAtMs = now;
    }
    lastSeenMs = now;

    uint32 const elapsed = getMSTimeDiff(fixateAtMs, now);
    uint8 const comboPoints = drake->GetComboPoints();

    // A finisher on an empty bank is SPELL_FAILED_NO_COMBO_POINTS, reported as a success.
    if (!comboPoints)
    {
        return false;
    }

    // Held until the cover still reaches the end of the beam; at the fixate it expires early.
    uint32 const cover = DRAKE_SHIELD_BASE_MS + comboPoints * DRAKE_SHIELD_MS_PER_COMBO;
    uint32 const castAt = std::min(cover >= SURGE_BEAM_END_MS ? 0u : SURGE_BEAM_END_MS - cover, SURGE_BEAM_DELAY_MS);

    if (elapsed < castAt)
    {
        return false;
    }

    // A big bank is worth more as a finisher, so yield - until the beam is actually landing.
    if (comboPoints > DRAKE_SHIELD_MAX_COMBO && elapsed < SURGE_BEAM_DELAY_MS)
    {
        return false;
    }

    if (!DrakeCanAfford(drake, SPELL_FLAME_SHIELD))
    {
        return false;
    }

    // Safe mid-dodge: self-cast, so CastVehicleSpell skips the turn-the-vehicle and stop-moving
    // branches, and it is instant. Forced, because CanCastVehicleSpell reports BAD_TARGETS.
    botAI->CastVehicleSpell(SPELL_FLAME_SHIELD, drake);

    // Hand the tick on either way, or a Static Field dodge below this one stops halfway.
    return false;
}
