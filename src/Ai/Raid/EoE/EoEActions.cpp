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
#include <list>
#include <vector>

Unit* FindFreeHoverDisk(Player* bot)
{
    std::list<Creature*> disks;
    bot->GetCreatureListWithEntryInGrid(disks, NPC_HOVER_DISK, 40.0f);

    Unit* closest = nullptr;
    float closestDist = std::numeric_limits<float>::max();
    for (Creature* disk : disks)
    {
        if (!disk->IsAlive() || disk->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
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
    return bot->FindNearestCreature(NPC_SCION_OF_ETERNITY, EOE_ADD_SEARCH_RADIUS, true) != nullptr;
}

void GetNearbyStaticFields(Unit* drake, std::vector<Unit*>& fields)
{
    if (!drake) { return; }

    std::list<Creature*> found;
    drake->GetCreatureListWithEntryInGrid(found, NPC_STATIC_FIELD, STATIC_FIELD_SEARCH_RADIUS);
    for (Creature* field : found)
    {
        if (field->IsAlive())
        {
            fields.push_back(field);
        }
    }
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

bool IsStaticFieldNear(Player* bot)
{
    Unit* drake = bot->GetVehicleBase();
    return drake && drake->FindNearestCreature(NPC_STATIC_FIELD, STATIC_FIELD_DANGER_RADIUS, true) != nullptr;
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
    if (boss->GetExactDist2d(POWER_SPARK_GRIP_POSITION.first, POWER_SPARK_GRIP_POSITION.second) <
        POWER_SPARK_GRIP_SAFE_BOSS_DISTANCE)
    {
        return false;
    }

    Unit* spark = GetNearestPowerSpark(botAI);
    return spark && spark->GetExactDist2d(POWER_SPARK_GRIP_POSITION.first, POWER_SPARK_GRIP_POSITION.second) <=
                        POWER_SPARK_GRIP_ENGAGE_RADIUS;
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
    Creature* current = bot->FindNearestCreature(NPC_ARCANE_OVERLOAD, BUBBLE_SEARCH_RADIUS, true);
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

    if (phase == 1)
    {
        // Whoever Malygos is actually chewing on has to behave like the tank, even if the raid
        // never assigned one - anyone else walking away would drag the boss and swing his cone.
        Unit* boss = MalygosTrigger::getMalygos(bot);
        bool isBossTank = botAI->IsMainTank(bot) || (boss && boss->GetVictim() == bot);

        // Ranged dps hold their own spot well back: anything closer is inside Malygos' effective
        // minimum range and inside the "enemy too close for spell" threshold his CombatReach inflates.
        // Healers stay on the raid stack so the tank stays inside their heal range. A DK on spark duty
        // steps out to the grip spot and comes back here once the grip is spent - this action owns the
        // walking in both directions, PullPowerSparkAction only ever casts.
        bool const gripDuty = !isBossTank && IsOnPowerSparkGripDuty(botAI);
        std::pair<float, float> const& spot = isBossTank                ? MALYGOS_MAINTANK_POSITION
                                              : gripDuty                ? POWER_SPARK_GRIP_POSITION
                                              : botAI->IsRangedDps(bot) ? MALYGOS_RANGED_POSITION
                                                                        : MALYGOS_STACK_POSITION;
        float const tolerance = gripDuty ? POWER_SPARK_GRIP_TOLERANCE : MALYGOS_P1_POSITION_TOLERANCE;

        // The hold spots are fixed, never recomputed from where the boss happens to be: a spot that
        // chases the boss flips to the far side of him whenever he is still on his way out, and the
        // tank then ping-pongs between the edge and the middle, sweeping the cone through the raid.
        if (bot->GetDistance2d(spot.first, spot.second) > tolerance)
        {
            return MoveTo(EOE_MAP_ID, spot.first, spot.second, bot->GetPositionZ(),
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

        // Fall back to Malygos unless a spark should be picked up by ranged DPS.
        Unit* newTarget = boss;
        Unit* spark = nullptr;

        GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
        for (auto& target : targets)
        {
            Unit* unit = botAI->GetUnit(target);
            if (unit && unit->GetEntry() == NPC_POWER_SPARK)
            {
                spark = unit;
                break;
            }
        }

        if (spark && botAI->IsRangedDps(bot))
        {
            newTarget = spark;
        }

        Unit* currentTarget = AI_VALUE(Unit*, "current target");

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
        // Gated on actually having a pet - the grid sweep is the expensive half, and most of the raid
        // would pay for it every tick to find out it has nothing to redirect.
        if (bot->GetGuardianPet())
        {
            if (Unit* petTarget = bot->FindNearestCreature(NPC_NEXUS_LORD, EOE_ADD_SEARCH_RADIUS, true))
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
    if (bot->GetDistance2d(POWER_SPARK_GRIP_POSITION.first, POWER_SPARK_GRIP_POSITION.second) >
        POWER_SPARK_GRIP_TOLERANCE)
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
    // Only ranged DPS peel onto sparks (matching MalygosTargetAction); tanks and melee stay on
    // Malygos so his threat and Arcane Breath cone don't swing into the raid. DK grips handle the
    // spark separately via PullPowerSparkAction.
    if (!botAI->IsRangedDps(bot)) { return false; }

    GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
    for (auto& target : targets)
    {
        Unit* unit = botAI->GetUnit(target);
        if (unit && unit->GetEntry() == NPC_POWER_SPARK)
        {
            return true;
        }
    }
    return false;
}

bool KillPowerSparkAction::Execute(Event /*event*/)
{
    Unit* spark = nullptr;
    GuidVector targets = AI_VALUE(GuidVector, "possible targets no los");
    for (auto& target : targets)
    {
        Unit* unit = botAI->GetUnit(target);
        if (unit && unit->GetEntry() == NPC_POWER_SPARK)
        {
            spark = unit;
            break;
        }
    }
    if (!spark) { return false; }

    Unit* currentTarget = AI_VALUE(Unit*, "current target");
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

    // Arcane Overload is non-attackable, so it never shows up in "possible targets"; scan the grid.
    std::list<Creature*> found;
    bot->GetCreatureListWithEntryInGrid(found, NPC_ARCANE_OVERLOAD, BUBBLE_SEARCH_RADIUS);

    std::vector<Creature*> bubbles;
    for (Creature* bubble : found)
    {
        if (bubble->IsAlive())
        {
            bubbles.push_back(bubble);
        }
    }
    if (bubbles.empty())
    {
        assignedBubbleGuid.Clear();
        return false;
    }

    std::sort(bubbles.begin(), bubbles.end(), [](Creature* a, Creature* b)
    {
        return a->GetGUID().GetRawValue() < b->GetGUID().GetRawValue();
    });

    // Bubbles shrink from the moment they land, so an old one is not worth walking to even though
    // its model still looks full size.
    std::vector<Creature*> usable;
    for (Creature* bubble : bubbles)
    {
        if (GetBubbleShrinkFactor(bubble) >= BUBBLE_MIN_USABLE_FACTOR)
        {
            usable.push_back(bubble);
        }
    }

    // Every bubble is nearly spent: take the freshest anyway rather than stand in the open.
    if (usable.empty())
    {
        usable.push_back(*std::max_element(bubbles.begin(), bubbles.end(), [](Creature* a, Creature* b)
        {
            return GetBubbleShrinkFactor(a) < GetBubbleShrinkFactor(b);
        }));
    }

    Creature* target = nullptr;
    if (!assignedBubbleGuid.IsEmpty())
    {
        // Drop the latch once the assigned bubble ages out, so the bot re-picks a fresh one.
        for (Creature* bubble : usable)
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
            for (Creature* bubble : usable)
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
    std::list<Creature*> scions;
    disk->GetCreatureListWithEntryInGrid(scions, NPC_SCION_OF_ETERNITY, 100.0f);
    for (Creature* candidate : scions)
    {
        if (!candidate->IsAlive())
        {
            continue;
        }
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
    Creature* surge = bot->FindNearestCreature(NPC_SURGE_OF_POWER, 100.0f, true);

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
    uint8 const numPlayers = bot->GetRaidDifficulty() == RAID_DIFFICULTY_25MAN_NORMAL ? 25 : 10;

    // Hold a slot on a ring around Malygos rather than trailing the raid leader. Trailing left the
    // drake permanently in motion, and a moving vehicle can neither finish a cast nor hold a facing,
    // which is why the flight used to circle the boss without ever firing at him. The ring also
    // spreads the raid so one Static Field or Arcane Storm cannot clip everybody.
    if (Unit* boss = MalygosTrigger::getMalygos(bot))
    {
        float const angle = botAI->GetGroupSlotIndex(bot) * 2.0f * static_cast<float>(M_PI) / numPlayers;
        float const tx = boss->GetPositionX() + std::cos(angle) * DRAKE_RING_RADIUS;
        float const ty = boss->GetPositionY() + std::sin(angle) * DRAKE_RING_RADIUS;
        float const tz = boss->GetPositionZ();

        if (drake->GetExactDist(tx, ty, tz) > DRAKE_RING_TOLERANCE)
        {
            // Let an approach finish instead of restamping the destination under the boss every tick.
            if (mm->GetCurrentMovementGeneratorType() == POINT_MOTION_TYPE) { return true; }

            // Straight 3d spline for the same reason the hover disks use one: a generated path is
            // 2d and drops the destination onto whatever is underneath.
            drake->SetCanFly(true);
            mm->Clear(false);
            mm->MovePoint(0, tx, ty, tz, FORCED_MOVEMENT_NONE, 0.f, 0.f,
                          /*generatePath*/ false, /*forceDestination*/ true);
            drake->SendMovementFlagUpdate();
            return true;
        }

        if (mm->GetCurrentMovementGeneratorType() != IDLE_MOTION_TYPE)
        {
            mm->Clear(false);
            mm->MoveIdle();
            drake->SendMovementFlagUpdate();
        }

        // Healers have to be free to turn onto whoever they are healing, so only the dps get pinned.
        if (!IsDrakeHealer(botAI) && !drake->HasInArc(CAST_ANGLE_IN_FRONT, boss))
        {
            drake->SetFacingToObject(boss);
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

    if (IsDrakeHealer(botAI))
    {
        return DrakeHealAction();
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

    // Closing the gap belongs to EoEFlyDrakeAction, and to AvoidStaticFieldAction while a field is
    // up. Steering the same vehicle from here as well left the two fighting over the destination,
    // and MoveForwards runs its endpoint through CanReachPositionAndGetValidCoords, which has no
    // answer for a point in mid-air - so the drake stopped dead and stayed out of range for good.
    if (drake->GetExactDist(target) > DRAKE_ATTACK_RANGE) { return false; }

    uint8 comboPoints = drake->GetComboPoints(target);
    if (comboPoints >= 2)
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

    Unit* latched = nullptr;
    Unit* mostInjured = nullptr;
    uint8 injuredCount = 0;
    if (Group* group = bot->GetGroup())
    {
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* member = itr->GetSource();
            if (!member) { continue; }

            Unit* allyDrake = member->GetVehicleBase();
            if (!allyDrake || allyDrake->GetEntry() != NPC_WYRMREST_SKYTALON) { continue; }
            if (!allyDrake->IsAlive() || allyDrake->IsFullHealth()) { continue; }

            ++injuredCount;
            if (allyDrake->GetGUID() == healTargetGuid) { latched = allyDrake; }
            if (!mostInjured || allyDrake->GetHealthPct() < mostInjured->GetHealthPct())
            {
                mostInjured = allyDrake;
            }
        }
    }

    // Life Burst is the finisher and it heals the whole flight, so it is worth the dump as soon as
    // more than one drake is hurt. Combo points live on whoever Revivify last landed on, so it has
    // to be aimed there too - fired at ourselves it would find no combo points to spend.
    Unit* comboUnit = latched ? latched : drake;
    if (drake->GetComboPoints() >= DRAKE_LIFE_BURST_COMBO && injuredCount >= 2)
    {
        healTargetGuid.Clear();
        return botAI->CastVehicleSpell(SPELL_LIFE_BURST, comboUnit);
    }

    // Stay on the latched drake even when someone else has dipped lower: switching resets the combo
    // count to one and Life Burst never comes up. Nobody hurt at all means banking points on our own
    // drake, ready for the next Arcane Pulse.
    Unit* healTarget = latched ? latched : (mostInjured ? mostInjured : drake);
    healTargetGuid = healTarget->GetGUID();

    // Revivify is single-target. CanCastVehicleSpell reports BAD_TARGETS on drakes, so force it.
    return botAI->CastVehicleSpell(SPELL_REVIVIFY, healTarget);
}

bool AvoidStaticFieldAction::isPossible()
{
    Unit* vehicleBase = bot->GetVehicleBase();
    return (vehicleBase && vehicleBase->GetEntry() == NPC_WYRMREST_SKYTALON);
}

bool AvoidStaticFieldAction::Execute(Event /*event*/)
{
    Unit* drake = bot->GetVehicleBase();
    if (!drake) { fleeing = false; return false; }

    // Every field in range, not just the nearest: the boss lands one every 12s and they live 20s, so
    // running clear of one regularly means running into the other.
    std::vector<Unit*> fields;
    GetNearbyStaticFields(drake, fields);
    if (fields.empty()) { fleeing = false; return false; }

    float const drakeX = drake->GetPositionX();
    float const drakeY = drake->GetPositionY();

    // Clear of all of them: hand the tick back so the drake rotation runs. The phase-3 multiplier
    // holds the follow formation off while a field is close, so nothing drags the drake back in.
    if (IsClearOfStaticFields(drakeX, drakeY, fields, STATIC_FIELD_SAFE_RADIUS))
    {
        fleeing = false;
        return false;
    }

    MotionMaster* mm = drake->GetMotionMaster();

    // Let our own escape run, but only while the point we picked is still clear - a field landing on
    // the way there has to be able to re-route us. Testing the MotionMaster alone could not tell our
    // escape from the dps range-close, so a drake mid-approach sat out the entire flee.
    if (fleeing && mm->GetCurrentMovementGeneratorType() == POINT_MOTION_TYPE &&
        IsClearOfStaticFields(fleeX, fleeY, fields, STATIC_FIELD_SAFE_RADIUS))
    {
        return true;
    }

    // Sweep headings at the shortest workable hop first, and inside a hop take the one that ends up
    // furthest from anything. Z is held: the fields are ground-anchored columns, climbing over one
    // is not an option a drake has.
    uint8 const headings = 16;
    bool found = false;
    float bestX = 0.0f;
    float bestY = 0.0f;
    for (float hop = STATIC_FIELD_SAFE_RADIUS; !found && hop <= STATIC_FIELD_SAFE_RADIUS * 2.0f;
         hop += STATIC_FIELD_SAFE_RADIUS)
    {
        float bestClearance = 0.0f;
        for (uint8 i = 0; i < headings; ++i)
        {
            float angle = i * (2.0f * static_cast<float>(M_PI) / headings);
            float cx = drakeX + std::cos(angle) * hop;
            float cy = drakeY + std::sin(angle) * hop;

            float clearance = std::numeric_limits<float>::max();
            for (Unit* field : fields)
            {
                clearance = std::min(clearance, field->GetExactDist2d(cx, cy));
            }

            if (clearance >= STATIC_FIELD_SAFE_RADIUS && clearance > bestClearance)
            {
                bestClearance = clearance;
                bestX = cx;
                bestY = cy;
                found = true;
            }
        }
    }

    if (!found) { fleeing = false; return false; }

    fleeX = bestX;
    fleeY = bestY;
    fleeing = true;

    mm->Clear(false);
    mm->MovePoint(0, bestX, bestY, drake->GetPositionZ(), FORCED_MOVEMENT_NONE, 0.f, 0.f,
                  /*generatePath*/ false, /*forceDestination*/ true);
    drake->SendMovementFlagUpdate();
    return true;
}

bool DrakeDodgeSurgeAction::isPossible()
{
    Unit* vehicleBase = bot->GetVehicleBase();
    return (vehicleBase && vehicleBase->GetEntry() == NPC_WYRMREST_SKYTALON);
}

bool DrakeDodgeSurgeAction::Execute(Event /*event*/)
{
    Unit* drake = bot->GetVehicleBase();
    if (!drake) { return false; }

    Unit* boss = MalygosTrigger::getMalygos(bot);
    if (!boss) { return false; }

    // The boss leaves its victim list standing until it picks the next one, so the trigger stays hot
    // for the best part of the 7s between beams. Shield and peel once, then hand the ticks back -
    // the spline carries on running while the drake resumes its rotation.
    if (lastDodgeAtMs && GetMSTimeDiffToNow(lastDodgeAtMs) < DRAKE_SURGE_DODGE_COOLDOWN_MS)
    {
        return false;
    }

    // Flame Shield absorbs the surge; pop it on cooldown as the fixate lands.
    if (botAI->CanCastVehicleSpell(SPELL_FLAME_SHIELD, drake) &&
        !drake->HasSpellCooldown(SPELL_FLAME_SHIELD))
    {
        if (botAI->CastVehicleSpell(SPELL_FLAME_SHIELD, drake))
        {
            drake->AddSpellCooldown(SPELL_FLAME_SHIELD, 0, 30000);
        }
    }

    // Blazing Speed to peel out of the beam faster when available.
    if (botAI->CanCastVehicleSpell(SPELL_BLAZING_SPEED, drake) &&
        !drake->HasSpellCooldown(SPELL_BLAZING_SPEED))
    {
        if (botAI->CastVehicleSpell(SPELL_BLAZING_SPEED, drake))
        {
            drake->AddSpellCooldown(SPELL_BLAZING_SPEED, 0, 60000);
        }
    }

    // Strafe perpendicular to the boss line so we leave the beam's path.
    float angle = boss->GetAngle(drake) + M_PI_2;
    float const strafe = 25.0f;
    float tx = drake->GetPositionX() + std::cos(angle) * strafe;
    float ty = drake->GetPositionY() + std::sin(angle) * strafe;

    MotionMaster* mm = drake->GetMotionMaster();
    mm->Clear(false);
    mm->MovePoint(0, tx, ty, drake->GetPositionZ(), FORCED_MOVEMENT_NONE, 0.f, 0.f,
                  /*generatePath*/ false, /*forceDestination*/ true);
    drake->SendMovementFlagUpdate();
    lastDodgeAtMs = getMSTime();
    return true;
}
