/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "VoAActions.h"
#include "VoATriggers.h"
#include "Define.h"
#include "DynamicObject.h"
#include "Event.h"
#include "Group.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "Playerbots.h"
#include "RaidBossHelpers.h"
#include "SpellAuras.h"
#include "Unit.h"

const Position VOA_EMALON_RESTORE_POSITION = Position(-221.8f, -243.8f, 96.8f, 4.7f);

bool EmalonLightingNovaAction::Execute(Event /*event*/)
{
    const float radius = 25.0f;  // 20 yards + 5 yard for safety for 10 man. For 25man there is no maximum range but 25 yards should be ok

    Unit* boss = AI_VALUE2(Unit*, "find target", "emalon the storm watcher");
    if (!boss)
        return false;

    float currentDistance = bot->GetDistance2d(boss);

    if (currentDistance < radius)
    {
        return MoveAway(boss, radius - currentDistance);
    }

    return false;
}

bool EmalonLightingNovaAction::isUseful()
{
    EmalonLightingNovaTrigger emalonLightingNovaTrigger(botAI);
    return emalonLightingNovaTrigger.IsActive();
}

bool EmalonOverchargeAction::Execute(Event /*event*/)
{
    // Check if there is any overcharged minion
    Unit* minion = nullptr;
    GuidVector npcs = AI_VALUE(GuidVector, "nearest hostile npcs");
    for (auto& npc : npcs)
    {
        Unit* unit = botAI->GetUnit(npc);
        if (!unit)
            continue;

        uint32 entry = unit->GetEntry();
        if (entry == NPC_TEMPEST_MINION && unit->HasAura(AURA_OVERCHARGE))
        {
            minion = unit;
            break;
        }
    }
    if (!minion)
    {
        return false;
    }

    // The trigger already gates this to a single tank bot; mark the overcharged minion so DPS burn it.
    // MarkTargetWithSkull null-checks the group and only re-sets when the icon differs (idempotent).
    MarkTargetWithSkull(bot, minion);
    return true;
}

bool EmalonOverchargeAction::isUseful()
{
    EmalonOverchargeTrigger emalonOverchargeTrigger(botAI);
    return emalonOverchargeTrigger.IsActive();
}

bool EmalonFallFromFloorAction::Execute(Event /*event*/)
{
    return bot->TeleportTo(bot->GetMapId(), VOA_EMALON_RESTORE_POSITION.GetPositionX(),
                           VOA_EMALON_RESTORE_POSITION.GetPositionY(), VOA_EMALON_RESTORE_POSITION.GetPositionZ(),
                           VOA_EMALON_RESTORE_POSITION.GetOrientation());
}

bool EmalonFallFromFloorAction::isUseful()
{
    EmalonFallFromFloorTrigger emalonFallFromFloorTrigger(botAI);
    return emalonFallFromFloorTrigger.IsActive();
}

//
//  Archavon the Stone Watcher
//

// Ranged bots spread out so a single Rock Shards splashes fewer players
bool ArchavonRockShardsSpreadAction::Execute(Event /*event*/)
{
    constexpr float safeDistance = 8.0f;
    constexpr uint32 minInterval = 3000;
    if (Unit* nearestPlayer = GetNearestPlayerInRadius(bot, safeDistance))
    {
        return FleePosition(nearestPlayer->GetPosition(), safeDistance, minInterval);
    }

    return false;
}

bool ArchavonRockShardsSpreadAction::isUseful()
{
    ArchavonRockShardsSpreadTrigger archavonRockShardsSpreadTrigger(botAI);
    return archavonRockShardsSpreadTrigger.IsActive();
}

//
//  Koralon the Flame Watcher
//

// Non-tanks step out of Koralon's frontal Burning Breath cone
bool KoralonBurningBreathAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "koralon the flame watcher");
    if (!boss)
    {
        return false;
    }

    // Step perpendicular to the boss's facing to clear the frontal cone, fleeing toward whichever
    // side the bot is already on.
    const float orientation = boss->GetOrientation();
    const float dirX = std::cos(orientation);
    const float dirY = std::sin(orientation);

    const float relX = bot->GetPositionX() - boss->GetPositionX();
    const float relY = bot->GetPositionY() - boss->GetPositionY();

    const float perpendicular = relX * dirY - relY * dirX;
    const float side = perpendicular >= 0.0f ? 1.0f : -1.0f;

    const float escapeX = dirY * side;
    const float escapeY = -dirX * side;

    botAI->InterruptSpell();

    constexpr float clearance = 12.0f;
    float destX = bot->GetPositionX() + escapeX * clearance;
    float destY = bot->GetPositionY() + escapeY * clearance;
    float destZ = bot->GetPositionZ();
    bot->UpdateAllowedPositionZ(destX, destY, destZ);

    return MoveTo(bot->GetMapId(), destX, destY, destZ, false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT, true, false);
}

bool KoralonBurningBreathAction::isUseful()
{
    KoralonBurningBreathTrigger koralonBurningBreathTrigger(botAI);
    return koralonBurningBreathTrigger.IsActive();
}

// Ranged bots spread out so a single Flaming Cinder splashes fewer players
bool KoralonFlamingCinderSpreadAction::Execute(Event /*event*/)
{
    constexpr float safeDistance = 8.0f;
    constexpr uint32 minInterval = 3000;
    if (Unit* nearestPlayer = GetNearestPlayerInRadius(bot, safeDistance))
    {
        return FleePosition(nearestPlayer->GetPosition(), safeDistance, minInterval);
    }

    return false;
}

bool KoralonFlamingCinderSpreadAction::isUseful()
{
    KoralonFlamingCinderSpreadTrigger koralonFlamingCinderSpreadTrigger(botAI);
    return koralonFlamingCinderSpreadTrigger.IsActive();
}

//
//  Toravon the Ice Watcher
//

// Bot stands in a Freezing Ground patch: flee away from the patch's dynamic object center
bool ToravonFreezingGroundAction::Execute(Event /*event*/)
{
    constexpr float safeDistance = 8.0f;
    constexpr uint32 minInterval = 1000;

    // The patch is a dynobj-owned area aura; use its owner as the point to flee from
    Aura* aura = AI_VALUE(Aura*, "area debuff");
    if (aura && !aura->IsRemoved() && aura->GetSpellInfo() && aura->GetSpellInfo()->Id == SPELL_FREEZING_GROUND)
    {
        if (DynamicObject* dynOwner = aura->GetDynobjOwner())
        {
            if (dynOwner->IsInWorld())
            {
                return FleePosition(dynOwner->GetPosition(), safeDistance, minInterval);
            }
        }
    }

    return false;
}

bool ToravonFreezingGroundAction::isUseful()
{
    ToravonFreezingGroundTrigger toravonFreezingGroundTrigger(botAI);
    return toravonFreezingGroundTrigger.IsActive();
}

// Frozen Orbs pulse frost damage as they chase players: flee the nearest orb
bool ToravonFrozenOrbAvoidAction::Execute(Event /*event*/)
{
    constexpr float orbDangerRadius = 10.0f;
    constexpr float safeDistance = 12.0f;
    constexpr uint32 minInterval = 1000;

    if (Unit* orb = bot->FindNearestCreature(NPC_FROZEN_ORB, orbDangerRadius))
    {
        return FleePosition(orb->GetPosition(), safeDistance, minInterval);
    }

    return false;
}

bool ToravonFrozenOrbAvoidAction::isUseful()
{
    ToravonFrozenOrbAvoidTrigger toravonFrozenOrbAvoidTrigger(botAI);
    return toravonFrozenOrbAvoidTrigger.IsActive();
}
