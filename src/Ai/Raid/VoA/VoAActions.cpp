/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "VoAActions.h"
#include "Define.h"
#include "DynamicObject.h"
#include "Event.h"
#include "Group.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "SpellAuras.h"
#include "Unit.h"
#include "VoAHelpers.h"
#include "VoATriggers.h"

using namespace VoaHelpers;
using namespace EncounterHelpers;

const Position VOA_EMALON_RESTORE_POSITION = Position(-221.8f, -243.8f, 96.8f, 4.7f);

bool EmalonPositioningAction::MoveToClamped(float x, float y, float tolerance)
{
    ClampToChamber(x, y);

    if (bot->GetExactDist2d(x, y) <= tolerance)
        return false;

    return MoveTo(VOA_MAP_ID, x, y, bot->GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT);
}

bool EmalonLightingNovaAction::Execute(Event /*event*/)
{
    Unit* boss = GetEmalon(bot);
    if (!boss)
        return false;

    if (bot->GetExactDist2d(boss) >= LIGHTNING_NOVA_CLEAR_RADIUS)
        return false;

    // Out towards the entrance, not radially away from wherever the bot happens to stand. Fleeing
    // radially walks anyone caught south of the boss into the two dead-end alcoves behind him. The
    // sideways half-step keeps each bot on the side it is already on rather than running the whole
    // melee pack through him, and puts the destination 24.6yd out - clear of the 20yd radius.
    float const sideStep = LIGHTNING_NOVA_CLEAR_RADIUS / 2.0f;
    float const x = boss->GetPositionX() +
                    (bot->GetPositionX() < boss->GetPositionX() ? -sideStep : sideStep);
    float const y = boss->GetPositionY() + LIGHTNING_NOVA_CLEAR_RADIUS;

    // The cast is 5s long, so even a melee bot standing on him covers this comfortably.
    return MoveToClamped(x, y, 0.0f);
}

bool EmalonLightingNovaAction::isUseful()
{
    EmalonLightingNovaTrigger emalonLightingNovaTrigger(botAI);
    return emalonLightingNovaTrigger.IsActive();
}

bool EmalonMainTankHoldAction::Execute(Event /*event*/)
{
    // The anchor is 10.33yd past where Emalon should end up, which is exactly his melee range: he
    // walks until the tank is inside it and stops there. Nothing here needs to know whether the drag
    // has landed - the tolerance below stops the move being re-issued once the tank is on the spot.
    return MoveToClamped(CHAMBER_CENTER_X, MAIN_TANK_ANCHOR_Y, MAIN_TANK_ARRIVAL_TOLERANCE);
}

bool EmalonRingHoldAction::Execute(Event /*event*/)
{
    Unit* boss = GetEmalon(bot);
    if (!boss)
        return false;

    float x = 0.0f;
    float y = 0.0f;
    if (!RingSlotFor(botAI, bot, boss, x, y))
        return false;

    // The slot is derived from the boss, and he moves ~12yd during the pull drag. Without this the
    // whole ring trails him yard for yard instead of walking once and settling.
    if (hasDest)
    {
        float const driftX = x - destX;
        float const driftY = y - destY;
        if (driftX * driftX + driftY * driftY <= RING_ANCHOR_DRIFT * RING_ANCHOR_DRIFT)
        {
            x = destX;
            y = destY;
        }
    }

    ClampToChamber(x, y);
    hasDest = true;
    destX = x;
    destY = y;

    return MoveToClamped(x, y, RING_ARRIVAL_TOLERANCE);
}

bool EmalonOffTankHoldAction::Execute(Event /*event*/)
{
    Unit* minion = MinionToPickUp(bot);

    if (minion)
    {
        // Acquiring the target must not cost the tick, or the off-tank spends the whole pull picking
        // targets and never walks anywhere.
        if (bot->GetVictim() != minion)
            Attack(minion);
        else if (bot->GetTarget() != minion->GetGUID())
            bot->SetSelection(minion->GetGUID());

        // Every taunt in the game reaches 30yd, and the camp is 18-19yd from the two near spawn
        // corners - but the two far ones are 47yd out, which nothing reaches. Walk at those rather
        // than leave a loose minion to pick a healer.
        if (bot->GetExactDist2d(minion) > MINION_TAUNT_RANGE)
        {
            float x = minion->GetPositionX();
            float y = minion->GetPositionY();
            ClampToChamber(x, y);
            return MoveTo(VOA_MAP_ID, x, y, bot->GetPositionZ(), false, false, false, false,
                          MovementPriority::MOVEMENT_COMBAT);
        }
    }
    else if (Unit* boss = GetEmalon(bot))
    {
        // Nothing to hold. If the off-tank is still swinging at Emalon he has to let go here: the
        // multiplier blocks re-acquiring the boss but cannot drop a target already picked up.
        if (bot->GetVictim() == boss)
            bot->AttackStop();
    }

    float x = OFFTANK_CAMP.GetPositionX();
    float y = OFFTANK_CAMP.GetPositionY();
    ClampToChamber(x, y);

    if (bot->GetExactDist2d(x, y) <= OFFTANK_ARRIVAL_TOLERANCE)
        return false;

    return MoveTo(VOA_MAP_ID, x, y, bot->GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT);
}

bool EmalonAttackPriorityAction::Execute(Event /*event*/)
{
    Unit* boss = GetEmalon(bot);
    if (!boss)
        return false;

    // Melee never leave the boss. The overcharged minion is parked 34yd away at the off-tank camp and
    // only lives 20s, so a melee round trip spends over half the window walking - and it does not need
    // to: ~52k (10-man) and ~191k (25-man) of minion health falls to the ranged half of the raid well
    // inside the timer.
    Unit* target = boss;
    if (botAI->IsRanged(bot))
    {
        if (Unit* overcharged = OverchargedMinion(bot))
            target = overcharged;
    }

    if (bot->GetGuardianPet())
        CommandPetAttack(botAI, target);

    if (bot->GetVictim() != target)
        return Attack(target);

    return false;
}

Player* EmalonRedirectThreatAction::GetRedirectTank()
{
    return RedirectTarget(botAI, bot);
}

Unit* EmalonRedirectThreatAction::GetThreatDumpTarget()
{
    // Spend the three Misdirection charges on whatever the off-tank is trying to hold, which is the
    // threat that actually matters here.
    // Null off-tank falls through harmlessly: the lookup returns nothing to dump into.
    return MinionToPickUp(GetOffTank(botAI, bot));
}

bool EmalonOverchargeAction::Execute(Event /*event*/)
{
    Unit* minion = OverchargedMinion(bot);
    if (!minion)
        return false;

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

    bot->CastStop();

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
