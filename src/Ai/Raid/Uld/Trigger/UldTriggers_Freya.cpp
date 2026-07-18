#include "UldTriggers_Freya.h"

#include "GameObject.h"
#include "Object.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldBossHelper.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "RaidBossHelpers.h"
#include "ScriptedCreature.h"
#include "SharedDefines.h"
#include "Trigger.h"
#include "Vehicle.h"
#include <MovementActions.h>
#include <FollowMasterStrategy.h>
#include <RtiTargetValue.h>

bool FreyaNearNatureBombTrigger::IsActive()
{
    // Check boss and it is alive
    Unit* boss = AI_VALUE2(Unit*, "find target", "freya");
    if (!boss || !boss->IsAlive())
    {
        return false;
    }

    // Find the nearest Nature Bomb
    GameObject* target = bot->FindNearestGameObject(GOBJECT_NATURE_BOMB, 12.0f);
    return target != nullptr;
}

bool FreyaMarkDpsTargetTrigger::IsActive()
{
    // Check boss and it is alive
    Unit* boss = AI_VALUE2(Unit*, "find target", "freya");
    if (!boss || !boss->IsAlive())
        return false;

    // Only tank bot can mark target
    if (!botAI->IsTank(bot))
        return false;

    // Get current raid dps target
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    int8 skullIndex = 7;
    ObjectGuid currentSkullTarget = group->GetTargetIcon(skullIndex);
    Unit* currentSkullUnit = botAI->GetUnit(currentSkullTarget);

    if (currentSkullUnit && !currentSkullUnit->IsAlive())
    {
        currentSkullUnit = nullptr;
    }

    // Check which adds is up
    Unit* eonarsGift = nullptr;
    Unit* ancientConservator = nullptr;
    Unit* snaplasher = nullptr;
    Unit* ancientWaterSpirit = nullptr;
    Unit* stormLasher = nullptr;
    Unit* firstDetonatingLasher = nullptr;

    GuidVector targets = AI_VALUE(GuidVector, "possible targets");
    Unit* target = nullptr;
    for (auto i = targets.begin(); i != targets.end(); ++i)
    {
        target = botAI->GetUnit(*i);
        if (!target || !target->IsAlive())
            continue;

        if (target->GetEntry() == NPC_EONARS_GIFT)
        {
            eonarsGift = target;
        }
        else if (target->GetEntry() == NPC_ANCIENT_CONSERVATOR)
        {
            ancientConservator = target;
        }
        else if (target->GetEntry() == NPC_SNAPLASHER)
        {
            snaplasher = target;
        }
        else if (target->GetEntry() == NPC_ANCIENT_WATER_SPIRIT)
        {
            ancientWaterSpirit = target;
        }
        else if (target->GetEntry() == NPC_STORM_LASHER)
        {
            stormLasher = target;
        }
        else if (target->GetEntry() == NPC_DETONATING_LASHER && !firstDetonatingLasher)
        {
            firstDetonatingLasher = target;
        }
    }

    // Check that eonars gift is need to be mark
    if (eonarsGift && (!currentSkullUnit || currentSkullUnit->GetEntry() != eonarsGift->GetEntry()))
    {
        return true;
    }

    // Check that ancient conservator is need to be mark
    if (ancientConservator && (!currentSkullUnit || currentSkullUnit->GetEntry() != ancientConservator->GetEntry()))
    {
        return true;
    }

    // Check that trio of adds is need to be mark
    if (snaplasher || ancientWaterSpirit || stormLasher)
    {
        Unit* highestHealthUnit = nullptr;
        uint32 highestHealth = 0;

        if (snaplasher && snaplasher->GetHealth() > highestHealth)
        {
            highestHealth = snaplasher->GetHealth();
            highestHealthUnit = snaplasher;
        }
        if (ancientWaterSpirit && ancientWaterSpirit->GetHealth() > highestHealth)
        {
            highestHealth = ancientWaterSpirit->GetHealth();
            highestHealthUnit = ancientWaterSpirit;
        }
        if (stormLasher && stormLasher->GetHealth() > highestHealth)
        {
            highestHealthUnit = stormLasher;
        }

        // If the highest health unit is not already marked, mark it
        if (highestHealthUnit && (!currentSkullUnit || currentSkullUnit->GetEntry() != highestHealthUnit->GetEntry()))
        {
            return true;
        }
    }

    // Check that detonating lasher is need to be mark
    if (firstDetonatingLasher &&
        (!currentSkullUnit || currentSkullUnit->GetEntry() != firstDetonatingLasher->GetEntry()))
    {
        Map* map = bot->GetMap();
        if (!map || !map->IsRaid())
            return false;

        uint32 healthThreshold = map->Is25ManRaid() ? 7200 : 4900;  // Detonate maximum damage

        // Check that detonate lasher dont kill raid members
        for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
        {
            Player* member = gref->GetSource();
            if (!member || !member->IsAlive())
                continue;

            if (member->GetHealth() < healthThreshold)
                return false;
        }

        return true;
    }

    return false;
}

bool FreyaMoveToHealingSporeTrigger::IsActive()
{
    // Check for the Freya boss
    Unit* boss = AI_VALUE2(Unit*, "find target", "freya");
    if (!boss || !boss->IsAlive())
        return false;

    if (!botAI->IsRanged(bot))
        return false;

    Unit* conservatory = AI_VALUE2(Unit*, "find target", "ancient conservator");
    if (!conservatory || !conservatory->IsAlive())
        return false;

    GuidVector targets = AI_VALUE(GuidVector, "nearest npcs");
    float nearestDistance = std::numeric_limits<float>::max();
    bool foundSpore = false;

    // Iterate through all targets to find healthy spores
    for (const ObjectGuid& guid : targets)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        // Check if the unit is a healthy spore
        if (unit->GetEntry() == NPC_HEALTHY_SPORE)
        {
            foundSpore = true;
            float distance = bot->GetDistance(unit);
            if (distance < nearestDistance)
            {
                nearestDistance = distance;
            }
        }
    }

    // If no healthy spores are found, return false
    if (!foundSpore)
        return false;

    // If the nearest spore is farther than 6 yards, a move is required
    return nearestDistance > 6.0f;
}

bool FreyaBreakIronRootsTrigger::IsActive()
{
    if (!IsFreyaHardModeActive(botAI))
        return false;

    // Trapped by either the Ironbranch or the Freya-cast Iron Roots (each leaves its own DoT).
    return bot->HasAura(SPELL_IRON_ROOTS_DAMAGE) || bot->HasAura(SPELL_IRON_ROOTS_FREYA_DAMAGE);
}

bool FreyaDodgeUnstableSunBeamTrigger::IsActive()
{
    if (!IsFreyaHardModeActive(botAI))
        return false;

    // The beam stalkers are non-selectable, so they never show up in attack-target lists - scan the
    // raw nearby-npc list instead.
    GuidVector npcs = AI_VALUE(GuidVector, "nearest npcs");
    for (auto const& guid : npcs)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        if (unit->GetEntry() != NPC_FREYA_SUN_BEAM && unit->GetEntry() != NPC_FREYA_UNSTABLE_SUN_BEAM)
            continue;

        if (bot->GetExactDist2d(unit) < ULDUAR_FREYA_UNSTABLE_SUN_BEAM_RADIUS)
            return true;
    }

    return false;
}
