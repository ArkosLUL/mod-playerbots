#include "UldActions_Auriaya.h"
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
    Unit* boss = AI_VALUE2(Unit*, "find target", "auriaya");
    if (!boss || !boss->IsAlive())
        return false;

    // Sidestep the shortest way out of the frontal cone while keeping current range
    Position const dest = GetPositionOutsideFrontalCone(bot, boss, M_PI / 2.0f);
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
    Unit* target = GetFirstAliveUnitByEntry(botAI, NPC_AURIAYA_FERAL_DEFENDER);
    if (!target)
        target = GetFirstAliveUnitByEntry(botAI, NPC_AURIAYA_SANCTUM_SENTRY);

    if (!target)
        return false;

    MarkTargetWithSkull(bot, target);
    SetRtiTarget(botAI, "skull", target);
    return true;
}
