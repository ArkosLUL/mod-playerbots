#include "UldActions_Kologarn.h"
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

const Position ULDUAR_KOLOGARN_RESTORE_POSITION = Position(1764.3749f, -24.02903f, 448.0f, 0.00087690353f);
const Position ULDUAR_KOLOGARN_EYEBEAM_LEFT_POSITION = Position(1781.2051f, 9.34402f, 449.0f, 0.00087690353f);
const Position ULDUAR_KOLOGARN_EYEBEAM_RIGHT_POSITION = Position(1763.2561f, -24.44305f, 449.0f, 0.00087690353f);

bool KologarnMarkDpsTargetAction::isUseful()
{
    KologarnMarkDpsTargetTrigger kologarnMarkDpsTargetTrigger(botAI);
    return kologarnMarkDpsTargetTrigger.IsActive();
}

bool KologarnMarkDpsTargetAction::Execute(Event /*event*/)
{
    Unit* targetToMark = nullptr;
    Unit* additionalTargetToMark = nullptr;
    Unit* targetToCcMark = nullptr;
    int8 skullIndex = 7;  // Skull
    int8 crossIndex = 6;  // Cross
    int8 moonIndex = 4;   // Moon

    Unit* boss = AI_VALUE2(Unit*, "find target", "kologarn");
    if (!boss || !boss->IsAlive())
        return false;

    // Check that there is rubble to mark
    GuidVector targets = AI_VALUE(GuidVector, "possible targets");
    Unit* target = nullptr;
    for (auto i = targets.begin(); i != targets.end(); ++i)
    {
        target = botAI->GetUnit(*i);
        if (!target)
            continue;

        if (target->GetEntry() == NPC_RUBBLE && target->IsAlive())
        {
            targetToMark = target;
            additionalTargetToMark = boss;
        }
    }

    if (!targetToMark)
    {
        Unit* rightArm = AI_VALUE2(Unit*, "find target", "right arm");
        if (rightArm && rightArm->IsAlive())
        {
            targetToMark = rightArm;
            additionalTargetToMark = boss;
        }
    }

    if (!targetToMark)
    {
        Unit* boss = AI_VALUE2(Unit*, "find target", "kologarn");
        if (boss && boss->IsAlive())
            targetToMark = boss;
    }

    if (!targetToMark)
        return false;  // No target to mark

    Unit* leftArm = AI_VALUE2(Unit*, "find target", "left arm");
    if (leftArm && leftArm->IsAlive())
        targetToCcMark = leftArm;

    bool isMainTank = botAI->IsMainTank(bot);
    Unit* mainTankUnit = AI_VALUE(Unit*, "main tank");
    Player* mainTank = mainTankUnit ? mainTankUnit->ToPlayer() : nullptr;

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
                    group->SetTargetIcon(skullIndex, bot->GetGUID(), targetToMark->GetGUID());
                    if (targetToCcMark)
                        group->SetTargetIcon(moonIndex, bot->GetGUID(), targetToCcMark->GetGUID());

                    if (additionalTargetToMark)
                        group->SetTargetIcon(crossIndex, bot->GetGUID(), additionalTargetToMark->GetGUID());

                    return true;
                }
                break;  // Stop after finding the first valid bot tank
            }
        }
    }
    else if (isMainTank && bot->IsAlive())  // Bot is the main tank
    {
        Group* group = bot->GetGroup();
        if (group)
        {
            group->SetTargetIcon(skullIndex, bot->GetGUID(), targetToMark->GetGUID());
            if (targetToCcMark)
                group->SetTargetIcon(moonIndex, bot->GetGUID(), targetToCcMark->GetGUID());

            if (additionalTargetToMark)
                group->SetTargetIcon(crossIndex, bot->GetGUID(), additionalTargetToMark->GetGUID());

            return true;
        }
    }
    else
    {
        for (int i = 0; i < 3; ++i)
        {
            if (botAI->IsAssistTankOfIndex(bot, i) && GET_PLAYERBOT_AI(bot) && bot->IsAlive())  // Bot is a valid tank
            {
                Group* group = bot->GetGroup();
                if (group)
                {
                    group->SetTargetIcon(skullIndex, bot->GetGUID(), targetToMark->GetGUID());
                    if (targetToCcMark)
                        group->SetTargetIcon(moonIndex, bot->GetGUID(), targetToCcMark->GetGUID());

                    if (additionalTargetToMark)
                        group->SetTargetIcon(crossIndex, bot->GetGUID(), additionalTargetToMark->GetGUID());

                    return true;
                }
                break;  // Stop after finding the first valid bot tank
            }
        }
    }

    return false;
}

bool KologarnFallFromFloorAction::Execute(Event /*event*/)
{
    return bot->TeleportTo(bot->GetMapId(), ULDUAR_KOLOGARN_RESTORE_POSITION.GetPositionX(),
                           ULDUAR_KOLOGARN_RESTORE_POSITION.GetPositionY(),
                           ULDUAR_KOLOGARN_RESTORE_POSITION.GetPositionZ(),
                           ULDUAR_KOLOGARN_RESTORE_POSITION.GetOrientation());
}

bool KologarnFallFromFloorAction::isUseful()
{
    KologarnFallFromFloorTrigger kologarnFallFromFloorTrigger(botAI);
    return kologarnFallFromFloorTrigger.IsActive();
}

bool KologarnRubbleSlowdownAction::Execute(Event /*event*/)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    int8 skullIndex = 7;
    ObjectGuid currentSkullTarget = group->GetTargetIcon(skullIndex);
    Unit* currentSkullUnit = botAI->GetUnit(currentSkullTarget);
    if (!currentSkullUnit || !currentSkullUnit->IsAlive() || currentSkullUnit->GetEntry() != NPC_RUBBLE)
        return false;

    return botAI->CastSpell("frost trap", currentSkullUnit);
}

bool KologarnEyebeamAction::Execute(Event /*event*/)
{
    float distanceToLeftPoint = bot->GetExactDist(ULDUAR_KOLOGARN_EYEBEAM_LEFT_POSITION);
    float distanceToRightPoint = bot->GetExactDist(ULDUAR_KOLOGARN_EYEBEAM_RIGHT_POSITION);

    bool runToLeftSide;
    if (!distanceToLeftPoint)
        runToLeftSide = true;
    else if (!distanceToRightPoint)
        runToLeftSide = false;
    else
        runToLeftSide = distanceToRightPoint > distanceToLeftPoint;

    bool teleportedToPoint;
    KologarnEyebeamTrigger kologarnEyebeamTrigger(botAI);
    if (runToLeftSide)
    {
        teleportedToPoint = bot->TeleportTo(bot->GetMapId(), ULDUAR_KOLOGARN_EYEBEAM_LEFT_POSITION.GetPositionX(),
                                            ULDUAR_KOLOGARN_EYEBEAM_LEFT_POSITION.GetPositionY(),
                                            ULDUAR_KOLOGARN_EYEBEAM_LEFT_POSITION.GetPositionZ(),
                                            ULDUAR_KOLOGARN_EYEBEAM_LEFT_POSITION.GetOrientation());
    }
    else
    {
        teleportedToPoint = bot->TeleportTo(bot->GetMapId(), ULDUAR_KOLOGARN_EYEBEAM_RIGHT_POSITION.GetPositionX(),
                                            ULDUAR_KOLOGARN_EYEBEAM_RIGHT_POSITION.GetPositionY(),
                                            ULDUAR_KOLOGARN_EYEBEAM_RIGHT_POSITION.GetPositionZ(),
                                            ULDUAR_KOLOGARN_EYEBEAM_RIGHT_POSITION.GetOrientation());
    }

    if (teleportedToPoint)
        SetNextMovementDelay(5000);

    return teleportedToPoint;
}

bool KologarnEyebeamAction::isUseful()
{
    KologarnEyebeamTrigger kologarnEyebeamTrigger(botAI);
    if (!kologarnEyebeamTrigger.IsActive())
        return false;

    return botAI->HasCheat(BotCheatMask::raid);
}

bool KologarnRtiTargetAction::isUseful()
{
    KologarnRtiTargetTrigger kologarnRtiTargetTrigger(botAI);
    return kologarnRtiTargetTrigger.IsActive();
}

bool KologarnRtiTargetAction::Execute(Event /*event*/)
{
    if (botAI->IsMainTank(bot) || botAI->IsAssistTankOfIndex(bot, 0))
    {
        context->GetValue<std::string>("rti")->Set("cross");
        return true;
    }

    context->GetValue<std::string>("rti")->Set("skull");
    return true;
}

bool KologarnCrunchArmorAction::isUseful()
{
    KologarnCrunchArmorTrigger kologarnCrunchArmorTrigger(botAI);
    if (!kologarnCrunchArmorTrigger.IsActive())
        return false;

    return botAI->HasCheat(BotCheatMask::raid);
}

bool KologarnCrunchArmorAction::Execute(Event /*event*/)
{
    bot->RemoveAura(SPELL_CRUNCH_ARMOR);
    return true;
}
