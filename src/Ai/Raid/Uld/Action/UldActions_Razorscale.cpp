#include "UldActions_Razorscale.h"
#include "UldActions_Shared.h"

#include <CombatStrategy.h>
#include <FollowMasterStrategy.h>

#include <algorithm>
#include <cmath>

#include "AiObjectContext.h"
#include "DBCEnums.h"
#include "GameObject.h"
#include "Group.h"
#include "LastMovementValue.h"
#include "Map.h"
#include "ObjectGuid.h"
#include "PlayerbotAI.h"
#include "PlayerbotAIConfig.h"
#include "Playerbots.h"
#include "Position.h"
#include "UldBossHelper.h"
#include "UldScripts.h"
#include "RaidBossHelpers.h"
#include "RtiValue.h"
#include "ScriptedCreature.h"
#include "ServerFacade.h"
#include "Unit.h"
#include "Vehicle.h"
#include <RtiTargetValue.h>
#include <TankAssistStrategy.h>

float RazorscaleAvoidDevouringFlameAction::ClearRadius()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "razorscale");
    bool const airborne = boss && boss->GetPositionZ() >= RazorscaleBossHelper::RAZORSCALE_FLYING_Z_THRESHOLD;
    float const multiplier = (botAI->IsMainTank(bot) && airborne) ? 2.3f : 1.0f;

    return RazorscaleBossHelper::DEVOURING_FLAME_CLEAR_RADIUS * multiplier;
}

bool RazorscaleAvoidDevouringFlameAction::ReturnSpotBlocked()
{
    // Ranged never has to walk back onto the boss, and holding them here would fight the grounded
    // stack-up for no gain. Melee and both tanks are the ones the reach actions drag into the patch.
    if (!botAI->IsMelee(bot))
        return false;

    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target)
        return false;

    return RazorscaleBossHelper::DevouringFlameBlocks(bot, target->GetPositionX(), target->GetPositionY());
}

bool RazorscaleAvoidDevouringFlameAction::StepClearOfFlames(Unit* flame, float clearRadius)
{
    // Measured from where the bot is standing, not from the patch: MoveAway and MoveTo both step by a
    // distance rather than to one, so a bot on the centre needs the whole radius.
    float const step = std::max(clearRadius - bot->GetDistance2d(flame) + 1.0f, 1.0f);
    float const initAngle = flame->GetAngle(bot);

    for (float delta = 0.0f; delta <= M_PI / 2.0f; delta += M_PI / 8.0f)
    {
        for (float sign : {1.0f, -1.0f})
        {
            if (delta == 0.0f && sign < 0.0f)
                continue;

            float const angle = initAngle + sign * delta;
            float x = bot->GetPositionX() + std::cos(angle) * step;
            float y = bot->GetPositionY() + std::sin(angle) * step;
            float z = bot->GetPositionZ();

            if (!bot->GetMap()->CheckCollisionAndGetValidCoords(bot, bot->GetPositionX(), bot->GetPositionY(),
                                                                bot->GetPositionZ(), x, y, z))
                continue;

            // A destination inside the next patch is what turns one dodge into a chain of them. She
            // drops these every 6-12s and they stack up, so the whole arena has to be consulted.
            if (RazorscaleBossHelper::DevouringFlameBlocks(bot, x, y))
                continue;

            if (MoveTo(bot->GetMapId(), x, y, z, false, false, true, true, MovementPriority::MOVEMENT_COMBAT))
                return true;
        }
    }

    // Nothing validated clear. Any step out beats standing in it.
    return MoveAway(flame, step);
}

bool RazorscaleAvoidDevouringFlameAction::Execute(Event /*event*/)
{
    float const clearRadius = ClearRadius();

    if (Unit* flame = RazorscaleBossHelper::FindDevouringFlameNear(botAI, clearRadius))
        return StepClearOfFlames(flame, clearRadius);

    // Standing clear already. Consuming the tick without moving is what stops "razorscale grounded",
    // the reach actions and the formation step from walking the bot straight back in; casting is
    // untouched. It releases as soon as the tank has dragged her off the patch.
    return ReturnSpotBlocked();
}

bool RazorscaleAvoidDevouringFlameAction::isUseful()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "razorscale");
    if (!boss)
        return false;

    if (RazorscaleBossHelper::FindDevouringFlameNear(botAI, ClearRadius()))
        return true;

    return ReturnSpotBlocked();
}

bool RazorscaleAvoidSentinelAction::Execute(Event /*event*/)
{
    // Marking is not done here - "razorscale kill target action" owns the skull for the whole fight,
    // so it can hand it to the boss the moment she lands.
    if (!botAI->IsRanged(bot))
        return false;

    const float radius = 8.0f;
    GuidVector npcs = AI_VALUE(GuidVector, "nearest hostile npcs");

    bool movedAway = false;
    for (auto& npc : npcs)
    {
        Unit* unit = botAI->GetUnit(npc);
        if (unit && unit->GetEntry() == RazorscaleBossHelper::UNIT_DARK_RUNE_SENTINEL)
        {
            if (bot->GetDistance2d(unit) < radius)
                movedAway = MoveAway(unit, radius) || movedAway;
        }
    }

    return movedAway;
}

bool RazorscaleAvoidSentinelAction::isUseful()
{
    if (!botAI->IsRanged(bot))
        return false;

    const float radius = 8.0f;
    GuidVector npcs = AI_VALUE(GuidVector, "nearest hostile npcs");
    for (auto& npc : npcs)
    {
        Unit* unit = botAI->GetUnit(npc);
        if (unit && unit->GetEntry() == RazorscaleBossHelper::UNIT_DARK_RUNE_SENTINEL)
        {
            if (bot->GetDistance2d(unit) < radius)
                return true;
        }
    }

    return false;
}

bool RazorscaleAvoidWhirlwindAction::Execute(Event /*event*/)
{
    if (botAI->IsTank(bot))
    {
        return false;
    }

    const float radius = 8.0f;
    GuidVector npcs = AI_VALUE(GuidVector, "nearest hostile npcs");
    for (auto& npc : npcs)
    {
        Unit* unit = botAI->GetUnit(npc);
        if (unit && unit->GetEntry() == RazorscaleBossHelper::UNIT_DARK_RUNE_SENTINEL)
        {
            float currentDistance = bot->GetDistance2d(unit);
            if (currentDistance < radius)
                return MoveAway(unit, radius);
        }
    }
    return false;
}

bool RazorscaleAvoidWhirlwindAction::isUseful()
{
    // Tanks do not avoid Whirlwind
    if (botAI->IsTank(bot))
    {
        return false;
    }

    const float radius = 8.0f;
    GuidVector npcs = AI_VALUE(GuidVector, "nearest hostile npcs");
    for (auto& npc : npcs)
    {
        Unit* unit = botAI->GetUnit(npc);
        if (unit && unit->GetEntry() == RazorscaleBossHelper::UNIT_DARK_RUNE_SENTINEL)
        {
            if (unit->HasAura(RazorscaleBossHelper::SPELL_SENTINEL_WHIRLWIND) ||
                unit->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
            {
                if (bot->GetDistance2d(unit) < radius)
                {
                    return true;
                }
            }
        }
    }

    return false;
}

bool RazorscaleIgnoreBossAction::isUseful()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "razorscale");
    if (!boss)
    {
        return false;
    }

    // Check if the boss is flying
    if (boss->GetPositionZ() >= RazorscaleBossHelper::RAZORSCALE_FLYING_Z_THRESHOLD)
    {
        // Check if the bot is outside the designated area
        if (bot->GetDistance2d(RazorscaleBossHelper::RAZORSCALE_ARENA_CENTER_X,
                               RazorscaleBossHelper::RAZORSCALE_ARENA_CENTER_Y) >
            RazorscaleBossHelper::RAZORSCALE_ARENA_RADIUS + 25.0f)
        {
            return true;  // Movement to the center is the top priority for all bots
        }

        if (!botAI->IsTank(bot))
            return false;

        Group* group = bot->GetGroup();
        if (!group)
            return false;

        // Check if the boss is already set as the moon marker
        int8 moonIndex = 4;  // Moon marker index
        ObjectGuid currentMoonTarget = group->GetTargetIcon(moonIndex);
        if (currentMoonTarget == boss->GetGUID())
            return false;  // Moon marker is already correctly set, no further action needed

        // Proceed to tank-specific logic
        Unit* mainTankUnit = AI_VALUE(Unit*, "main tank");
        Player* mainTank = mainTankUnit ? mainTankUnit->ToPlayer() : nullptr;

        // If this bot is the main tank, it needs to set the moon marker
        if (mainTankUnit == bot)
            return true;

        // If the main tank is a human, check if this bot is the lowest-indexed bot tank
        if (mainTank && !GET_PLAYERBOT_AI(mainTank))  // Main tank is a human player
        {
            for (int i = 0; i < 3; ++i)  // Only iterate through the first 3 indexes
            {
                if (botAI->IsAssistTankOfIndex(bot, i) && GET_PLAYERBOT_AI(bot))  // Valid bot tank
                    return true;  // This bot should assign the marker
            }
        }
    }

    return false;
}

bool RazorscaleIgnoreBossAction::Execute(Event /*event*/)
{
    if (!bot)
        return false;

    Unit* boss = AI_VALUE2(Unit*, "find target", "razorscale");
    if (!boss)
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    // Check if the bot is outside the designated area and move inside first
    if (bot->GetDistance2d(RazorscaleBossHelper::RAZORSCALE_ARENA_CENTER_X,
                           RazorscaleBossHelper::RAZORSCALE_ARENA_CENTER_Y) >
        RazorscaleBossHelper::RAZORSCALE_ARENA_RADIUS + 25.0f)
    {
        return MoveInside(ULDUAR_MAP_ID, RazorscaleBossHelper::RAZORSCALE_ARENA_CENTER_X,
                          RazorscaleBossHelper::RAZORSCALE_ARENA_CENTER_Y, bot->GetPositionZ(),
                          RazorscaleBossHelper::RAZORSCALE_ARENA_RADIUS - 10.0f, MovementPriority::MOVEMENT_NORMAL);
    }

    if (!botAI->IsTank(bot))
        return false;

    // Check if the boss is already set as the moon marker
    int8 moonIndex = 4;
    ObjectGuid currentMoonTarget = group->GetTargetIcon(moonIndex);
    if (currentMoonTarget == boss->GetGUID())
        return false;  // Moon marker is already correctly set

    // Get the main tank and determine role
    Unit* mainTankUnit = AI_VALUE(Unit*, "main tank");
    Player* mainTank = mainTankUnit ? mainTankUnit->ToPlayer() : nullptr;

    // If the main tank is a human, assign the moon marker using the lowest-indexed bot tank
    if (mainTank && !GET_PLAYERBOT_AI(mainTank))  // Main tank is a real player
    {
        for (int i = 0; i < 3; ++i)  // Only iterate through the first 3 indexes
        {
            if (botAI->IsAssistTankOfIndex(bot, i) && GET_PLAYERBOT_AI(bot))  // Bot is a valid tank
            {
                group->SetTargetIcon(moonIndex, bot->GetGUID(), boss->GetGUID());
                SetNextMovementDelay(1000);
                break;  // Assign the moon marker and stop
            }
        }
    }
    else if (mainTankUnit == bot)  // If this bot is the main tank
    {
        group->SetTargetIcon(moonIndex, bot->GetGUID(), boss->GetGUID());
        SetNextMovementDelay(1000);
    }

    // Tanks move inside the arena
    return MoveInside(ULDUAR_MAP_ID, RazorscaleBossHelper::RAZORSCALE_ARENA_CENTER_X,
                      RazorscaleBossHelper::RAZORSCALE_ARENA_CENTER_Y, bot->GetPositionZ(),
                      RazorscaleBossHelper::RAZORSCALE_ARENA_RADIUS - 10.0f, MovementPriority::MOVEMENT_NORMAL);
}

bool RazorscaleGroundedAction::isUseful()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "razorscale");
    if (!boss || !boss->IsAlive() || boss->GetPositionZ() > RazorscaleBossHelper::RAZORSCALE_FLYING_Z_THRESHOLD)
        return false;

    if (botAI->IsMainTank(bot))
    {
        Group* group = bot->GetGroup();
        if (!group)
            return false;

        // Check if the boss is marked with Moon
        int8 moonIndex = 4;
        ObjectGuid currentMoonTarget = group->GetTargetIcon(moonIndex);

        // Useful only if the boss is currently marked with Moon
        return currentMoonTarget == boss->GetGUID();
    }

    if (botAI->IsTank(bot) && !botAI->IsMainTank(bot))
    {
        Group* group = bot->GetGroup();
        if (!group)
            return false;

        // Find the main tank
        Player* mainTank = nullptr;
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->GetSource();
            if (member && botAI->IsMainTank(member))
            {
                mainTank = member;
                break;
            }
        }

        if (mainTank)
        {
            constexpr float maxDistance = 2.0f;
            float distanceToMainTank = bot->GetDistance2d(mainTank);
            return (distanceToMainTank > maxDistance);
        }
    }

    if (botAI->IsMelee(bot))
        return false;

    if (botAI->IsRanged(bot))
    {
        constexpr float landingX = 588.0f;
        constexpr float landingY = -166.0f;
        constexpr float landingZ = 391.1f;

        float bossX = boss->GetPositionX();
        float bossY = boss->GetPositionY();
        float bossZ = boss->GetPositionZ();

        bool atInitialLandingPosition =
            (fabs(bossX - landingX) < 2.0f) && (fabs(bossY - landingY) < 2.0f) && (fabs(bossZ - landingZ) < 1.0f);

        constexpr float initialLandingRadius = 14.0f;
        constexpr float normalRadius = 12.0f;

        if (atInitialLandingPosition)
        {
            float adjustedCenterX = RazorscaleBossHelper::RAZORSCALE_ARENA_CENTER_X;
            float adjustedCenterY = RazorscaleBossHelper::RAZORSCALE_ARENA_CENTER_Y - 20.0f;

            float distanceToAdjustedCenter = bot->GetDistance2d(adjustedCenterX, adjustedCenterY);
            return distanceToAdjustedCenter > initialLandingRadius;
        }

        float distanceToCenter = bot->GetDistance2d(RazorscaleBossHelper::RAZORSCALE_ARENA_CENTER_X,
                                                    RazorscaleBossHelper::RAZORSCALE_ARENA_CENTER_Y);
        return distanceToCenter > normalRadius;
    }

    return false;
}

bool RazorscaleGroundedAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "razorscale");
    if (!boss || !boss->IsAlive() || boss->GetPositionZ() > RazorscaleBossHelper::RAZORSCALE_FLYING_Z_THRESHOLD)
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    Unit* mainTankUnit = AI_VALUE(Unit*, "main tank");
    Player* mainTank = mainTankUnit ? mainTankUnit->ToPlayer() : nullptr;

    if (mainTank && !GET_PLAYERBOT_AI(mainTank))  // Main tank is a human player
    {
        // Iterate through the first 3 bot tanks to handle the moon marker
        for (int i = 0; i < 3; ++i)
        {
            if (botAI->IsAssistTankOfIndex(bot, i) && GET_PLAYERBOT_AI(bot))  // Bot is a valid tank
            {
                int8 moonIndex = 4;
                ObjectGuid currentMoonTarget = group->GetTargetIcon(moonIndex);

                // If the moon marker is set to the boss, reset it
                if (currentMoonTarget == boss->GetGUID())
                {
                    group->SetTargetIcon(moonIndex, bot->GetGUID(), ObjectGuid::Empty);
                    SetNextMovementDelay(1000);
                    return true;
                }
            }
        }
    }
    else if (botAI->IsMainTank(bot))  // Bot is the main tank
    {
        int8 moonIndex = 4;
        ObjectGuid currentMoonTarget = group->GetTargetIcon(moonIndex);

        // If the moon marker is set to the boss, reset it
        if (currentMoonTarget == boss->GetGUID())
        {
            group->SetTargetIcon(moonIndex, bot->GetGUID(), ObjectGuid::Empty);
            SetNextMovementDelay(1000);
            return true;
        }
    }

    if (mainTank && (botAI->IsTank(bot) && !botAI->IsMainTank(bot)))
    {
        constexpr float followDistance = 2.0f;
        return MoveNear(mainTank, followDistance, MovementPriority::MOVEMENT_COMBAT);
    }

    if (botAI->IsRanged(bot))
    {
        constexpr float landingX = 588.0f;
        constexpr float landingY = -166.0f;
        constexpr float landingZ = 391.1f;

        float bossX = boss->GetPositionX();
        float bossY = boss->GetPositionY();
        float bossZ = boss->GetPositionZ();

        bool atInitialLandingPosition =
            (fabs(bossX - landingX) < 2.0f) && (fabs(bossY - landingY) < 2.0f) && (fabs(bossZ - landingZ) < 1.0f);

        if (atInitialLandingPosition)
        {
            // If at the initial landing position, use 12-yard radius with a
            // 20 yard offset on the Y axis so everyone is behind the boss
            return MoveInside(ULDUAR_MAP_ID, RazorscaleBossHelper::RAZORSCALE_ARENA_CENTER_X,
                              RazorscaleBossHelper::RAZORSCALE_ARENA_CENTER_Y - 20.0f, bot->GetPositionZ(),
                              RazorscaleBossHelper::RAZORSCALE_ARENA_RADIUS - 12.0f, MovementPriority::MOVEMENT_COMBAT);
        }

        // Otherwise, move inside a 12-yard radius around the arena center
        return MoveInside(ULDUAR_MAP_ID, RazorscaleBossHelper::RAZORSCALE_ARENA_CENTER_X,
                          RazorscaleBossHelper::RAZORSCALE_ARENA_CENTER_Y, bot->GetPositionZ(), 12.0f,
                          MovementPriority::MOVEMENT_COMBAT);
    }
    return false;
}

bool RazorscaleHarpoonAction::Execute(Event /*event*/)
{
    if (!bot)
        return false;

    RazorscaleBossHelper razorscaleHelper(botAI);

    // Update the boss AI context
    if (!razorscaleHelper.UpdateBossAI())
        return false;

    Unit* boss = razorscaleHelper.GetBoss();
    if (!boss || !boss->IsAlive())
        return false;

    // Retrieve harpoon data from the helper
    const std::vector<RazorscaleBossHelper::HarpoonData>& harpoonData = razorscaleHelper.GetHarpoonData();

    GameObject* closestHarpoon = nullptr;
    float minDistance = std::numeric_limits<float>::max();

    // Find the nearest harpoon that hasn't been fired and is not on cooldown
    for (auto const& harpoon : harpoonData)
    {
        if (GameObject* harpoonGO = bot->FindNearestGameObject(harpoon.gameObjectEntry, 200.0f))
        {
            if (RazorscaleBossHelper::IsHarpoonReady(harpoonGO))
            {
                float distance = bot->GetDistance2d(harpoonGO);
                if (distance < minDistance)
                {
                    minDistance = distance;
                    closestHarpoon = harpoonGO;
                }
            }
        }
    }

    if (!closestHarpoon)
        return false;

    // Find the nearest ranged DPS (not a healer) to the harpoon
    Player* closestRangedDPS = nullptr;
    minDistance = std::numeric_limits<float>::max();
    GuidVector groupBots = AI_VALUE(GuidVector, "group members");

    for (auto& guid : groupBots)
    {
        Player* member = ObjectAccessor::FindPlayer(guid);
        if (member && member->IsAlive() && botAI->IsRanged(member) && botAI->IsDps(member) && !botAI->IsHeal(member))
        {
            float distance = member->GetDistance2d(closestHarpoon);
            if (distance < minDistance)
            {
                minDistance = distance;
                closestRangedDPS = member;
            }
        }
    }

    // Only proceed if this bot is the closest ranged DPS
    if (closestRangedDPS != bot)
        return false;

    float botDist = bot->GetDistance(closestHarpoon);
    if (botDist > INTERACTION_DISTANCE - 1.0f)
    {
        return MoveTo(bot->GetMapId(), closestHarpoon->GetPositionX(), closestHarpoon->GetPositionY(),
                      closestHarpoon->GetPositionZ());
    }

    SetNextMovementDelay(1000);

    // Interact with the harpoon
    {
        WorldPacket usePacket(CMSG_GAMEOBJ_USE);
        usePacket << closestHarpoon->GetGUID();
        bot->GetSession()->HandleGameObjectUseOpcode(usePacket);
    }

    {
        WorldPacket reportPacket(CMSG_GAMEOBJ_REPORT_USE);
        reportPacket << closestHarpoon->GetGUID();
        bot->GetSession()->HandleGameobjectReportUse(reportPacket);
    }

    RazorscaleBossHelper::SetHarpoonOnCooldown(closestHarpoon);

    return true;
}

bool RazorscaleHarpoonAction::isUseful()
{
    RazorscaleBossHelper razorscaleHelper(botAI);

    // Update the boss AI context to ensure we have the latest info
    if (!razorscaleHelper.UpdateBossAI())
        return false;

    Unit* boss = razorscaleHelper.GetBoss();
    if (!boss || !boss->IsAlive())
        return false;

    const std::vector<RazorscaleBossHelper::HarpoonData>& harpoonData = razorscaleHelper.GetHarpoonData();

    for (auto const& harpoon : harpoonData)
    {
        if (GameObject* harpoonGO = bot->FindNearestGameObject(harpoon.gameObjectEntry, 200.0f))
        {
            if (RazorscaleBossHelper::IsHarpoonReady(harpoonGO))
            {
                // Check if this bot is a ranged DPS (not a healer)
                if (botAI->IsRanged(bot) && botAI->IsDps(bot) && !botAI->IsHeal(bot))
                    return true;
            }
        }
    }

    return false;
}

bool RazorscaleFuseArmorAction::isUseful()
{
    // If this bot cannot tank at all, no need to do anything
    if (!botAI->IsTank(bot))
        return false;

    // If this bot is the main tank AND has Fuse Armor at the threshold, return true immediately
    if (botAI->IsMainTank(bot))
    {
        Aura* fuseArmor = bot->GetAura(RazorscaleBossHelper::SPELL_FUSE_ARMOR);
        if (fuseArmor && fuseArmor->GetStackAmount() >= RazorscaleBossHelper::FUSEARMOR_THRESHOLD)
            return true;
    }

    // Otherwise, check if there's any other main tank with high Fuse Armor
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* member = gref->GetSource();
        if (!member)
            continue;

        if (botAI->IsMainTank(member) && member != bot)
        {
            Aura* fuseArmor = member->GetAura(RazorscaleBossHelper::SPELL_FUSE_ARMOR);
            if (fuseArmor && fuseArmor->GetStackAmount() >= RazorscaleBossHelper::FUSEARMOR_THRESHOLD)
            {
                // There is another main tank with high Fuse Armor
                return true;
            }
        }
    }

    return false;
}

bool RazorscaleFuseArmorAction::Execute(Event /*event*/)
{
    // We already know from isUseful() that:
    //  1) This bot can tank, AND
    //  2) There is at least one main tank (possibly this bot) with Fuse Armor >= threshold.

    RazorscaleBossHelper bossHelper(botAI);

    // Attempt to reassign the roles based on health/Fuse Armor debuff
    bossHelper.AssignRolesBasedOnHealth();
    return true;
}

bool RazorscaleKillTargetAction::isUseful()
{
    RazorscaleKillTargetTrigger razorscaleKillTargetTrigger(botAI);
    return razorscaleKillTargetTrigger.IsActive();
}

bool RazorscaleKillTargetAction::Execute(Event /*event*/)
{
    Unit* target = GetRazorscaleKillTarget(botAI);
    if (!target)
        return false;

    MarkTargetWithSkull(bot, target);
    SetRtiTarget(botAI, "skull", target);
    return true;
}

bool RazorscalePetControlAction::isUseful()
{
    RazorscalePetControlTrigger razorscalePetControlTrigger(botAI);
    return razorscalePetControlTrigger.IsActive();
}

bool RazorscalePetControlAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "razorscale");
    if (!boss || !boss->IsAlive())
        return false;

    if (boss->GetPositionZ() >= RazorscaleBossHelper::RAZORSCALE_FLYING_Z_THRESHOLD)
    {
        // She takes no damage up there, so the pet's whole contribution is the adds.
        if (Unit* add = GetRazorscaleAddKillTarget(botAI))
            CommandPetAttack(botAI, add);
        else
            StopPet(botAI);
    }
    else
    {
        CommandPetAttack(botAI, boss);
    }

    // Deliberately reports failure: the order is a side effect, and the tick still belongs to the
    // dodge and positioning nodes below.
    return false;
}

bool RazorscaleFlameBreathAction::isUseful()
{
    RazorscaleFlameBreathTrigger razorscaleFlameBreathTrigger(botAI);
    return razorscaleFlameBreathTrigger.IsActive();
}

bool RazorscaleFlameBreathAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "razorscale");
    if (!boss || !boss->IsAlive())
        return false;

    // Sidestep the shortest way out of the frontal cone while keeping current range
    Position const dest = GetPositionOutsideFrontalCone(bot, boss, M_PI / 2.0f);
    return MoveTo(boss->GetMapId(), dest.GetPositionX(), dest.GetPositionY(), dest.GetPositionZ(), false, false, false,
                  true, MovementPriority::MOVEMENT_COMBAT);
}
