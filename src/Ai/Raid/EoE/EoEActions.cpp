/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "EoEActions.h"
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
    // Seeded with the range a bot is willing to walk, so anything further out never wins.
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

bool GetDrakeStackPoint(Player* bot, std::vector<Unit*> const& fields, float& x, float& y, float& z)
{
    if (!bot->GetVehicleBase()) { return false; }

    Unit* boss = MalygosTrigger::getMalygos(bot);
    if (!boss) { return false; }

    float const bossX = boss->GetPositionX();
    float const bossY = boss->GetPositionY();
    z = MALYGOS_P3_BOSS_Z;

    x = bossX + std::cos(DRAKE_STACK_ANGLE) * DRAKE_STACK_RADIUS;
    y = bossY + std::sin(DRAKE_STACK_ANGLE) * DRAKE_STACK_RADIUS;

    if (IsClearOfStaticFields(x, y, fields, STATIC_FIELD_CLEARANCE)) { return true; }

    // A field has landed on the stack, so the flight slides off together rather than scattering.
    // It slides *around* the boss, never across him or away from him: the replacement is another
    // point on the same ring, so the dodge cannot walk the flight into Arcane Pulse or out of
    // DRAKE_ATTACK_RANGE. An earlier version hopped a straight STATIC_FIELD_SAFE_RADIUS off the
    // anchor in whichever direction had the most clearance, and boss-ward was often that direction.
    // Headings are always walked the same way around the ring, and the first one that clears wins.
    // Sweeping both ways found a nearer spot, but when the next field landed on it the answer often
    // flipped to the far side of the anchor - straight back over the field the flight had just left.
    // Going one way only, a second field can only ever push the flight further along.
    uint8 const headings = 24;
    float const step = 2.0f * static_cast<float>(M_PI) / headings;

    float bestClearance = 0.0f;
    float bestX = x;
    float bestY = y;

    for (uint8 i = 1; i <= headings; ++i)
    {
        float const angle = DRAKE_STACK_ANGLE + static_cast<float>(i) * step;
        float const cx = bossX + std::cos(angle) * DRAKE_STACK_RADIUS;
        float const cy = bossY + std::sin(angle) * DRAKE_STACK_RADIUS;

        float clearance = std::numeric_limits<float>::max();
        for (Unit* field : fields)
        {
            clearance = std::min(clearance, field->GetExactDist2d(cx, cy));
        }

        if (clearance >= STATIC_FIELD_CLEARANCE)
        {
            x = cx;
            y = cy;
            return true;
        }

        if (clearance > bestClearance)
        {
            bestClearance = clearance;
            bestX = cx;
            bestY = cy;
        }
    }

    // Every heading is covered. Take the roomiest one anyway rather than sit in the field - still
    // on the ring, and the fields expire on their own in 20s.
    x = bestX;
    y = bestY;
    return true;
}

namespace
{
struct LayoutCacheEntry
{
    bool latched = false;
    MalygosP1Layout layout;
};

// Keyed on the instance, like the phase and creature caches in EoETriggers.cpp, and thread_local for
// the same reason: a bot is only ever updated from its own map's thread.
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

    // Out of combat the encounter is either not started or reset, so the next pull gets a fresh
    // bearing. Nothing reads the layout in that state anyway.
    uint8 const phase = MalygosTrigger::getPhase(bot);
    if (phase == 0) { cached.latched = false; }
    else if (cached.latched) { return cached.layout; }

    // Malygos is already committed to his landing bearing by the time bots see the intro:
    // JustEngagedWith puts him in combat and schedules EVENT_INTRO_MOVE_CENTER in the same instant,
    // and that snapshots the angle and flies him straight in along it. So the first resolve of the
    // pull is the right one, and latching it stops the layout drifting as he chases the tank.
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
                // Wrapped into [-pi, pi] so a bearing either side of the seam still picks its
                // neighbour rather than the one three quarters of the way round.
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
        if (!unit || unit->GetEntry() != NPC_POWER_SPARK) { continue; }

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
    if (sparks.empty()) { return nullptr; }

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
            // Melee only ever swing at a spark that has walked into them on its way to the boss, so
            // the one they already have gets a couple of yards of grace before they drop it.
            inReach = bot->IsWithinMeleeRange(spark, currentTarget == spark ? POWER_SPARK_MELEE_STICKY : 0.0f);
        }
        if (!inReach) { continue; }

        // Whichever is closest to handing over its buff.
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
    if (!bot->IsClass(CLASS_DEATH_KNIGHT) || bot->GetVehicle()) { return false; }

    // Walking out costs boss uptime and the grip's cooldown outlasts the gap between spawns, so only
    // leave when the pull is actually available.
    uint32 const gripId = botAI->GetAiObjectContext()->GetValue<uint32>("spell id", "death grip")->Get();
    if (!gripId || !bot->HasSpell(gripId) || bot->HasSpellCooldown(gripId)) { return false; }

    // The spot only works while the tank has Malygos where he belongs. If he has drifted onto the
    // raid, dropping a spark on the raid drops it on him too.
    Unit* boss = MalygosTrigger::getMalygos(bot);
    if (!boss) { return false; }

    std::pair<float, float> const& grip = GetMalygosP1Layout(bot).grip;
    if (boss->GetExactDist2d(grip.first, grip.second) < POWER_SPARK_GRIP_SAFE_BOSS_DISTANCE)
    {
        return false;
    }

    Unit* spark = GetNearestPowerSpark(botAI);
    return spark && spark->GetExactDist2d(grip.first, grip.second) <= POWER_SPARK_GRIP_ENGAGE_RADIUS;
}

bool IsDrakeHealer(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    Group* group = bot->GetGroup();
    if (!group) { return botAI->IsHeal(bot); }

    uint8 const wanted =
        bot->GetRaidDifficulty() == RAID_DIFFICULTY_25MAN_NORMAL ? DRAKE_HEALERS_25MAN : DRAKE_HEALERS_10MAN;

    std::vector<ObjectGuid> healers;
    std::vector<ObjectGuid> others;
    for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
    {
        Player* member = itr->GetSource();
        if (!member) { continue; }

        (botAI->IsHeal(member) ? healers : others).push_back(member->GetGUID());
    }

    // Guid order is identical on every bot, so the whole flight agrees on the roster without talking.
    std::sort(healers.begin(), healers.end());
    std::sort(others.begin(), others.end());

    // Every drake carries the same spellbook, so this is a raid-size call rather than a spec one: cap
    // the healers when the raid brought more than the flight needs, top up from the dps when it
    // brought fewer. A raid stacked with healers used to put all of them on Revivify and the boss
    // outlasted the phase.
    for (size_t i = 0; i < wanted && i < healers.size(); ++i)
    {
        if (healers[i] == bot->GetGUID()) { return true; }
    }

    if (healers.size() >= wanted) { return false; }

    size_t const shortfall = wanted - healers.size();
    for (size_t i = 0; i < shortfall && i < others.size(); ++i)
    {
        if (others[i] == bot->GetGUID()) { return true; }
    }
    return false;
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

    // Only the periodic effect counts ticks; the others sit at 0, so the max is the one we want.
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

    // The bot stands at its bubble's centre, so the nearest one is the one covering it.
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

    // Malygos flies an intro circuit before he touches down, and getPhase reports that as 4 - the
    // same value the P1->P2 and P2->P3 gaps carry. He is untouchable for all of it, so full health
    // is what tells the intro apart from those two. Holding the phase 1 spots through it puts the
    // tank on his spot before the boss lands, instead of the centre gather dragging him off it and
    // leaving him to walk back out through the raid once the fight is already running.
    bool const intro = phase == 4 && boss && boss->IsFullHealth();

    if (phase == 1 || intro)
    {
        // Whoever Malygos is actually chewing on has to behave like the tank, even if the raid
        // never assigned one - anyone else walking away would drag the boss and swing his cone.
        // Not during the intro: he is pacified there and his victim is only whoever pulled.
        bool isBossTank = botAI->IsMainTank(bot) || (!intro && boss && boss->GetVictim() == bot);

        // Hunters hold their own spot well back: anything closer is inside Malygos' effective minimum
        // range, which his CombatReach inflates to ~28y. Nothing else has one, so the rest of the
        // ranged dps join the melee and the healers on the stack - out there they could not reach a
        // Power Spark closing on the boss from the far side, and the stack keeps the tank inside heal
        // range. A DK on spark duty steps out to the grip spot and comes back here once the grip is
        // spent - this action owns the walking in both directions, PullPowerSparkAction only casts.
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

        // The layout assumes Malygos stops ~21.5y short of the tank spot on the bearing he landed on.
        // He does not always: his chase stops wherever it first brings him inside melee range of the
        // tank, so coming in off-bearing can leave him far enough out that the melee half of the raid
        // stands on the stack with nothing in reach. Pull the stack up the line towards him, and only
        // as far as it takes to swing. It keeps the bearing the stack already holds from him, so it
        // can never end up in front of him, and it slides with him instead of snapping between two
        // spots - a spot that jumps is what sets a raid bouncing. Not during the intro: he is
        // circling and untouchable.
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

        // The tank and hunter spots stay put for the whole pull. The layout is picked once, off the
        // bearing Malygos landed on, and never recomputed from where he happens to be standing: a
        // spot that chases him flips to his far side whenever he is still on his way out, and the
        // tank then ping-pongs between the edge and the middle, sweeping the cone through the raid.
        if (bot->GetDistance2d(spotX, spotY) > tolerance)
        {
            return MoveTo(EOE_MAP_ID, spotX, spotY, bot->GetPositionZ(),
                false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
        }

        // Parked. Keep the tank pointed at Malygos so he holds still and keeps facing north, and
        // hand the tick back either way - this action outranks the class rotation, so owning the
        // tick here would stop the bot attacking.
        if (isBossTank && boss)
        {
            ServerFacade::instance().SetFacingTo(bot, boss);
        }
        return false;
    }
    else if (phase == 2 || phase == 4)
    {
        // Anti-fall: the platform edge drops into the void. Keep everyone inside a safe
        // interior ring around the centre; also gathers the raid for the P3 drake mount.
        float const cx = MALYGOS_CENTER_POSITION.first;
        float const cy = MALYGOS_CENTER_POSITION.second;
        float const safeRadius = 30.0f;

        float dist = bot->GetDistance2d(cx, cy);
        if (dist > safeRadius)
        {
            float target = safeRadius - 3.0f;
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
        if (botAI->IsHeal(bot)) { return false; }
        if (!boss) { return false; }

        Unit* currentTarget = AI_VALUE(Unit*, "current target");

        // Any dps peels onto a spark it can actually hit from where it stands - melee get the ones
        // that walk into them on the way to the boss. The tank never does: dropping Malygos swings
        // his Arcane Breath cone through whoever is behind him.
        Unit* newTarget = boss;
        bool const isBossTank = botAI->IsMainTank(bot) || boss->GetVictim() == bot;
        if (!isBossTank && botAI->IsDps(bot))
        {
            if (Unit* spark = GetPowerSparkToKill(botAI, currentTarget)) { newTarget = spark; }
        }

        if (!currentTarget || currentTarget->GetGUID() != newTarget->GetGUID())
        {
            return Attack(newTarget);
        }
    }
    else if (phase == 2)
    {
        // Scions hover 20-30y up where no pet can follow, so pets stay on the grounded Nexus Lords
        // whatever their owner is shooting at. Runs before the early returns below so a disk rider's
        // ghoul is covered too; CommandPetAttack no-ops once the pet is already on that target.
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

        if (botAI->IsHeal(bot)) { return false; }

        // Disk riders pick their own Scion in MalygosRideDiskAction.
        if (bot->GetVehicle()) { return false; }

        Unit* nexusLord = nullptr;
        Unit* scionOfEternity = nullptr;
        Unit* anyLord = nullptr;
        Unit* anyScion = nullptr;

        // Ranged hold their bubble, so they prefer what can be hit from where they already stand -
        // picking something out of reach just makes them walk out of shelter. The gate is
        // IsWithinCombatRange, the same 3d combat-reach test Spell::CheckRange uses, because the
        // Scions sit 20-30y above the platform and a flat 2d distance claims a reach that isn't
        // there. Melee are still expected to walk to the Nexus Lords, so they get no distance gate.
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
            if (!unit) { continue; }

            float dist = bot->GetExactDist(unit);
            bool inReach = !gateByReach || bot->IsWithinCombatRange(unit, reach);

            if (unit->GetEntry() == NPC_NEXUS_LORD)
            {
                if (dist < closestAnyLord) { closestAnyLord = dist; anyLord = unit; }
                if (inReach && dist < closestLord) { closestLord = dist; nexusLord = unit; }
            }
            else if (unit->GetEntry() == NPC_SCION_OF_ETERNITY)
            {
                if (dist < closestAnyScion) { closestAnyScion = dist; anyScion = unit; }
                if (inReach && dist < closestScion) { closestScion = dist; scionOfEternity = unit; }
            }
        }

        // Nothing in reach at all: fall back to the nearest add anyway. Standing there with no
        // target means the class rotation never fires once something does drift into range.
        if (!nexusLord && !scionOfEternity)
        {
            nexusLord = anyLord;
            scionOfEternity = anyScion;
        }

        // Nexus Lords land, can be tanked and die faster, so they come first for everyone. Scions
        // never land, which leaves them to ranged dps once no Lord is in reach.
        Unit* newTarget = nexusLord;
        if (!newTarget && botAI->IsRangedDps(bot)) { newTarget = scionOfEternity; }
        if (!newTarget) { return false; }

        Unit* currentTarget = AI_VALUE(Unit*, "current target");

        // Hold a live add of the right kind that is still in reach, so two equidistant Scions can't
        // make the bot flip between them every tick. Anything else gets swapped by GUID - matching
        // on entry alone left bots welded to an add they could never get to.
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
    if (!IsOnPowerSparkGripDuty(botAI)) { return false; }

    // Cast from the spot or not at all. Death Grip lands the spark on the caster, so standing in the
    // wrong place is worse than not gripping: the corpse's ground buff would land out of everyone's
    // way, or the spark itself next to Malygos. Getting there is MalygosPositionAction's job, and
    // this returns false while the walk is still going so it keeps the tick.
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
    if (!spark) { return false; }

    return botAI->CastSpell("death grip", spark);
}

bool KillPowerSparkAction::isUseful()
{
    // Any dps peels onto a spark, but only one it can reach standing still - nobody walks in P1, and
    // a bot holding a target it can't touch is a bot doing nothing. The tank stays on Malygos so his
    // threat and Arcane Breath cone don't swing into the raid, and DK grips are PullPowerSparkAction.
    if (!botAI->IsDps(bot) || botAI->IsMainTank(bot)) { return false; }

    Unit* boss = MalygosTrigger::getMalygos(bot);
    if (boss && boss->GetVictim() == bot) { return false; }

    return GetPowerSparkToKill(botAI, AI_VALUE(Unit*, "current target")) != nullptr;
}

bool KillPowerSparkAction::Execute(Event /*event*/)
{
    Unit* currentTarget = AI_VALUE(Unit*, "current target");

    Unit* spark = GetPowerSparkToKill(botAI, currentTarget);
    if (!spark) { return false; }

    if (!currentTarget || currentTarget->GetGUID() != spark->GetGUID())
    {
        return Attack(spark);
    }
    return false;
}

Unit* MalygosSpellstealAction::GetHastedLord()
{
    if (!bot->IsClass(CLASS_MAGE) || bot->GetVehicle()) { return nullptr; }
    if (MalygosTrigger::getPhase(bot) != 2) { return nullptr; }

    Unit* best = nullptr;
    float closest = std::numeric_limits<float>::max();

    GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
    for (auto& target : targets)
    {
        Unit* unit = botAI->GetUnit(target);
        if (!unit || unit->GetEntry() != NPC_NEXUS_LORD || !unit->IsAlive()) { continue; }
        if (!unit->HasAura(SPELL_HASTE)) { continue; }

        // Staying in the bubble beats stealing, so only a Lord already in range counts.
        if (!bot->IsWithinCombatRange(unit, sPlayerbotAIConfig.spellDistance)) { continue; }

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
    if (!lord) { return false; }

    return botAI->CastSpell("spellsteal", lord);
}

bool MalygosSeekBubbleAction::Execute(Event /*event*/)
{
    if (bot->GetVehicle())
    {
        return false;
    }

    // Sheltered in a bubble with life left - hand the tick back so the dps/heal rotation runs.
    if (IsSafelySheltered(bot))
    {
        return false;
    }

    // Arcane Overload is non-attackable, so it never shows up in "possible targets"; the shared
    // creature cache is what stands in for a per-bot grid sweep.
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

    // Bubbles shrink from the moment they land, so an old one is not worth walking to even though
    // its model still looks full size.
    std::vector<Unit*> usable;
    for (Unit* bubble : bubbles)
    {
        if (GetBubbleShrinkFactor(bubble) >= BUBBLE_MIN_USABLE_FACTOR)
        {
            usable.push_back(bubble);
        }
    }

    // Every bubble is nearly spent: take the freshest anyway rather than stand in the open.
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
        // Drop the latch once the assigned bubble ages out, so the bot re-picks a fresh one.
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
        // Spread the raid over the bubbles that are up rather than piling everyone on the nearest.
        uint32 index = static_cast<uint32>(std::max(0, botAI->GetGroupSlotIndex(bot)));
        target = usable[index % usable.size()];

        // ...unless the round-robin pick is across the room, in which case survival beats spreading.
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

    // Hug the centre: the protected radius shrinks about 2% per tick over the bubble's 45s life.
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

    // Scions can die before the Nexus Lords do, and the core only despawns the P2 summons once both
    // are gone - so the ride has to be ended by hand or the bot idles in the air until the phase
    // ends. Fly back down over the platform first; stepping off 20-30y up is a long drop.
    if (!scion)
    {
        if (disk->GetPositionZ() > MALYGOS_PLATFORM_Z + 5.0f)
        {
            // The disk is probably still running the approach to the Scion that just died, so the
            // descent has to be stamped over it once rather than waiting for POINT to clear.
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

        // The core lands the disk itself once the passenger is gone.
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

    float const reach = 3.0f;

    if (closestDist > reach + 2.0f)
    {
        // Let an in-progress approach finish instead of restamping the destination every tick.
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

        // Straight 3d spline, never a terrain path: the mmap is 2d, so a generated path drops the
        // destination onto the platform and the disk dives 25y instead of flying to the Scion.
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

    // In range and already on it - let the class rotation take the tick.
    return false;
}

bool AvoidSurgeOfPowerAction::Execute(Event /*event*/)
{
    // Disk riders are immune to the surge, and a bubble already halves it - walking either of them
    // out of cover is strictly worse than eating the beam, and the bubbles sit close enough to the
    // centre that MoveAway would do exactly that for the beam's whole 10s life.
    if (bot->GetVehicle() || IsSafelySheltered(bot))
    {
        return false;
    }

    // Surge of Power's focus unit is non-attackable too; find it directly like SurgeOfPowerTrigger.
    Unit* surge = GetNearestEoECreature(bot, NPC_SURGE_OF_POWER, 100.0f);

    // The beam runs from Malygos through the surge focus; stepping off that line clears it.
    if (surge && bot->GetExactDist2d(surge) < 12.0f)
    {
        return MoveAway(surge, 6.0f);
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
    if (!drake) { return false; }

    MotionMaster* mm = drake->GetMotionMaster();

    // Resolving the stack point costs a grid sweep for the Static Fields. It only moves when one
    // lands on it and they land 12s apart, so the answer keeps for a second.
    if (!stackCalcAtMs || GetMSTimeDiffToNow(stackCalcAtMs) >= DRAKE_STACK_RECALC_MS)
    {
        std::vector<Unit*> fields;
        GetStaticFields(bot, fields);
        stackValid = GetDrakeStackPoint(bot, fields, stackX, stackY, stackZ);
        stackCalcAtMs = getMSTime();
    }

    // The whole flight parks on that one point. Trailing the raid leader left every drake
    // permanently in motion, and a moving vehicle can neither finish a cast nor hold a facing -
    // which is why the flight used to circle the boss without ever firing at him.
    if (stackValid)
    {
        if (drake->GetExactDist(stackX, stackY, stackZ) > DRAKE_STACK_TOLERANCE)
        {
            bool const sameSpot =
                issued && std::fabs(stackX - issuedX) < 2.0f && std::fabs(stackY - issuedY) < 2.0f;

            if (!sameSpot || mm->GetCurrentMovementGeneratorType() != POINT_MOTION_TYPE)
            {
                // Straight 3d spline for the same reason the hover disks use one: a generated path
                // is 2d and drops the destination onto whatever is underneath.
                drake->SetCanFly(true);
                mm->Clear(false);
                mm->MovePoint(0, stackX, stackY, stackZ, FORCED_MOVEMENT_NONE, 0.f, 0.f,
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

        // Everything a drake casts is aimed either at the boss or at itself, so it can stay pinned
        // on him - a heal never needs the vehicle turned.
        if (Unit* boss = MalygosTrigger::getMalygos(bot))
        {
            if (!drake->HasInArc(CAST_ANGLE_IN_FRONT, boss)) { drake->SetFacingToObject(boss); }
        }

        // Parked and pointed the right way - hand the tick to the drake rotation.
        return false;
    }

    // No boss in reach yet: the flight is still forming up, so fan out behind the raid leader.
    Player* master = botAI->GetMaster();
    if (!master) { return false; }
    Unit* masterVehicle = master->GetVehicleBase();
    if (!masterVehicle) { return false; }

    if (drake->GetExactDist(masterVehicle) > 20.0f)
    {
        uint8 const numPlayers = bot->GetRaidDifficulty() == RAID_DIFFICULTY_25MAN_NORMAL ? 25 : 10;
        // 3/4 of a circle, with a 90 deg frontal cone left clear
        float angle = botAI->GetGroupSlotIndex(bot) * (2*M_PI - M_PI_2)/numPlayers + M_PI_2;
        drake->SetCanFly(true);
        mm->MoveFollow(masterVehicle, 15.0f, angle);
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
    vehicleBase = bot->GetVehicleBase();
    if (!vehicleBase)
    {
        return false;
    }

    // Healers only ever cast on their own drake, so they need no boss resolved at all.
    if (IsDrakeHealer(botAI))
    {
        return DrakeHealAction();
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

    return DrakeDpsAction(boss);
}

bool EoEDrakeAttackAction::CastDrakeSpellAction(Unit* target, uint32 spellId, uint32 cooldown)
{
    if (botAI->CanCastVehicleSpell(spellId, target))
        if (botAI->CastVehicleSpell(spellId, target))
        {
            vehicleBase->AddSpellCooldown(spellId, 0, cooldown);
            return true;
        }
    return false;
}

bool EoEDrakeAttackAction::DrakeDpsAction(Unit* target)
{
    Unit* drake = bot->GetVehicleBase();
    if (!drake) { return false; }

    // Closing the gap belongs to EoEFlyDrakeAction and nothing else. Steering the same vehicle from
    // here as well left the two fighting over the destination, and MoveForwards runs its endpoint
    // through CanReachPositionAndGetValidCoords, which has no answer for a point in mid-air - so the
    // drake stopped dead and stayed out of range for good.
    if (drake->GetExactDist(target) > DRAKE_ATTACK_RANGE) { return false; }

    uint8 comboPoints = drake->GetComboPoints(target);
    if (comboPoints >= DRAKE_ENGULF_COMBO)
    {
        return CastDrakeSpellAction(target, SPELL_ENGULF_IN_FLAMES, 0);
    }
    else
    {
        return CastDrakeSpellAction(target, SPELL_FLAME_SPIKE, 0);
    }
}

bool EoEDrakeAttackAction::DrakeHealAction()
{
    Unit* drake = bot->GetVehicleBase();
    if (!drake)
    {
        return false;
    }

    // Revivify is a heal over time and every cast banks a combo point; Life Burst spends five of
    // them as a heal centred on the caster that reaches the whole flight. Both stay on our own
    // drake. A unit holds combo points for one target at a time, so chasing whoever is lowest
    // resets the count to one every time it switches and the finisher never comes up - and with the
    // flight stacked, a self-cast Life Burst covers the same drakes anyway.
    // CanCastVehicleSpell reports BAD_TARGETS on a drake, so both go out unchecked.
    if (drake->GetComboPoints() >= DRAKE_LIFE_BURST_COMBO)
    {
        return botAI->CastVehicleSpell(SPELL_LIFE_BURST, drake);
    }

    return botAI->CastVehicleSpell(SPELL_REVIVIFY, drake);
}

bool DrakeSurgeShieldAction::isPossible()
{
    Unit* vehicleBase = bot->GetVehicleBase();
    return (vehicleBase && vehicleBase->GetEntry() == NPC_WYRMREST_SKYTALON);
}

bool DrakeSurgeShieldAction::Execute(Event /*event*/)
{
    Unit* drake = bot->GetVehicleBase();
    if (!drake) { return false; }

    // There is nothing to dodge: the boss fires Surge of Power as a triggered instant 3s after the
    // fixate, so no amount of flying gets the drake out of it. Halving it with Flame Shield is the
    // whole reaction, and the trigger reads the fixate, so the shield goes up with the full 3s
    // still to run. Anything it does not cover is left to the healers.
    if (drake->HasSpellCooldown(SPELL_FLAME_SHIELD)) { return false; }

    // Safe to fire mid-dodge: the shield is a self-cast, so CastVehicleSpell skips both the branch
    // that turns the vehicle onto a target and the one that stops it dead, and it is instant.
    // Same as the drake heals: CanCastVehicleSpell reports BAD_TARGETS on a drake, so force it.
    botAI->CastVehicleSpell(SPELL_FLAME_SHIELD, drake);

    // Only cool it down once the aura is actually up. CastVehicleSpell reports success even when the
    // spell it prepared failed its CheckCast, so trusting it meant one silent miss inside the 3s
    // window cost the drake the shield for the next 30s. Nothing sets this cooldown but us - the core
    // does not cool a vehicle spell down by itself.
    if (!drake->HasAura(SPELL_FLAME_SHIELD)) { return false; }
    drake->AddSpellCooldown(SPELL_FLAME_SHIELD, 0, 30000);

    // Hand the tick on either way: eoe fly drake sits directly below this one, and a Static Field
    // dodge that loses a tick here is a dodge that stops halfway.
    return false;
}
