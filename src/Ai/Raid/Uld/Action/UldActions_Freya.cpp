#include "UldActions_Freya.h"
#include "UldActions_Shared.h"

#include <CombatStrategy.h>
#include <FollowMasterStrategy.h>

#include <cmath>

#include "AiObjectContext.h"
#include "DBCEnums.h"
#include "GameObject.h"
#include "Group.h"
#include "LastMovementValue.h"
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

bool FreyaMoveAwayNatureBombAction::isUseful()
{
    // Check boss and it is alive
    Unit* boss = AI_VALUE2(Unit*, "find target", "freya");
    if (!boss || !boss->IsAlive())
    {
        return false;
    }

    // Find the nearest Nature Bomb
    GameObject* target = bot->FindNearestGameObject(GOBJECT_NATURE_BOMB, 12.0f);
    if (!target)
        return false;

    return true;
}

bool FreyaMoveAwayNatureBombAction::Execute(Event /*event*/)
{
    GameObject* target = bot->FindNearestGameObject(GOBJECT_NATURE_BOMB, 12.0f);
    if (!target)
        return false;

    return FleePosition(target->GetPosition(), 13.0f);
}

bool FreyaMarkDpsTargetAction::isUseful()
{
    FreyaMarkDpsTargetTrigger freyaMarkDpsTargetTrigger(botAI);
    return freyaMarkDpsTargetTrigger.IsActive();
}

bool FreyaMarkDpsTargetAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "freya");
    if (!boss || !boss->IsAlive())
        return false;

    Unit* targetToMark = nullptr;

    // Check which adds is up
    Unit* eonarsGift = nullptr;
    Unit* ancientConservator = nullptr;
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
            eonarsGift = target;
        else if (target->GetEntry() == NPC_ANCIENT_CONSERVATOR)
            ancientConservator = target;
        else if (target->GetEntry() == NPC_ANCIENT_WATER_SPIRIT)
            ancientWaterSpirit = target;
        else if (target->GetEntry() == NPC_STORM_LASHER)
            stormLasher = target;
        else if (target->GetEntry() == NPC_DETONATING_LASHER && !firstDetonatingLasher)
            firstDetonatingLasher = target;
    }

    // Check that eonars gift is need to be mark
    if (eonarsGift)
        targetToMark = eonarsGift;

    // Check that ancient conservator is need to be mark
    if (ancientConservator && !targetToMark)
        targetToMark = ancientConservator;

    // Trio wave: Storm Lasher is the burst/interrupt priority, Ancient Water Spirit next.
    // The Snaplasher hardens the more attackers strike it, so it is deliberately left
    // unmarked to avoid funnelling the whole raid onto it (which would make it invulnerable).
    if (!targetToMark)
    {
        if (stormLasher)
            targetToMark = stormLasher;
        else if (ancientWaterSpirit)
            targetToMark = ancientWaterSpirit;
    }

    // Check that detonating lasher is need to be mark
    if (firstDetonatingLasher && !targetToMark)
        targetToMark = firstDetonatingLasher;

    if (!targetToMark)
        return false;  // No target to mark

    bool isMainTank = botAI->IsMainTank(bot);
    Unit* mainTankUnit = AI_VALUE(Unit*, "main tank");
    Player* mainTank = mainTankUnit ? mainTankUnit->ToPlayer() : nullptr;
    int8 squareIndex = 5;  // Square
    int8 skullIndex = 7;   // Skull

    if (mainTank && !GET_PLAYERBOT_AI(mainTank))  // Main tank is a real player
    {
        // Iterate through the first 3 bot tanks to assign the Skull marker
        for (int i = 0; i < 3; ++i)
        {
            if (botAI->IsAssistTankOfIndex(bot, i) && GET_PLAYERBOT_AI(bot))  // Bot is a valid tank
            {
                Group* group = bot->GetGroup();
                if (group)
                {
                    ObjectGuid currentSkullTarget = group->GetTargetIcon(skullIndex);

                    if (!currentSkullTarget || (targetToMark->GetGUID() != currentSkullTarget))
                    {
                        group->SetTargetIcon(skullIndex, bot->GetGUID(), targetToMark->GetGUID());
                        group->SetTargetIcon(squareIndex, bot->GetGUID(), boss->GetGUID());
                        return true;
                    }
                }
                break;
            }
        }
    }
    else if (isMainTank)  // Bot is the main tank
    {
        Group* group = bot->GetGroup();
        if (group)
        {
            ObjectGuid currentSkullTarget = group->GetTargetIcon(skullIndex);

            if (!currentSkullTarget || (targetToMark->GetGUID() != currentSkullTarget))
            {
                group->SetTargetIcon(skullIndex, bot->GetGUID(), targetToMark->GetGUID());
                group->SetTargetIcon(squareIndex, bot->GetGUID(), boss->GetGUID());
                botAI->GetAiObjectContext()->GetValue<std::string>("rti")->Set("square");
                return true;
            }
        }
    }

    return false;
}

bool FreyaMoveToHealingSporeAction::isUseful()
{
    FreyaMoveToHealingSporeTrigger freyaMoveToHealingSporeTrigger(botAI);
    return freyaMoveToHealingSporeTrigger.IsActive();
}

bool FreyaMoveToHealingSporeAction::Execute(Event /*event*/)
{
    GuidVector targets = AI_VALUE(GuidVector, "nearest npcs");
    Creature* nearestSpore = nullptr;
    float nearestDistance = std::numeric_limits<float>::max();

    for (auto guid : targets)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit)
            continue;

        // Check if this unit is a healthy spore and alive
        if (unit->GetEntry() != NPC_HEALTHY_SPORE || !unit->IsAlive())
            continue;

        float distance = bot->GetDistance2d(unit);
        if (distance < nearestDistance)
        {
            nearestDistance = distance;
            nearestSpore = static_cast<Creature*>(unit);
        }
    }

    if (!nearestSpore)
        return false;

    return MoveTo(nearestSpore->GetMapId(), nearestSpore->GetPositionX(), nearestSpore->GetPositionY(),
                  nearestSpore->GetPositionZ(), false, false, false, true, MovementPriority::MOVEMENT_COMBAT);
}
