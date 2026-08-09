/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "EoETriggers.h"
#include "CreatureAI.h"
#include "EoEActions.h"
#include "InstanceScript.h"
#include "SharedDefines.h"
#include "Spell.h"
#include "Timer.h"
#include "Vehicle.h"

#include <unordered_map>

namespace
{
constexpr uint32 EOE_PHASE_CACHE_MS = 500;

struct PhaseCacheEntry
{
    uint32 at;
    uint8 phase;
};

// getPhase is the entry point for every trigger in this strategy and for both of its per-tick
// actions, so an uncached call meant a dozen grid sweeps per bot per tick and the raid crawled.
// A bot is only ever updated from its own map's thread, so a thread_local cache needs no lock.
thread_local std::unordered_map<uint64, PhaseCacheEntry> phaseCache;
}

Unit* MalygosTrigger::getMalygos(Player* bot)
{
    // getPhase runs several times per bot per tick, and a 250y grid sweep is the most expensive
    // thing in this strategy. The instance script already holds the guid, so ask it first.
    if (InstanceScript* instance = bot->GetInstanceScript())
    {
        // Guid lookup has no liveness filter of its own, unlike the search below it.
        if (Creature* boss = instance->GetCreature(EOE_DATA_MALYGOS))
        {
            return boss->IsAlive() ? boss : nullptr;
        }
    }

    return bot->FindNearestCreature(NPC_MALYGOS, 250.0f, true);
}

uint8 MalygosTrigger::getPhase(Player* bot)
{
    if (bot->GetMapId() != EOE_MAP_ID) { return 0; }

    uint64 const key = bot->GetGUID().GetRawValue();
    uint32 const now = getMSTime();
    PhaseCacheEntry& cached = phaseCache[key];
    if (cached.at && getMSTimeDiff(cached.at, now) < EOE_PHASE_CACHE_MS)
    {
        return cached.phase;
    }
    cached.at = now;
    cached.phase = 0;

    Unit* drake = bot->GetVehicleBase();
    if (drake && drake->GetEntry() == NPC_WYRMREST_SKYTALON)
    {
        cached.phase = 3;
        return 3;
    }

    Unit* boss = getMalygos(bot);
    if (!boss || !boss->IsInCombat()) { return 0; }

    // P2: Malygos is airborne/untargetable while the disc adds are up.
    if (bot->FindNearestCreature(NPC_NEXUS_LORD, EOE_ADD_SEARCH_RADIUS, true) ||
        bot->FindNearestCreature(NPC_SCION_OF_ETERNITY, EOE_ADD_SEARCH_RADIUS, true))
    {
        cached.phase = 2;
        return 2;
    }

    // Attackable with no adds and not on a drake -> Phase 1.
    if (!boss->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE))
    {
        cached.phase = 1;
        return 1;
    }

    // In combat, non-attackable, no adds, not yet mounted -> P1->P2 / P2->P3 transition.
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
    if (MalygosTrigger::getPhase(bot) != 1) { return false; }

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

bool MalygosBubbleTrigger::IsActive()
{
    if (MalygosTrigger::getPhase(bot) != 2) { return false; }

    // Disk riders are already immune to both Arcane Overload and Surge of Power damage.
    if (bot->GetVehicle()) { return false; }

    // Fires again once the current bubble is nearly spent, so the bot walks to a fresh one before
    // the old one despawns rather than after.
    if (IsSafelySheltered(bot)) { return false; }

    if (!botAI->IsRanged(bot) && !botAI->IsHeal(bot))
    {
        // Melee and tanks owe the raid a dead Nexus Lord first, then a disk ride up to the Scions.
        // They only take shelter once neither job is on offer - and the disk half only counts for
        // bots that MalygosFreeDiskTrigger will actually let board.
        if (bot->FindNearestCreature(NPC_NEXUS_LORD, BUBBLE_SEARCH_RADIUS, true)) { return false; }
        if (IsEligibleDiskRider(bot) && AnyScionAlive(bot) && FindFreeHoverDisk(bot)) { return false; }
    }

    return bot->FindNearestCreature(NPC_ARCANE_OVERLOAD, BUBBLE_SEARCH_RADIUS, true) != nullptr;
}

bool MalygosFreeDiskTrigger::IsActive()
{
    if (MalygosTrigger::getPhase(bot) != 2) { return false; }
    if (bot->GetVehicle()) { return false; }
    if (!IsEligibleDiskRider(bot)) { return false; }

    // Don't climb back onto a disk the bot just got off because the Scions are dead.
    if (!AnyScionAlive(bot)) { return false; }

    return FindFreeHoverDisk(bot) != nullptr;
}

bool MalygosOnDiskTrigger::IsActive()
{
    Unit* vehicleBase = bot->GetVehicleBase();
    return vehicleBase && vehicleBase->GetEntry() == NPC_HOVER_DISK;
}

bool SurgeOfPowerTrigger::IsActive()
{
    if (MalygosTrigger::getPhase(bot) != 2) { return false; }

    if (bot->FindNearestCreature(NPC_SURGE_OF_POWER, 100.0f, true))
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

bool StaticFieldTrigger::IsActive()
{
    if (MalygosTrigger::getPhase(bot) != 3) { return false; }

    Unit* drake = bot->GetVehicleBase();
    if (!drake) { return false; }

    Creature* field = drake->FindNearestCreature(NPC_STATIC_FIELD, STATIC_FIELD_DANGER_RADIUS, true);
    return bool(field);
}

bool DrakeSurgeTrigger::IsActive()
{
    if (MalygosTrigger::getPhase(bot) != 3) { return false; }

    Unit* drake = bot->GetVehicleBase();
    if (!drake) { return false; }

    Unit* boss = MalygosTrigger::getMalygos(bot);
    if (!boss) { return false; }

    Creature* bossCreature = boss->ToCreature();
    if (!bossCreature || !bossCreature->AI()) { return false; }

    // Both P3 surges are DoCastAOE with no unit target, so reading the spell's target guid always
    // came back empty and this never fired once. The boss AI publishes the victims in its guid slots
    // instead, and it does so 3s before the beam - the whole reaction window lives there.
    // Only a victim burns Flame Shield and breaks formation; everyone else keeps the cooldown.
    for (uint8 i = 0; i < EOE_NUM_MAX_SURGE_TARGETS; ++i)
    {
        if (bossCreature->AI()->GetGUID(EOE_DATA_FIRST_SURGE_TARGET_GUID + i) == drake->GetGUID())
        {
            return true;
        }
    }
    return false;
}
