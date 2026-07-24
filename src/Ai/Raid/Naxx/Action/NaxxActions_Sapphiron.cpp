#include "NaxxActions.h"

#include <algorithm>
#include <limits>

#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "NaxxBossHelper.h"
#include "NaxxSpellIds.h"

bool SapphironGroundPositionAction::Execute(Event event)
{
    if (!helper.UpdateBossAI())
    {
        return false;
    }
    if (botAI->IsHeal(bot) && helper.HasLifeDrainInGroup() && !botAI->HasStrategy("cure", BOT_STATE_COMBAT))
    {
        botAI->ChangeStrategy("cure", BOT_STATE_COMBAT);
    }
    if (botAI->IsMainTank(bot))
    {
        if (AI_VALUE2(bool, "has aggro", "current target"))
        {
            return MoveTo(NAXX_MAP_ID, helper.mainTankPos.first, helper.mainTankPos.second, helper.GENERIC_HEIGHT, false, false, false,
                          false, MovementPriority::MOVEMENT_COMBAT);
        }
        return false;
    }
    Unit* boss = AI_VALUE2(Unit*, "find target", "sapphiron");
    if (boss && helper.IsPhaseGround())
    {
        bool needsSideStack = boss->isInFront(bot) || boss->isInBack(bot);
        if (!needsSideStack)
        {
            needsSideStack = NaxxSpellIds::HasAnyAura(bot, {NaxxSpellIds::LifeDrain}) || botAI->HasAura("life drain", bot);
        }
        if (needsSideStack)
        {
            float distance;
            if (botAI->IsRanged(bot) || botAI->IsHeal(bot) ||
                NaxxSpellIds::HasAnyAura(bot, {NaxxSpellIds::LifeDrain}) || botAI->HasAura("life drain", bot))
            {
                distance = 30.0f;
            }
            else
            {
                distance = 5.0f;
            }
            float angle = boss->GetOrientation() + M_PI / 2;
            float posX = boss->GetPositionX() + cos(angle) * distance;
            float posY = boss->GetPositionY() + sin(angle) * distance;
            if (MoveTo(NAXX_MAP_ID, posX, posY, helper.GENERIC_HEIGHT, false, false, false, false, MovementPriority::MOVEMENT_COMBAT))
            {
                return true;
            }
            return MoveInside(NAXX_MAP_ID, posX, posY, helper.GENERIC_HEIGHT, 2.0f, MovementPriority::MOVEMENT_COMBAT);
        }
    }
    if (helper.JustLanded())
    {
        uint32 index = botAI->GetGroupSlotIndex(bot);
        float start_angle = 0.85 * M_PI;
        float offset_angle = M_PI * 0.02 * index;
        float angle = start_angle + offset_angle;
        float distance;
        if (botAI->IsRanged(bot))
        {
            distance = 35.0f;
        }
        else if (botAI->IsHeal(bot))
        {
            distance = 30.0f;
        }
        else
        {
            distance = 5.0f;
        }
        float posX = helper.center.first + cos(angle) * distance;
        float posY = helper.center.second + sin(angle) * distance;
        if (MoveTo(NAXX_MAP_ID, posX, posY, helper.GENERIC_HEIGHT, false, false, false, false, MovementPriority::MOVEMENT_COMBAT))
        {
            return true;
        }
        return MoveInside(NAXX_MAP_ID, posX, posY, helper.GENERIC_HEIGHT, 2.0f, MovementPriority::MOVEMENT_COMBAT);
    }
    else
    {
        std::vector<float> dest;
        if (helper.FindPosToAvoidChill(dest))
        {
            return MoveTo(NAXX_MAP_ID, dest[0], dest[1], dest[2], false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
        }
    }
    return false;
}

bool SapphironFlightPositionAction::Execute(Event event)
{
    if (!helper.UpdateBossAI())
    {
        return false;
    }
    if (botAI->IsHeal(bot) && helper.HasLifeDrainInGroup() && !botAI->HasStrategy("cure", BOT_STATE_COMBAT))
    {
        botAI->ChangeStrategy("cure", BOT_STATE_COMBAT);
    }
    if (NaxxSpellIds::HasAnyAura(bot, {NaxxSpellIds::Icebolt10, NaxxSpellIds::Icebolt25}) ||
        botAI->HasAura("icebolt", bot, false, false, -1, true))
    {
        // This bot is now a block. Drop the latch so it re-shelters once thawed.
        ResetShelterLatch();
        return false;
    }
    if (helper.WaitForExplosion())
    {
        switch (MoveToNearestIcebolt())
        {
            case ShelterResult::Moving:
                // Own the tick while moving; a cast-time heal couldn't fire anyway.
                return true;
            case ShelterResult::Sheltered:
                // Stopped behind the block: let heals run; non-healers hold position.
                return botAI->IsHeal(bot) ? false : true;
            case ShelterResult::None:
            default:
                return false;
        }
    }
    else
    {
        ResetShelterLatch();
        std::vector<float> dest;
        if (helper.FindPosToAvoidChill(dest))
        {
            return MoveTo(NAXX_MAP_ID, dest[0], dest[1], dest[2], false, false, false, false, MovementPriority::MOVEMENT_COMBAT);
        }
    }
    return false;
}

void SapphironFlightPositionAction::ResetShelterLatch()
{
    sheltered = false;
    assignedBlockGuid = ObjectGuid::Empty;
}

SapphironFlightPositionAction::ShelterResult SapphironFlightPositionAction::MoveToNearestIcebolt()
{
    Group* group = bot->GetGroup();
    if (!group)
    {
        return ShelterResult::None;
    }
    Unit* boss = AI_VALUE2(Unit*, "find target", "sapphiron");
    if (!boss)
    {
        return ShelterResult::None;
    }
    std::vector<Player*> icebolts;
    icebolts.reserve(5);
    Player* nearest = nullptr;
    float nearestDist = std::numeric_limits<float>::max();
    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive())
        {
            continue;
        }
        if (NaxxSpellIds::HasAnyAura(member, {NaxxSpellIds::Icebolt10, NaxxSpellIds::Icebolt25}) ||
            botAI->HasAura("icebolt", member, false, false, -1, true))
        {
            icebolts.push_back(member);
            float d = bot->GetDistance(member);
            if (!nearest || d < nearestDist)
            {
                nearest = member;
                nearestDist = d;
            }
        }
    }
    if (icebolts.empty())
    {
        return ShelterResult::None;
    }

    // In 25-man the core may spawn only 3 iceblocks, so "nearest" can over-stack one
    // block. Assign a stable block per bot for the whole phase so the target does not
    // hop each tick: keep the latched block while it is still alive with Icebolt,
    // otherwise pick a fresh one by group-slot round-robin (sorted for determinism),
    // with a safety fallback to nearest if the pick is across the room.
    std::sort(icebolts.begin(), icebolts.end(), [](Player* a, Player* b)
    {
        return a->GetGUID().GetRawValue() < b->GetGUID().GetRawValue();
    });

    int32 slotIndex = botAI->GetGroupSlotIndex(bot);
    Player* playerWithIcebolt = nullptr;
    if (!assignedBlockGuid.IsEmpty())
    {
        for (Player* p : icebolts)
        {
            if (p->GetGUID() == assignedBlockGuid)
            {
                playerWithIcebolt = p;
                break;
            }
        }
    }
    if (!playerWithIcebolt)
    {
        playerWithIcebolt = icebolts[std::max<int32>(0, slotIndex) % icebolts.size()];
        if (nearest && playerWithIcebolt != nearest &&
            bot->GetDistance(playerWithIcebolt) > nearestDist + 20.0f)
        {
            playerWithIcebolt = nearest;
        }
        assignedBlockGuid = playerWithIcebolt->GetGUID();
        // Re-picking a block means we are no longer parked behind the old one.
        sheltered = false;
    }

    constexpr float shelterDistance = 9.0f;
    constexpr float lateralOffset = 1.5f;
    // LOS test tolerance and arrival/hold deadbands. Acceptance is behind-the-block
    // LOS within arriveDeadband (not an exact point); once sheltered we hold until LOS
    // is lost or the bot drifts past reEngageRadius. Boss/icebolt drift within this
    // band must NOT re-issue a move, or the bot never settles and can't cast.
    constexpr float losTolerance = 2.5f;
    constexpr float arriveDeadband = 3.5f;
    constexpr float reEngageRadius = 6.0f;

    float angle = boss->GetAngle(playerWithIcebolt);
    float posX = playerWithIcebolt->GetPositionX() + cos(angle) * shelterDistance;
    float posY = playerWithIcebolt->GetPositionY() + sin(angle) * shelterDistance;
    float posZ = playerWithIcebolt->GetPositionZ();
    float offsetSign = (slotIndex % 2 == 0) ? 1.0f : -1.0f;
    float offsetAngle = angle + (M_PI / 2.0f);
    float offsetX = cos(offsetAngle) * lateralOffset * offsetSign;
    float offsetY = sin(offsetAngle) * lateralOffset * offsetSign;
    float candidateX = posX + offsetX;
    float candidateY = posY + offsetY;
    float bossX = boss->GetPositionX();
    float bossY = boss->GetPositionY();
    float lineDx = candidateX - bossX;
    float lineDy = candidateY - bossY;
    float lineLen = sqrt(lineDx * lineDx + lineDy * lineDy);
    if (lineLen > 0.1f)
    {
        float relX = playerWithIcebolt->GetPositionX() - bossX;
        float relY = playerWithIcebolt->GetPositionY() - bossY;
        float proj = (relX * lineDx + relY * lineDy) / lineLen;
        float clamped = std::max(0.0f, std::min(lineLen, proj));
        float closestX = bossX + (lineDx / lineLen) * clamped;
        float closestY = bossY + (lineDy / lineLen) * clamped;
        float distToLine = playerWithIcebolt->GetDistance2d(closestX, closestY);
        if (distToLine <= 2.0f && playerWithIcebolt->IsWithinDist2d(candidateX, candidateY, 10.0f))
        {
            posX = candidateX;
            posY = candidateY;
        }
    }
    float bossDist = boss->GetDistance2d(bot);
    float iceboltDist = boss->GetDistance2d(playerWithIcebolt);
    if (bossDist <= iceboltDist + 0.5f)
    {
        posX = playerWithIcebolt->GetPositionX() + cos(angle) * (shelterDistance + 1.5f);
        posY = playerWithIcebolt->GetPositionY() + sin(angle) * (shelterDistance + 1.5f);
    }

    bool losOk = playerWithIcebolt->IsInBetween(boss, bot, losTolerance);
    float distToLosPos = bot->GetDistance2d(posX, posY);

    // Hysteresis hold: stay parked as long as LOS holds and we haven't drifted far.
    if (sheltered)
    {
        if (losOk && distToLosPos <= reEngageRadius)
        {
            if (bot->isMoving())
            {
                bot->StopMoving();
            }
            return ShelterResult::Sheltered;
        }
        sheltered = false;  // lost shelter -> re-engage below
    }

    // Accept as sheltered once we have block LOS within the arrival deadband, then stop.
    if (losOk && distToLosPos <= arriveDeadband)
    {
        sheltered = true;
        if (bot->isMoving())
        {
            bot->StopMoving();
        }
        return ShelterResult::Sheltered;
    }

    MoveTo(NAXX_MAP_ID, posX, posY, posZ, false, false, false, true, MovementPriority::MOVEMENT_FORCED);
    return ShelterResult::Moving;
}