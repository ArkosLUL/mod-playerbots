#include "UldActions_Vezax.h"
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

bool VezaxCheatAction::Execute(Event /*event*/)
{
    // Restore bot's mana to full
    uint32 maxMana = bot->GetMaxPower(POWER_MANA);
    if (maxMana > 0)
        bot->SetPower(POWER_MANA, maxMana);

    return true;
}

bool VezaxShadowCrashAction::Execute(Event /*event*/)
{
    // Find General Vezax boss
    Unit* boss = AI_VALUE2(Unit*, "find target", "general vezax");
    if (!boss || !boss->IsAlive())
        return false;

    // Get bot's current position relative to boss
    float bossX = boss->GetPositionX();
    float bossY = boss->GetPositionY();
    float bossZ = boss->GetPositionZ();

    float botX = bot->GetPositionX();
    float botY = bot->GetPositionY();

    // Calculate current angle and distance from boss
    float currentAngle = atan2(botY - bossY, botX - bossX);
    float currentDistance = bot->GetDistance2d(boss);

    // Shadow Crash can land on melee since #26884, so strafe out of the puddle at the bot's own
    // range instead of dragging everyone to 15 yards - melee that walks out stops attacking.
    bool const stayInMelee = botAI->IsMelee(bot) || botAI->IsTank(bot);
    float const minDistance = stayInMelee ? ULDUAR_VEZAX_SHADOW_CRASH_MELEE_MIN_RANGE
                                          : ULDUAR_VEZAX_SHADOW_CRASH_RANGED_MIN_RANGE;
    float const maxDistance = stayInMelee ? ULDUAR_VEZAX_SHADOW_CRASH_MELEE_MAX_RANGE
                                          : ULDUAR_VEZAX_SHADOW_CRASH_RANGED_MAX_RANGE;
    float const desiredDistance = std::clamp(currentDistance, minDistance, maxDistance);

    // Constant step length around the boss, so a melee bot on a tight radius still clears the
    // puddle in as few ticks as a ranged one further out.
    float const angleIncrement = ULDUAR_VEZAX_SHADOW_CRASH_STEP_YARDS / std::max(desiredDistance, 1.0f);
    float newAngle = currentAngle + angleIncrement;

    // Calculate new position
    float newX = bossX + desiredDistance * cos(newAngle);
    float newY = bossY + desiredDistance * sin(newAngle);
    float newZ = bossZ;  // Keep same Z level as boss

    // Move to the new position
    return MoveTo(boss->GetMapId(), newX, newY, newZ, false, false, false, true, MovementPriority::MOVEMENT_COMBAT,
                  true);
}

bool VezaxMarkOfTheFacelessAction::Execute(Event /*event*/)
{
    return MoveTo(bot->GetMapId(), ULDUAR_VEZAX_MARK_OF_THE_FACELESS_SPOT.GetPositionX(),
                  ULDUAR_VEZAX_MARK_OF_THE_FACELESS_SPOT.GetPositionY(),
                  ULDUAR_VEZAX_MARK_OF_THE_FACELESS_SPOT.GetPositionZ(), false, false, false, true,
                  MovementPriority::MOVEMENT_FORCED, true, false);
}

bool VezaxSaroniteAnimusAction::Execute(Event /*event*/)
{
    Unit* animus = GetFirstAliveUnitByEntry(botAI, NPC_VEZAX_SARONITE_ANIMUS);
    if (!animus)
        return false;

    return Attack(animus);
}
