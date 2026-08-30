/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "VoATriggers.h"
#include "EventMap.h"
#include "Group.h"
#include "Object.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "SpellMgr.h"
#include "VoAHelpers.h"

using namespace VoaHelpers;
using namespace EncounterHelpers;

namespace
{
// Skull. Raw index because that is what Group::GetTargetIcon takes.
constexpr uint8 SKULL_ICON = 7;
}

bool EmalonMarkBossTrigger::IsActive()
{
    // Only tank bot can mark target
    if (!botAI->IsTank(bot))
        return false;

    Unit* boss = GetEmalon(bot);
    if (!boss)
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    if (group->GetTargetIcon(SKULL_ICON) == boss->GetGUID())
        return false;

    // While a minion is overcharged the skull belongs to it, not to the boss.
    return OverchargedMinion(bot) == nullptr;
}

bool EmalonLightingNovaTrigger::IsActive()
{
    Unit* boss = GetEmalon(bot);
    if (!boss)
        return false;

    // The main tank eats it by definition, and the off-tank is already 34yd out on his camp.
    if (botAI->IsTank(bot))
        return false;

    // Deliberately not gated on standing inside the radius. This trigger also drives the movement
    // suppression, so it has to stay up for the whole 5s cast: the moment it drops, reach melee walks
    // whoever just ran out straight back in. The action itself is what returns false once clear.
    bool const isCasting = boss->HasUnitState(UNIT_STATE_CASTING);
    bool const isLightingNova = boss->FindCurrentSpellBySpellId(SPELL_LIGHTNING_NOVA_10_MAN) ||
                                boss->FindCurrentSpellBySpellId(SPELL_LIGHTNING_NOVA_25_MAN);
    return isCasting && isLightingNova;
}

bool EmalonOverchargeTrigger::IsActive()
{
    // Only tank bot can mark target
    if (!botAI->IsTank(bot))
        return false;

    if (!GetEmalon(bot))
        return false;

    Unit* overchargedMinion = OverchargedMinion(bot);
    if (!overchargedMinion)
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    return group->GetTargetIcon(SKULL_ICON) != overchargedMinion->GetGUID();
}

bool EmalonFallFromFloorTrigger::IsActive()
{
    if (!GetEmalon(bot))
        return false;

    // Check if bot is on the floor
    return bot->GetPositionZ() < 80.0f;
}

bool EmalonMainTankHoldTrigger::IsActive()
{
    return botAI->IsMainTank(bot) && EmalonEncounterActive(bot);
}

bool EmalonRingHoldTrigger::IsActive()
{
    if (!EmalonEncounterActive(bot))
        return false;

    if (botAI->IsTank(bot))
        return false;

    return botAI->IsHeal(bot) || botAI->IsRanged(bot);
}

bool EmalonOffTankHoldTrigger::IsActive()
{
    if (!IsOffTank(bot) || !EmalonEncounterActive(bot))
        return false;

    return RequireOffTank(botAI, bot);
}

bool EmalonAttackPriorityTrigger::IsActive()
{
    return botAI->IsDps(bot) && EmalonEncounterActive(bot);
}

bool EmalonRedirectThreatTrigger::IsActive()
{
    if (bot->getClass() != CLASS_HUNTER && bot->getClass() != CLASS_ROGUE)
        return false;

    if (!EmalonEncounterActive(bot))
        return false;

    Player* tank = RedirectTarget(botAI, bot);
    return tank && tank != bot;
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

    // Only move when clustered with another player (Rock Shards splashes nearby). Trigger radius (6y) is
    // tighter than the action's flee distance (8y) so bots settle into a spread and stop repositioning.
    constexpr float spreadRadius = 6.0f;
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

    // Only move when clustered with another player (Flaming Cinder splashes nearby). Trigger radius (6y) is
    // tighter than the action's flee distance (8y) so bots settle into a spread and stop repositioning.
    constexpr float spreadRadius = 6.0f;
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
