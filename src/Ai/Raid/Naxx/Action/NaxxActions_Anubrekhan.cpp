/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "NaxxActions.h"
#include "MotionMaster.h"
#include "NaxxSpellIds.h"
#include "ObjectGuid.h"
#include "Playerbots.h"
#include "RaidBossHelpers.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace
{
// 240 degrees of arc keeps every slot within ~30 yd of the boss while still leaving ~10 yd between
// neighbours at eight bots - more than the Impale splash.
constexpr float RangedRingArc = 4.0f * static_cast<float>(M_PI) / 3.0f;
// The ring sits at the room-centre end of the band, healers at the far end, so the two rows do not
// share a radius and cannot line up on top of each other.
constexpr float RangedDpsBandOffset = 4.0f;
// A boss that has moved less than this leaves the ring where it is.
constexpr float AnchorDriftDistance = 8.0f;

// One ring shared by every non-tank, so the three role groups do not each build their own on the
// same spot. NaxxGetSlotIndexAndCount only indexes within a group, which is what the spread wants
// and the stack does not.
std::pair<size_t, size_t> SwarmSlot(PlayerbotAI* botAI, Player* bot)
{
    NaxxRoleGroups groups = NaxxGetRoleGroups(botAI, bot);
    std::vector<Player*> all;
    all.insert(all.end(), groups.healers.begin(), groups.healers.end());
    all.insert(all.end(), groups.rangedDps.begin(), groups.rangedDps.end());
    all.insert(all.end(), groups.meleeDps.begin(), groups.meleeDps.end());

    auto it = std::find(all.begin(), all.end(), bot);
    if (it == all.end())
    {
        return {0, 1};
    }
    return {static_cast<size_t>(std::distance(all.begin(), it)), all.size()};
}

// A spread slot is an offset from the boss, so a boss that walks toward a wall can push it straight
// through one. Pull anything past the room boundary back onto it.
void ClampToRoom(float& x, float& y)
{
    float dx = x - AnubrekhanBossHelper::RoomCenterX;
    float dy = y - AnubrekhanBossHelper::RoomCenterY;
    float dist = std::hypot(dx, dy);
    if (dist <= AnubrekhanBossHelper::MaxHoldRadius)
    {
        return;
    }
    float scale = AnubrekhanBossHelper::MaxHoldRadius / dist;
    x = AnubrekhanBossHelper::RoomCenterX + dx * scale;
    y = AnubrekhanBossHelper::RoomCenterY + dy * scale;
}
} // namespace

bool AnubrekhanChooseTargetAction::Execute(Event event)
{
    if (!helper.UpdateBossAI())
    {
        return false;
    }

    Unit* boss = helper.GetBoss();
    std::vector<Unit*> cryptGuards = helper.GetCryptGuards();
    std::vector<Unit*> corpseScarabs = helper.GetCorpseScarabs();
    if (!boss && cryptGuards.empty() && corpseScarabs.empty())
    {
        return false;
    }

    Unit* target = nullptr;
    if (botAI->IsMainTank(bot))
    {
        target = boss;
    }
    else if (botAI->IsAssistTank(bot))
    {
        // Newest guard first: the one the Locust Swarm just dropped is the one nobody holds yet.
        for (auto it = cryptGuards.rbegin(); it != cryptGuards.rend(); ++it)
        {
            Player* victim = (*it)->GetVictim() ? (*it)->GetVictim()->ToPlayer() : nullptr;
            if (!victim || !botAI->IsTank(victim))
            {
                target = *it;
                break;
            }
        }
        if (!target)
        {
            target = cryptGuards.empty() ? boss : cryptGuards.front();
        }
    }
    else if (!cryptGuards.empty())
    {
        // Lowest GUID, not lowest health: the health order changes every tick as the adds trade
        // places, and the whole raid used to re-target with it.
        target = cryptGuards.front();
    }
    else if (!corpseScarabs.empty())
    {
        for (Unit* scarab : corpseScarabs)
        {
            Player* victim = scarab->GetVictim() ? scarab->GetVictim()->ToPlayer() : nullptr;
            if (victim && botAI->IsHeal(victim))
            {
                target = scarab;
                break;
            }
        }
        if (!target)
        {
            for (Unit* scarab : corpseScarabs)
            {
                Player* victim = scarab->GetVictim() ? scarab->GetVictim()->ToPlayer() : nullptr;
                if (victim && !botAI->IsTank(victim))
                {
                    target = scarab;
                    break;
                }
            }
        }
        if (!target)
        {
            target = corpseScarabs.front();
        }
    }
    else
    {
        target = boss;
    }

    if (!target)
    {
        return false;
    }
    if (AI_VALUE(Unit*, "current target") == target)
    {
        return false;
    }
    return Attack(target);
}

bool AnubrekhanPositionAction::Execute(Event event)
{
    if (!helper.UpdateBossAI())
    {
        return false;
    }
    Unit* boss = helper.GetBoss();
    if (!boss)
    {
        return false;
    }

    bool swarm = helper.IsSwarmFormation();
    if (botAI->IsMainTank(bot))
    {
        // Locust Swarm neither roots nor threat-wipes the boss, so he keeps chasing - outside the
        // swarm there is nothing to kite and normal tanking should own the position.
        return swarm ? KiteBoss() : false;
    }
    if (botAI->IsAssistTank(bot))
    {
        return HoldAdds(boss);
    }
    if (swarm)
    {
        // The Impale spread is what puts people inside the swarm, so it goes away for the window.
        return TakeSwarmStack();
    }
    if (botAI->IsHeal(bot) || botAI->IsRanged(bot))
    {
        return TakeRangedSlot(boss);
    }
    // Melee stay on their target. Impale lands on a random player whatever anyone does, and walking
    // out of melee range for it costs more uptime than the splash it would save.
    Unit* target = AI_VALUE(Unit*, "current target");
    if (target && !bot->IsWithinMeleeRange(target) &&
        bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != CHASE_MOTION_TYPE)
    {
        // The swarm stack leaves a point-move behind, and nothing here re-issues the chase once the
        // window ends - without this melee just stand where the stack put them.
        return ChaseTo(target, sPlayerbotAIConfig.meleeDistance);
    }
    return false;
}

bool AnubrekhanPositionAction::TakeSwarmStack()
{
    // The spread is gone for the window, so it gets re-solved from wherever the boss ends up once the
    // swarm drops rather than sending everyone back to a slot from before the kite.
    AnubrekhanBossHelper::SlotStateFor(bot->GetGUID()).hasSpread = false;

    std::pair<size_t, size_t> slot = SwarmSlot(botAI, bot);
    float theta =
        2.0f * static_cast<float>(M_PI) * static_cast<float>(slot.first) / static_cast<float>(slot.second);
    float x = AnubrekhanBossHelper::RoomCenterX + std::cos(theta) * AnubrekhanBossHelper::SwarmStackRingRadius;
    float y = AnubrekhanBossHelper::RoomCenterY + std::sin(theta) * AnubrekhanBossHelper::SwarmStackRingRadius;
    return MoveToSlot(x, y);
}

bool AnubrekhanPositionAction::KiteBoss()
{
    uint32 nearest = FindNearestWaypoint();
    uint32 nextPoint = (nearest + 1) % intervals;
    return MoveTo(NAXX_MAP_ID, waypoints[nextPoint].first, waypoints[nextPoint].second,
                  AnubrekhanBossHelper::RoomFloorZ, false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT);
}

bool AnubrekhanPositionAction::HoldAdds(Unit* boss)
{
    if (helper.GetCryptGuards().empty())
    {
        return false;
    }

    // Park the adds on the far side of the boss from the room centre. The ranged ring sits on the
    // near side, and Crypt Guard cleave would otherwise land in it.
    float outward = std::atan2(boss->GetPositionY() - AnubrekhanBossHelper::RoomCenterY,
                               boss->GetPositionX() - AnubrekhanBossHelper::RoomCenterX);
    float bossRadius = boss->GetExactDist2d(AnubrekhanBossHelper::RoomCenterX, AnubrekhanBossHelper::RoomCenterY);
    // The wall cap can shorten the step out, but never past the boss - the far side is the whole
    // point, and anything short of it is where the ranged are standing.
    float holdRadius = std::max(
        bossRadius, std::min(bossRadius + AnubrekhanBossHelper::AddHoldDistance, AnubrekhanBossHelper::MaxHoldRadius));

    float x = AnubrekhanBossHelper::RoomCenterX + std::cos(outward) * holdRadius;
    float y = AnubrekhanBossHelper::RoomCenterY + std::sin(outward) * holdRadius;
    return MoveToSlot(x, y);
}

bool AnubrekhanPositionAction::TakeRangedSlot(Unit* boss)
{
    AnubrekhanBossHelper::SlotState& state = AnubrekhanBossHelper::SlotStateFor(bot->GetGUID());

    if (!state.hasSpread)
    {
        NaxxRoleGroups groups = NaxxGetRoleGroups(botAI, bot);
        std::pair<size_t, size_t> slot = NaxxGetSlotIndexAndCount(botAI, bot, groups);

        // The arc is anchored on the bearing from the boss to the room centre, which keeps it inside
        // the room - but only read once. That bearing swings hard whenever the boss is near the
        // centre, and re-reading it every tick swapped bots between opposite ends of the arc.
        float anchor = std::atan2(AnubrekhanBossHelper::RoomCenterY - boss->GetPositionY(),
                                  AnubrekhanBossHelper::RoomCenterX - boss->GetPositionX());
        state.spreadAngle = anchor - RangedRingArc / 2.0f +
                            RangedRingArc * (static_cast<float>(slot.first) + 0.5f) / static_cast<float>(slot.second);
        state.spreadRadius = botAI->IsHeal(bot) ? AnubrekhanBossHelper::RangedBandMax
                                                : AnubrekhanBossHelper::RangedBandMin + RangedDpsBandOffset;
        state.hasSpread = true;
    }

    float x = boss->GetPositionX() + std::cos(state.spreadAngle) * state.spreadRadius;
    float y = boss->GetPositionY() + std::sin(state.spreadAngle) * state.spreadRadius;
    ClampToRoom(x, y);

    // Standing apart is the whole defence against Impale - chasing the slot around is not. While the
    // boss stays within a step of where he was, the bot holds the spot it already walked to instead
    // of trailing him yard for yard.
    if (state.hasDest)
    {
        float driftX = x - state.destX;
        float driftY = y - state.destY;
        if (driftX * driftX + driftY * driftY <= AnchorDriftDistance * AnchorDriftDistance)
        {
            x = state.destX;
            y = state.destY;
        }
    }
    return MoveToSlot(x, y);
}

bool AnubrekhanPositionAction::MoveToSlot(float x, float y)
{
    if (bot->GetExactDist2d(x, y) <= AnubrekhanBossHelper::SlotTolerance)
    {
        return false;
    }

    AnubrekhanBossHelper::SlotState& state = AnubrekhanBossHelper::SlotStateFor(bot->GetGUID());
    uint32 now = getMSTime();
    float driftX = state.destX - x;
    float driftY = state.destY - y;
    bool sameDestination =
        state.hasDest && (driftX * driftX + driftY * driftY) <= AnchorDriftDistance * AnchorDriftDistance;

    // Re-issuing the same move every tick makes bots jitter in place instead of casting. Let an order
    // that is still valid run.
    if (sameDestination && getMSTimeDiff(state.lastMoveMs, now) < AnubrekhanBossHelper::RepositionIntervalMs &&
        !helper.IsSwarmImminent())
    {
        return false;
    }

    state.lastMoveMs = now;
    state.destX = x;
    state.destY = y;
    state.hasDest = true;
    return MoveTo(NAXX_MAP_ID, x, y, AnubrekhanBossHelper::RoomFloorZ, false, false, false, false,
                  MovementPriority::MOVEMENT_COMBAT);
}

Player* AnubrekhanRedirectThreatAction::GetRedirectTank()
{
    if (!helper.UpdateBossAI())
    {
        return nullptr;
    }
    Unit* boss = helper.GetBoss();
    Player* tank = boss ? GetTankHolding(boss) : nullptr;
    return tank ? tank : GetGroupMainTank(botAI, bot);
}

Unit* AnubrekhanRedirectThreatAction::GetThreatDumpTarget()
{
    return helper.UpdateBossAI() ? helper.GetBoss() : nullptr;
}
