#include "UldActions_Auriaya.h"
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

bool AuriayaFallFromFloorAction::Execute(Event /*event*/)
{
    Player* master = botAI->GetMaster();

    if (!master)
        return false;

    return bot->TeleportTo(bot->GetMapId(), master->GetPositionX(), master->GetPositionY(), master->GetPositionZ(),
                           master->GetOrientation());
}

bool AuriayaFallFromFloorAction::isUseful()
{
    AuriayaFallFromFloorTrigger auriayaFallFromFloorTrigger(botAI);
    return auriayaFallFromFloorTrigger.IsActive();
}

bool AuriayaSonicScreechAction::isUseful()
{
    AuriayaSonicScreechTrigger auriayaSonicScreechTrigger(botAI);
    return auriayaSonicScreechTrigger.IsActive();
}

bool AuriayaSonicScreechAction::Execute(Event /*event*/)
{
    Unit* boss = GetAuriaya(botAI);
    if (!boss)
        return false;

    // Sidestep the shortest way out of the frontal cone while keeping current range
    Position const dest = GetPositionOutsideFrontalCone(bot, boss, ULDUAR_AURIAYA_SONIC_SCREECH_CONE);
    return MoveTo(boss->GetMapId(), dest.GetPositionX(), dest.GetPositionY(), dest.GetPositionZ(), false, false, false,
                  true, MovementPriority::MOVEMENT_COMBAT);
}

bool AuriayaMarkDpsTargetAction::isUseful()
{
    AuriayaMarkDpsTargetTrigger auriayaMarkDpsTargetTrigger(botAI);
    return auriayaMarkDpsTargetTrigger.IsActive();
}

bool AuriayaMarkDpsTargetAction::Execute(Event /*event*/)
{
    Unit* target = GetAuriayaFocusTarget(botAI);
    if (!target)
        return false;

    MarkTargetWithSkull(bot, target);
    SetRtiTarget(botAI, "skull", target);
    return true;
}

bool AuriayaSentryTauntAction::isUseful()
{
    AuriayaSentryTauntTrigger auriayaSentryTauntTrigger(botAI);
    return auriayaSentryTauntTrigger.IsActive();
}

bool AuriayaSentryTauntAction::Execute(Event /*event*/)
{
    return UldCastClassTaunt(botAI, GetAuriayaLooseSentry(botAI, bot));
}

bool AuriayaTankFacingAction::isUseful()
{
    AuriayaTankFacingTrigger auriayaTankFacingTrigger(botAI);
    return auriayaTankFacingTrigger.IsActive();
}

bool AuriayaTankFacingAction::Execute(Event /*event*/)
{
    Unit* boss = GetAuriaya(botAI);
    if (!boss)
        return false;

    float error = 0.0f;
    if (!GetAuriayaFacingError(botAI, bot, error))
        return false;

    // Walk one small step around Auriaya at the range already held. Swinging straight to the far
    // side would drag her through the raid, and a wide arc off melee range drops threat.
    float const radius = std::max(2.0f, bot->GetExactDist2d(boss));
    float angle = std::atan2(bot->GetPositionY() - boss->GetPositionY(), bot->GetPositionX() - boss->GetPositionX());
    angle += (error < 0.0f) ? ULDUAR_AURIAYA_FACING_ARC_STEP : -ULDUAR_AURIAYA_FACING_ARC_STEP;

    float const moveX = boss->GetPositionX() + radius * std::cos(angle);
    float const moveY = boss->GetPositionY() + radius * std::sin(angle);

    return MoveTo(bot->GetMapId(), moveX, moveY, bot->GetPositionZ(), false, false, false, false,
                  MovementPriority::MOVEMENT_FORCED, true, false);
}
