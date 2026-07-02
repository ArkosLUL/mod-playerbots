#include "VoATriggers.h"

#include "EventMap.h"
#include "Object.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidBossHelpers.h"
#include "SpellMgr.h"

bool EmalonMarkBossTrigger::IsActive()
{
    // Only tank bot can mark target
    if (!botAI->IsTank(bot))
    {
        return false;
    }

    // Check boss and it is alive
    Unit* boss = AI_VALUE2(Unit*, "find target", "emalon the storm watcher");
    if (!boss || !boss->IsAlive())
    {
        return false;
    }

    // Check if boss have skull mark
    Group* group = bot->GetGroup();
    int8 skullIndex = 7;  // Skull
    ObjectGuid currentSkullTarget = group->GetTargetIcon(skullIndex);
    if (currentSkullTarget == boss->GetGUID())
    {
        return false;
    }

    // Check if there is any overcharged minion
    Unit* overchargedMinion = nullptr;
    GuidVector npcs = AI_VALUE(GuidVector, "nearest hostile npcs");
    for (auto& npc : npcs)
    {
        Unit* unit = botAI->GetUnit(npc);
        if (!unit)
            continue;

        uint32 entry = unit->GetEntry();
        if (entry == NPC_TEMPEST_MINION && unit->HasAura(AURA_OVERCHARGE))
        {
            overchargedMinion = unit;
            break;
        }
    }
    if (overchargedMinion)
    {
        return false;
    }

    return true;
}

bool EmalonLightingNovaTrigger::IsActive()
{
    // Check boss and it is alive
    Unit* boss = AI_VALUE2(Unit*, "find target", "emalon the storm watcher");
    if (!boss || !boss->IsAlive())
    {
        return false;
    }

    // Tank dont need to move
    if (botAI->IsTank(bot))
    {
        return false;
    }

    // Check if boss is casting Lightning Nova
    bool isCasting = boss->HasUnitState(UNIT_STATE_CASTING);
    bool isLightingNova = boss->FindCurrentSpellBySpellId(SPELL_LIGHTNING_NOVA_10_MAN) ||
                          boss->FindCurrentSpellBySpellId(SPELL_LIGHTNING_NOVA_25_MAN);
    return isCasting && isLightingNova;
}

bool EmalonOverchargeTrigger::IsActive()
{
    // Only tank bot can mark target
    if (!botAI->IsTank(bot))
    {
        return false;
    }

    // Check boss and it is alive
    Unit* boss = AI_VALUE2(Unit*, "find target", "emalon the storm watcher");
    if (!boss || !boss->IsAlive())
    {
        return false;
    }

    // Check if there is any overcharged minion
    Unit* overchargedMinion = nullptr;
    GuidVector npcs = AI_VALUE(GuidVector, "nearest hostile npcs");
    for (auto& npc : npcs)
    {
        Unit* unit = botAI->GetUnit(npc);
        if (!unit)
            continue;

        uint32 entry = unit->GetEntry();
        if (entry == NPC_TEMPEST_MINION && unit->HasAura(AURA_OVERCHARGE))
        {
            overchargedMinion = unit;
            break;
        }
    }
    if (!overchargedMinion)
    {
        return false;
    }

    // Check if minion have skull mark
    Group* group = bot->GetGroup();
    int8 skullIndex = 7;  // Skull
    ObjectGuid currentSkullTarget = group->GetTargetIcon(skullIndex);
    if (currentSkullTarget == overchargedMinion->GetGUID())
    {
        return false;
    }

    return true;
}

bool EmalonFallFromFloorTrigger::IsActive()
{
    // Check boss and it is alive
    Unit* boss = AI_VALUE2(Unit*, "find target", "emalon the storm watcher");
    if (!boss || !boss->IsAlive())
    {
        return false;
    }

    // Check if bot is on the floor
    return bot->GetPositionZ() < 80.0f;
}

//
// Archavon the Stone Watcher
//
bool ArchavonRockShardsSpreadTrigger::IsActive()
{
    // Only ranged bots spread; melee stay stacked on the boss
    if (!botAI->IsRanged(bot))
    {
        return false;
    }

    // Check boss is engaged and alive
    Unit* boss = AI_VALUE2(Unit*, "find target", "archavon the stone watcher");
    if (!boss || !boss->IsAlive() || !boss->IsInCombat())
    {
        return false;
    }

    // Only move when clustered with another player (Rock Shards splashes nearby)
    constexpr float spreadRadius = 8.0f;
    return GetNearestPlayerInRadius(bot, spreadRadius) != nullptr;
}

//
// Koralon the Flame Watcher
//
bool KoralonBurningBreathTrigger::IsActive()
{
    // Tanks hold the boss and stay in the cone; only non-tanks need to clear it
    if (botAI->IsTank(bot))
    {
        return false;
    }

    // Check boss and it is alive
    Unit* boss = AI_VALUE2(Unit*, "find target", "koralon the flame watcher");
    if (!boss || !boss->IsAlive())
    {
        return false;
    }

    // Koralon casts SPELL_BURNING_BREATH (10-man id); the core remaps it to the difficulty-specific
    // variant at cast time (SpellDifficulty), so resolve the id for the boss's map before matching it,
    // otherwise the 25-man cast is never detected.
    uint32 burningBreathId = SPELL_BURNING_BREATH;
    if (SpellInfo const* breathInfo = sSpellMgr->GetSpellInfo(SPELL_BURNING_BREATH))
    {
        burningBreathId = sSpellMgr->GetSpellForDifficultyFromSpell(breathInfo, boss)->Id;
    }

    // Check if boss is casting Burning Breath
    if (!boss->HasUnitState(UNIT_STATE_CASTING) || !boss->FindCurrentSpellBySpellId(burningBreathId))
    {
        return false;
    }

    // Only react when actually standing inside the frontal cone (90 deg, 40 yards)
    return IsBotInFrontalCone(bot, boss, float(M_PI) / 2.0f, 40.0f);
}

bool KoralonFlamingCinderSpreadTrigger::IsActive()
{
    // Only ranged bots spread; melee stay stacked on the boss
    if (!botAI->IsRanged(bot))
    {
        return false;
    }

    // Check boss is engaged and alive
    Unit* boss = AI_VALUE2(Unit*, "find target", "koralon the flame watcher");
    if (!boss || !boss->IsAlive() || !boss->IsInCombat())
    {
        return false;
    }

    // Only move when clustered with another player (Flaming Cinder splashes nearby)
    constexpr float spreadRadius = 8.0f;
    return GetNearestPlayerInRadius(bot, spreadRadius) != nullptr;
}

//
// Toravon the Ice Watcher
//
bool ToravonFreezingGroundTrigger::IsActive()
{
    // Freezing Ground drops a persistent frost patch (dynobj area aura) under a random player;
    // anyone standing in it gets the aura and must move out. Same spell id across all difficulties.
    return bot->HasAura(SPELL_FREEZING_GROUND);
}

bool ToravonFrozenOrbAvoidTrigger::IsActive()
{
    // Frozen Orbs pulse frost damage in a small radius and chase players; keep clear of them.
    // Orb count scales with difficulty (1 on 10-man, 3 on 25-man) but the entry is identical.
    constexpr float orbDangerRadius = 10.0f;
    return bot->FindNearestCreature(NPC_FROZEN_ORB, orbDangerRadius) != nullptr;
}
