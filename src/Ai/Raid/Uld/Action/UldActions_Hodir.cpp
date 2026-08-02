#include "UldActions_Hodir.h"
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

bool HodirMoveSnowpackedIcicleAction::isUseful()
{
    // Check boss and it is alive
    Unit* boss = AI_VALUE2(Unit*, "find target", "hodir");
    if (!boss || !boss->IsAlive())
        return false;

    // Check if boss is casting Flash Freeze
    if (!boss->HasUnitState(UNIT_STATE_CASTING) || !boss->FindCurrentSpellBySpellId(SPELL_FLASH_FREEZE))
        return false;

    // Only a Snowpacked Icicle blocks line of sight to Flash Freeze; a Toasty Fire does not
    Creature* target = bot->FindNearestCreature(NPC_SNOWPACKED_ICICLE, 100.0f);
    if (!target)
        return false;

    // Check that bot is stacked on the Snowpacked Icicle
    if (bot->GetDistance2d(target->GetPositionX(), target->GetPositionY()) <= 5.0f)
        return false;

    return true;
}

bool HodirMoveSnowpackedIcicleAction::Execute(Event /*event*/)
{
    Creature* target = bot->FindNearestCreature(NPC_SNOWPACKED_ICICLE, 100.0f);
    if (!target)
        return false;

    return MoveTo(target->GetMapId(), target->GetPositionX(), target->GetPositionY(), target->GetPositionZ(), false,
                  false, false, true, MovementPriority::MOVEMENT_NORMAL, true);
}

bool HodirBitingColdJumpAction::Execute(Event /*event*/)
{
    bot->RemoveAurasDueToSpell(SPELL_BITING_COLD_PLAYER_AURA);

    return true;

    // Backup when the overall strategy without cheat will be more vialable

    // int mapId = bot->GetMap()->GetId();
    // int x = bot->GetPositionX();
    // int y = bot->GetPositionY();
    // int z = bot->GetPositionZ() + 3.98f;
    // float speed = 7.96f;

    // UpdateMovementState();
    // if (!IsMovingAllowed(mapId, x, y, z))
    //{
    //     return false;
    // }
    // MovementPriority priority;
    // if (IsWaitingForLastMove(priority))
    //{
    //     return false;
    // }

    // MotionMaster& mm = *bot->GetMotionMaster();
    // mm.Clear();
    // mm.MoveJump(x, y, z, speed, speed, 1, AI_VALUE(Unit*, "current target"));
    // mm.MoveFall(0, true);
    // AI_VALUE(LastMovement&, "last movement").Set(mapId, x, y, z, bot->GetOrientation(), 1000, priority);

    // return true;
}

bool HodirBitingColdJumpAction::isUseful()
{
    return botAI->HasCheat(BotCheatMask::raid);
}

bool HodirFreeFrozenHelperAction::isUseful()
{
    HodirFreeFrozenHelperTrigger trigger(botAI);
    return trigger.IsActive();
}

bool HodirFreeFrozenHelperAction::Execute(Event /*event*/)
{
    Creature* block = bot->FindNearestCreature(NPC_HODIR_FLASH_FREEZE_BLOCK, 40.0f);
    if (!block || !block->IsAlive())
        return false;

    return Attack(block);
}

bool HodirSpreadStormCloudAction::isUseful()
{
    HodirSpreadStormCloudTrigger trigger(botAI);
    return trigger.IsActive();
}

bool HodirSpreadStormCloudAction::Execute(Event /*event*/)
{
    // Head to the nearest ally so Storm Power lands on a real cluster. Averaging the whole raid could aim
    // at an empty midpoint between two groups, where the carrier would buff nobody and never stabilise.
    Unit* nearestAlly = nullptr;
    float best = 0.0f;
    for (auto const& guid : AI_VALUE(GuidVector, "nearest friendly players"))
    {
        Unit* ally = botAI->GetUnit(guid);
        if (!ally || !ally->IsAlive())
            continue;

        float const dist = bot->GetExactDist2d(ally);
        if (!nearestAlly || dist < best)
        {
            best = dist;
            nearestAlly = ally;
        }
    }

    if (!nearestAlly)
        return false;

    return MoveTo(nearestAlly->GetMapId(), nearestAlly->GetPositionX(), nearestAlly->GetPositionY(),
                  nearestAlly->GetPositionZ(), false, false, false, true, MovementPriority::MOVEMENT_NORMAL);
}

bool HodirMoveToToastyFireAction::isUseful()
{
    HodirMoveToToastyFireTrigger trigger(botAI);
    return trigger.IsActive();
}

bool HodirMoveToToastyFireAction::Execute(Event /*event*/)
{
    Creature* fire = bot->FindNearestCreature(NPC_TOASTY_FIRE, 60.0f);
    if (!fire)
        return false;

    return MoveTo(fire->GetMapId(), fire->GetPositionX(), fire->GetPositionY(), fire->GetPositionZ(), false, false,
                  false, true, MovementPriority::MOVEMENT_NORMAL);
}
