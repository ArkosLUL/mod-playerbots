#include "UldActions_Thorim.h"
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

const Position ULDUAR_THORIM_JUMP_START_POINT = Position(2137.137f, -291.19025f, 438.24753f, 1.7059844f);

bool ThorimUnbalancingStrikeAction::isUseful()
{
    ThorimUnbalancingStrikeTrigger thorimUnbalancingStrikeTrigger(botAI);
    if (!thorimUnbalancingStrikeTrigger.IsActive())
        return false;

    return botAI->HasCheat(BotCheatMask::raid);
}

bool ThorimUnbalancingStrikeAction::Execute(Event /*event*/)
{
    bot->RemoveAura(SPELL_UNBALANCING_STRIKE);
    return true;
}

bool ThorimMarkDpsTargetAction::isUseful()
{
    ThorimMarkDpsTargetTrigger thorimMarkDpsTargetTrigger(botAI);
    return thorimMarkDpsTargetTrigger.IsActive();
}

bool ThorimMarkDpsTargetAction::Execute(Event /*event*/)
{
    Unit* targetToMark = nullptr;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    ObjectGuid currentMoonTarget = group->GetTargetIcon(RtiTargetValue::moonIndex);
    Unit* currentMoonUnit = botAI->GetUnit(currentMoonTarget);
    Unit* boss = AI_VALUE2(Unit*, "find target", "thorim");
    if (!currentMoonUnit && boss && boss->IsAlive() && boss->GetPositionZ() > ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
    {
        group->SetTargetIcon(RtiTargetValue::moonIndex, bot->GetGUID(), boss->GetGUID());
    }

    if (currentMoonUnit && boss && currentMoonUnit->GetEntry() == boss->GetEntry() &&
        boss->GetPositionZ() < ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
    {
        group->SetTargetIcon(RtiTargetValue::skullIndex, bot->GetGUID(), boss->GetGUID());
        return true;
    }

    if (botAI->IsMainTank(bot))
    {
        ObjectGuid currentSkullTarget = group->GetTargetIcon(RtiTargetValue::skullIndex);
        Unit* currentSkullUnit = botAI->GetUnit(currentSkullTarget);
        if (currentSkullUnit && !currentSkullUnit->IsAlive())
        {
            currentSkullUnit = nullptr;
        }

        Unit* acolyte = AI_VALUE2(Unit*, "find target", "dark rune acolyte");
        Unit* evoker = AI_VALUE2(Unit*, "find target", "dark rune evoker");

        if (acolyte && acolyte->IsAlive() && bot->GetDistance(acolyte) < 50.0f &&
            (!currentSkullUnit || currentSkullUnit->GetEntry() != acolyte->GetEntry()))
            targetToMark = acolyte;
        else if (evoker && evoker->IsAlive() && bot->GetDistance(evoker) < 50.0f &&
                 (!currentSkullUnit || currentSkullUnit->GetEntry() != evoker->GetEntry()))
            targetToMark = evoker;
        else
            return false;
    }
    else if (botAI->IsAssistTankOfIndex(bot, 0))
    {
        ObjectGuid currentCrossTarget = group->GetTargetIcon(RtiTargetValue::crossIndex);
        Unit* currentCrossUnit = botAI->GetUnit(currentCrossTarget);
        if (currentCrossUnit && !currentCrossUnit->IsAlive())
        {
            currentCrossUnit = nullptr;
        }

        Unit* acolyte = AI_VALUE2(Unit*, "find target", "dark rune acolyte");
        Unit* runicColossus = AI_VALUE2(Unit*, "find target", "runic colossus");
        Unit* ancientRuneGiant = AI_VALUE2(Unit*, "find target", "ancient rune giant");
        Unit* ironHonorGuard = AI_VALUE2(Unit*, "find target", "iron ring guard");
        Unit* ironRingGuard = AI_VALUE2(Unit*, "find target", "iron honor guard");

        if (acolyte && acolyte->IsAlive() && (!currentCrossUnit || currentCrossUnit->GetEntry() != acolyte->GetEntry()))
            targetToMark = acolyte;
        else if (runicColossus && runicColossus->IsAlive() &&
                 (!currentCrossUnit || currentCrossUnit->GetEntry() != runicColossus->GetEntry()))
            targetToMark = runicColossus;
        else if (ancientRuneGiant && ancientRuneGiant->IsAlive() &&
                 (!currentCrossUnit || currentCrossUnit->GetEntry() != ancientRuneGiant->GetEntry()))
            targetToMark = ancientRuneGiant;
        else if (ironHonorGuard && ironHonorGuard->IsAlive() &&
                 (!currentCrossUnit || currentCrossUnit->GetEntry() != ironHonorGuard->GetEntry()))
            targetToMark = ironHonorGuard;
        else if (ironRingGuard && ironRingGuard->IsAlive() &&
                 (!currentCrossUnit || currentCrossUnit->GetEntry() != ironRingGuard->GetEntry()))
            targetToMark = ironRingGuard;
        else
            return false;
    }

    if (!targetToMark)
        return false;  // No target to mark

    if (botAI->IsMainTank(bot))
    {
        group->SetTargetIcon(RtiTargetValue::skullIndex, bot->GetGUID(), targetToMark->GetGUID());
        return true;
    }

    if (botAI->IsAssistTankOfIndex(bot, 0))
    {
        group->SetTargetIcon(RtiTargetValue::crossIndex, bot->GetGUID(), targetToMark->GetGUID());
        return true;
    }

    return false;
}

bool ThorimArenaPositioningAction::isUseful()
{
    ThorimArenaPositioningTrigger thorimArenaPositioningTrigger(botAI);
    return thorimArenaPositioningTrigger.IsActive();
}

bool ThorimArenaPositioningAction::Execute(Event /*event*/)
{
    FollowMasterStrategy followMasterStrategy(botAI);

    MoveTo(bot->GetMapId(), ULDUAR_THORIM_NEAR_ARENA_CENTER.GetPositionX(),
           ULDUAR_THORIM_NEAR_ARENA_CENTER.GetPositionY(), ULDUAR_THORIM_NEAR_ARENA_CENTER.GetPositionZ(), false, false,
           false, true, MovementPriority::MOVEMENT_COMBAT, true);

    if (botAI->HasStrategy(followMasterStrategy.getName(), BotState::BOT_STATE_NON_COMBAT))
    {
        botAI->ChangeStrategy(REMOVE_STRATEGY_CHAR + followMasterStrategy.getName(), BotState::BOT_STATE_NON_COMBAT);
    }

    return true;
}

bool ThorimGauntletPositioningAction::isUseful()
{
    ThorimGauntletPositioningTrigger thorimGauntletPositioningTrigger(botAI);
    return thorimGauntletPositioningTrigger.IsActive();
}

bool ThorimGauntletPositioningAction::Execute(Event /*event*/)
{
    FollowMasterStrategy followMasterStrategy(botAI);

    Unit* master = botAI->GetMaster();

    std::string const rti = AI_VALUE(std::string, "rti");
    if (rti != "cross")
    {
        botAI->GetAiObjectContext()->GetValue<std::string>("rti")->Set("cross");
    }

    if (master->GetDistance(ULDUAR_THORIM_NEAR_ENTRANCE_POSITION) < 10.0f && (bot->GetDistance2d(master) > 5.0f))
    {
        if (MoveTo(bot->GetMapId(), master->GetPositionX(), master->GetPositionY(), master->GetPositionZ(), false,
                   false, false, true, MovementPriority::MOVEMENT_NORMAL, true))
        {
            if (!botAI->HasStrategy(followMasterStrategy.getName(), BotState::BOT_STATE_NON_COMBAT))
            {
                botAI->ChangeStrategy(ADD_STRATEGY_CHAR + followMasterStrategy.getName(),
                                      BotState::BOT_STATE_NON_COMBAT);
            }

            return true;
        }
    }

    if (master->GetDistance(ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_6_YARDS_1) < 6.0f ||
        master->GetDistance(ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_6_YARDS_2) < 6.0f ||
        master->GetDistance(ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_5_YARDS_1) < 5.0f ||
        master->GetDistance(ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_1) < 10.0f ||
        master->GetDistance(ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_2) < 10.0f ||
        master->GetDistance(ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_3) < 10.0f)
    {
        float distance1 = master->GetDistance(ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_6_YARDS_1);
        float distance2 = master->GetDistance(ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_6_YARDS_2);
        float distance3 = master->GetDistance(ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_5_YARDS_1);
        float distance4 = master->GetDistance(ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_1);
        float distance5 = master->GetDistance(ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_2);
        float distance6 = master->GetDistance(ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_3);

        float smallestDistance = std::min({distance1, distance2, distance3, distance4, distance5, distance6});

        Position targetPosition;

        if (smallestDistance == distance1)
        {
            return MoveTo(bot->GetMapId(), ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_6_YARDS_1.GetPositionX(),
                          ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_6_YARDS_1.GetPositionY(),
                          ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_6_YARDS_1.GetPositionZ(), false, false, false, true,
                          MovementPriority::MOVEMENT_NORMAL, true);
        }
        else if (smallestDistance == distance2)
        {
            return MoveTo(bot->GetMapId(), ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_6_YARDS_2.GetPositionX(),
                          ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_6_YARDS_2.GetPositionY(),
                          ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_6_YARDS_2.GetPositionZ(), false, false, false, true,
                          MovementPriority::MOVEMENT_COMBAT);
        }
        else if (smallestDistance == distance3)
        {
            return MoveTo(bot->GetMapId(), ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_5_YARDS_1.GetPositionX(),
                          ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_5_YARDS_1.GetPositionY(),
                          ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_5_YARDS_1.GetPositionZ(), false, false, false, true,
                          MovementPriority::MOVEMENT_NORMAL, true);
        }
        else if (smallestDistance == distance4)
        {
            return MoveTo(bot->GetMapId(), ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_1.GetPositionX(),
                          ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_1.GetPositionY(),
                          ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_1.GetPositionZ(), false, false, false, true,
                          MovementPriority::MOVEMENT_NORMAL, true);
        }
        else if (smallestDistance == distance5)
        {
            return MoveTo(bot->GetMapId(), ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_2.GetPositionX(),
                          ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_2.GetPositionY(),
                          ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_2.GetPositionZ(), false, false, false, true,
                          MovementPriority::MOVEMENT_NORMAL, true);
        }
        else if (smallestDistance == distance6)
        {
            return MoveTo(bot->GetMapId(), ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_3.GetPositionX(),
                          ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_3.GetPositionY(),
                          ULDUAR_THORIM_GAUNTLET_LEFT_SIDE_10_YARDS_3.GetPositionZ(), false, false, false, true,
                          MovementPriority::MOVEMENT_NORMAL, true);
        }
        else
            return false;
    }

    if (master->GetDistance(ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_6_YARDS_1) < 6.0f ||
        master->GetDistance(ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_6_YARDS_2) < 6.0f ||
        master->GetDistance(ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_5_YARDS_1) < 5.0f ||
        master->GetDistance(ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_1) < 10.0f ||
        master->GetDistance(ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_2) < 10.0f ||
        master->GetDistance(ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_3) < 10.0f)
    {
        float distance1 = master->GetDistance(ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_6_YARDS_1);
        float distance2 = master->GetDistance(ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_6_YARDS_2);
        float distance3 = master->GetDistance(ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_5_YARDS_1);
        float distance4 = master->GetDistance(ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_1);
        float distance5 = master->GetDistance(ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_2);
        float distance6 = master->GetDistance(ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_3);

        float smallestDistance = std::min({distance1, distance2, distance3, distance4, distance5, distance6});

        Position targetPosition;

        if (smallestDistance == distance1)
        {
            return MoveTo(bot->GetMapId(), ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_6_YARDS_1.GetPositionX(),
                          ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_6_YARDS_1.GetPositionY(),
                          ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_6_YARDS_1.GetPositionZ(), false, false, false, true,
                          MovementPriority::MOVEMENT_NORMAL, true);
        }
        else if (smallestDistance == distance2)
        {
            return MoveTo(bot->GetMapId(), ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_6_YARDS_2.GetPositionX(),
                          ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_6_YARDS_2.GetPositionY(),
                          ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_6_YARDS_2.GetPositionZ(), false, false, false, true,
                          MovementPriority::MOVEMENT_COMBAT);
        }
        else if (smallestDistance == distance3)
        {
            return MoveTo(bot->GetMapId(), ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_5_YARDS_1.GetPositionX(),
                          ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_5_YARDS_1.GetPositionY(),
                          ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_5_YARDS_1.GetPositionZ(), false, false, false, true,
                          MovementPriority::MOVEMENT_NORMAL, true);
        }
        else if (smallestDistance == distance4)
        {
            return MoveTo(bot->GetMapId(), ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_1.GetPositionX(),
                          ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_1.GetPositionY(),
                          ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_1.GetPositionZ(), false, false, false, true,
                          MovementPriority::MOVEMENT_NORMAL, true);
        }
        else if (smallestDistance == distance5)
        {
            return MoveTo(bot->GetMapId(), ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_2.GetPositionX(),
                          ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_2.GetPositionY(),
                          ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_2.GetPositionZ(), false, false, false, true,
                          MovementPriority::MOVEMENT_NORMAL, true);
        }
        else if (smallestDistance == distance6)
        {
            return MoveTo(bot->GetMapId(), ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_3.GetPositionX(),
                          ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_3.GetPositionY(),
                          ULDUAR_THORIM_GAUNTLET_RIGHT_SIDE_10_YARDS_3.GetPositionZ(), false, false, false, true,
                          MovementPriority::MOVEMENT_NORMAL, true);
        }
        else
            return false;
    }

    Unit* boss = AI_VALUE2(Unit*, "find target", "thorim");
    if (boss && boss->IsAlive() && bot->GetPositionZ() > ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD &&
        boss->GetPositionZ() < ULDUAR_THORIM_AXIS_Z_FLOOR_THRESHOLD)
    {
        MoveTo(bot->GetMapId(), ULDUAR_THORIM_JUMP_START_POINT.GetPositionX(),
               ULDUAR_THORIM_JUMP_START_POINT.GetPositionY(), ULDUAR_THORIM_JUMP_START_POINT.GetPositionZ(), false,
               false, false, true, MovementPriority::MOVEMENT_NORMAL, true);

        if (bot->GetDistance(ULDUAR_THORIM_JUMP_START_POINT) > 0.5f)
            return false;

        JumpTo(bot->GetMapId(), ULDUAR_THORIM_JUMP_END_POINT.GetPositionX(),
               ULDUAR_THORIM_JUMP_END_POINT.GetPositionY(), ULDUAR_THORIM_JUMP_END_POINT.GetPositionZ(),
               MovementPriority::MOVEMENT_COMBAT);
    }

    return false;
}

bool ThorimFallFromFloorAction::Execute(Event /*event*/)
{
    Player* master = botAI->GetMaster();

    if (!master)
        return false;

    return bot->TeleportTo(bot->GetMapId(), master->GetPositionX(), master->GetPositionY(), master->GetPositionZ(),
                           master->GetOrientation());
}

bool ThorimFallFromFloorAction::isUseful()
{
    ThorimFallFromFloorTrigger thorimFallFromFloorTrigger(botAI);
    return thorimFallFromFloorTrigger.IsActive();
}

bool ThorimPhase2PositioningAction::Execute(Event /*event*/)
{
    Position targetPosition;
    bool backward = false;

    if (botAI->IsMainTank(bot))
    {
        targetPosition = ULDUAR_THORIM_PHASE2_TANK_SPOT;
        backward = true;
    }
    else
    {
        Group* group = bot->GetGroup();
        if (!group)
            return false;

        uint32 memberPositionNumber = 0;
        for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
        {
            Player* member = gref->GetSource();
            if (!member)
                continue;

            if (botAI->IsRanged(member) || botAI->IsHeal(member))
            {
                if (bot->GetGUID() == member->GetGUID())
                    break;

                memberPositionNumber++;

                if (memberPositionNumber == 3)
                    memberPositionNumber = 0;
            }
        }

        if (memberPositionNumber == 0)
            targetPosition = ULDUAR_THORIM_PHASE2_RANGE1_SPOT;

        if (memberPositionNumber == 1)
            targetPosition = ULDUAR_THORIM_PHASE2_RANGE2_SPOT;

        if (memberPositionNumber == 2)
            targetPosition = ULDUAR_THORIM_PHASE2_RANGE3_SPOT;
    }

    MoveTo(bot->GetMapId(), targetPosition.GetPositionX(), targetPosition.GetPositionY(), targetPosition.GetPositionZ(),
           false, false, false, true, MovementPriority::MOVEMENT_COMBAT, true, backward);

    if (bot->GetDistance(targetPosition) > 1.0f)
        return false;

    return true;
}

bool ThorimPhase2PositioningAction::isUseful()
{
    ThorimPhase2PositioningTrigger thorimPhase2PositioningTrigger(botAI);
    return thorimPhase2PositioningTrigger.IsActive();
}

bool ThorimUnbalancingStrikeSwapAction::isUseful()
{
    ThorimUnbalancingStrikeSwapTrigger thorimUnbalancingStrikeSwapTrigger(botAI);
    return thorimUnbalancingStrikeSwapTrigger.IsActive();
}

bool ThorimUnbalancingStrikeSwapAction::Execute(Event event)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "thorim");
    if (!boss || !boss->IsAlive())
        return false;

    if (AI_VALUE(Unit*, "current target") != boss)
        return Attack(boss);

    if (boss->GetVictim() != bot)
        return botAI->DoSpecificAction("taunt spell", event, true);

    return false;
}
