/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCActions_FrostGiant.h"
#include "Playerbots.h"

// Rotting Giant WiP (waiting for core fix)
bool IccRottingFrostGiantTankPositionAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "rotting frost giant");
    if (!boss)
        return false;

    Aura* aura = botAI->GetAura("death plague", bot, false, false);
    if (aura && IccRaidCheat(botAI, "rfg.deathplague"))
        bot->RemoveAura(aura->GetId());

    /* TODO: code works for handling plague, but atm script is bugged and one bot can have 2 plagues at the same time or
       when cured, which should not happen, and it is immpossible to handle plague atm the legit way. const bool hasCure
       = botAI->GetAura("recently infected", bot) != nullptr;

        // Tank behavior - unchanged
        if (PlayerbotAI::IsTank(bot) && botAI->HasAggro(boss) && !isInfected)
            if (bot->GetExactDist2d(ICC_ROTTING_FROST_GIANT_TANK_POSITION) > 5.0f)
                return MoveTo(bot->GetMapId(), ICC_ROTTING_FROST_GIANT_TANK_POSITION.GetPositionX(),
                              ICC_ROTTING_FROST_GIANT_TANK_POSITION.GetPositionY(),
                              ICC_ROTTING_FROST_GIANT_TANK_POSITION.GetPositionZ(), false, false, false, true,
                              MovementPriority::MOVEMENT_NORMAL);

        if (PlayerbotAI::IsTank(bot))
            return false;

        // Handle infected bot behavior - move near a non-infected, non-cured bot
        if (isInfected)
        {
            const GuidVector members = AI_VALUE(GuidVector, "group members");

            // Count how many bots are targeting each potential target
            std::map<ObjectGuid, int> targetCounts;

            // First, identify all infected bots and their current targets (approximate)
            for (auto const& memberGuid : members)
            {
                Unit* member = botAI->GetUnit(memberGuid);
                if (!member || !member->IsAlive() || member == bot)
                    continue;

                const bool memberIsInfected = botAI->GetAura("death plague", member) != nullptr;

                if (memberIsInfected)
                {
                    // Find the nearest non-infected bot to this infected bot (as a guess of its target)
                    float minDist = 5.0f;  // Only count if they're close enough to likely be targeting
                    Unit* likelyTarget = nullptr;

                    for (auto const& targetGuid : members)
                    {
                        Unit* potentialTarget = botAI->GetUnit(targetGuid);
                        if (!potentialTarget || !potentialTarget->IsAlive() || potentialTarget == member)
                            continue;

                        const bool targetIsInfected = botAI->GetAura("death plague", potentialTarget) != nullptr;
                        const bool targetHasCure = botAI->GetAura("recently infected", potentialTarget) != nullptr;

                        if (!targetIsInfected && !targetHasCure)
                        {
                            float dist = member->GetExactDist2d(potentialTarget);
                            if (dist < minDist)
                            {
                                minDist = dist;
                                likelyTarget = potentialTarget;
                            }
                        }
                    }

                    if (likelyTarget)
                    {
                        targetCounts[likelyTarget->GetGUID()]++;
                    }
                }
            }

            // Find viable targets and score them based on various factors
            std::vector<std::pair<Unit*, float>> viableTargets;

            // First try to find ranged, non-infected, non-cured bots
            for (auto const& memberGuid : members)
            {
                Unit* member = botAI->GetUnit(memberGuid);
                if (!member || !member->IsAlive() || member == bot)
                    continue;

                const bool memberHasCure = botAI->GetAura("recently infected", member) != nullptr;
                const bool memberIsInfected = botAI->GetAura("death plague", member) != nullptr;

                if (!memberIsInfected && !memberHasCure)
                {
                    // Base score is distance (lower is better)
                    float score = bot->GetExactDist2d(member);

                    // Prefer ranged targets
                    if (PlayerbotAI::IsRanged(bot))
                    {
                        score *= 0.7f;  // Bonus for ranged targets
                    }

                    // Apply penalty based on how many other infected bots are targeting this one
                    int targetingCount = targetCounts[member->GetGUID()];
                    score *= (1.0f + targetingCount * 0.5f);  // Increase score (worse) for heavily targeted bots

                    viableTargets.push_back(std::make_pair(member, score));
                }
            }

            // Sort targets by score (lowest/best first)
            std::sort(viableTargets.begin(), viableTargets.end(),
                      [](const std::pair<Unit*, float>& a, const std::pair<Unit*, float>& b)
                      { return a.second < b.second; });

            // Choose the best target
            Unit* targetBot = nullptr;
            if (!viableTargets.empty())
            {
                targetBot = viableTargets[0].first;
            }

            // Move to target bot if found
            if (targetBot)
            {
                // If we're already close enough (1 yard), no need to move
                if (bot->GetExactDist2d(targetBot) > 1.0f)
                {
                    // Calculate a unique angle based on bot's GUID to ensure different approach angles
                    // This helps spread infected bots around the target
                    uint32 guidLow = bot->GetGUID().GetCounter();
                    float angleOffset = float(guidLow % 628) / 100.0f;  // Random angle between 0 and 2π

                    // Calculate position 1 yard away from target at our unique angle
                    float angle = targetBot->GetOrientation() + angleOffset;
                    float targetX = targetBot->GetPositionX() + cos(angle) * 1.0f;
                    float targetY = targetBot->GetPositionY() + sin(angle) * 1.0f;
                    float targetZ = targetBot->GetPositionZ();

                    return MoveTo(bot->GetMapId(), targetX, targetY, targetZ, false, false, false, true,
                                  MovementPriority::MOVEMENT_COMBAT);
                }
                return true;  // Already in position
            }
            // No suitable target found, continue with normal behavior
        }

        // For ranged bots, only spread from non-infected bots
        if (PlayerbotAI::IsRanged(bot))
        {
            const float safeSpacingRadius = 11.0f;
            const float moveIncrement = 2.0f;
            const float maxMoveDistance = 15.0f;

            const GuidVector members = AI_VALUE(GuidVector, "group members");

            // Calculate a combined vector representing all nearby NON-INFECTED members' positions
            float totalX = 0.0f;
            float totalY = 0.0f;
            int nearbyCount = 0;

            for (auto const& memberGuid : members)
            {
                Unit* member = botAI->GetUnit(memberGuid);
                if (!member || !member->IsAlive() || member == bot)
                    continue;

                // Only spread from non-infected bots (can stay near infected or cured bots)
                const bool memberIsInfected = botAI->GetAura("death plague", member) != nullptr;
                if (memberIsInfected)
                    continue;

                const float distance = bot->GetExactDist2d(member);
                if (distance < safeSpacingRadius)
                {
                    // Calculate vector from member to bot
                    float dx = bot->GetPositionX() - member->GetPositionX();
                    float dy = bot->GetPositionY() - member->GetPositionY();

                    // Weight by inverse distance (closer members have more influence)
                    float weight = (safeSpacingRadius - distance) / safeSpacingRadius;
                    totalX += dx * weight;
                    totalY += dy * weight;
                    nearbyCount++;
                }
            }

            // If we have nearby non-infected members, move away in the combined direction
            if (nearbyCount > 0)
            {
                // Normalize the combined vector
                float magnitude = std::sqrt(totalX * totalX + totalY * totalY);
                if (magnitude > 0.001f)  // Avoid division by zero
                {
                    totalX /= magnitude;
                    totalY /= magnitude;

                    // Calculate move distance based on nearest member
                    float moveDistance = std::min(moveIncrement, maxMoveDistance);

                    // Create target position in the combined direction
                    float targetX = bot->GetPositionX() + totalX * moveDistance;
                    float targetY = bot->GetPositionY() + totalY * moveDistance;
                    float targetZ = bot->GetPositionZ();  // Maintain current Z

                    // Check if the target position is valid and move there
                    if (bot->IsWithinLOS(targetX, targetY, targetZ))
                    {
                        return MoveTo(bot->GetMapId(), targetX, targetY, targetZ, false, false, false, true,
                                      MovementPriority::MOVEMENT_NORMAL);
                    }
                    else
                    {
                        // If los check fails, try shorter distance
                        targetX = bot->GetPositionX() + totalX * (moveDistance * 0.5f);
                        targetY = bot->GetPositionY() + totalY * (moveDistance * 0.5f);
                        return MoveTo(bot->GetMapId(), targetX, targetY, targetZ, false, false, false, true,
                                      MovementPriority::MOVEMENT_NORMAL);
                    }
                }
            }
        }
    */
    return false;
}
