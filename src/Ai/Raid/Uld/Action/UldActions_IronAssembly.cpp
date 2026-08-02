#include "UldActions_IronAssembly.h"
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
#include "UldHardMode.h"
#include "UldScripts.h"
#include "RaidBossHelpers.h"
#include "RtiValue.h"
#include "ScriptedCreature.h"
#include "ServerFacade.h"
#include "Unit.h"
#include "Vehicle.h"
#include <RtiTargetValue.h>
#include <TankAssistStrategy.h>

bool IronAssemblyLightningTendrilsAction::isUseful()
{
    IronAssemblyLightningTendrilsTrigger ironAssemblyLightningTendrilsTrigger(botAI);
    return ironAssemblyLightningTendrilsTrigger.IsActive();
}

bool IronAssemblyLightningTendrilsAction::Execute(Event /*event*/)
{
    const float radius = 18.0f + 10.0f;  // 18 yards + 10 yards for safety

    Unit* boss = AI_VALUE2(Unit*, "find target", "stormcaller brundir");
    if (!boss)
        return false;

    float currentDistance = bot->GetDistance2d(boss);

    if (currentDistance < radius)
        return MoveAway(boss, radius - currentDistance);

    return false;
}

bool IronAssemblyOverloadAction::isUseful()
{
    IronAssemblyOverloadTrigger ironAssemblyOverloadTrigger(botAI);
    return ironAssemblyOverloadTrigger.IsActive();
}

bool IronAssemblyOverloadAction::Execute(Event /*event*/)
{
    const float radius = 20.0f + 5.0f;  // 20 yards + 5 yards for safety

    Unit* boss = AI_VALUE2(Unit*, "find target", "stormcaller brundir");
    if (!boss)
        return false;

    float currentDistance = bot->GetDistance2d(boss);

    if (currentDistance < radius)
        return MoveAway(boss, radius - currentDistance);

    return false;
}

bool IronAssemblyRuneOfPowerAction::isUseful()
{
    IronAssemblyRuneOfPowerTrigger ironAssemblyRuneOfPowerTrigger(botAI);
    return ironAssemblyRuneOfPowerTrigger.IsActive();
}

bool IronAssemblyRuneOfPowerAction::Execute(Event /*event*/)
{
    Unit* target = botAI->GetUnit(bot->GetTarget());
    if (!target || !target->IsAlive())
        return false;

    return MoveAway(target, 10.0f, true);
}

bool IronAssemblyKillOrderAction::isUseful()
{
    // Coarse relevance gate; the paired trigger does the full check. Only the main tank drives
    // the marker, so skip the rest early.
    return IsIronAssemblyHardModeActive(botAI) && botAI->IsMainTank(bot);
}

bool IronAssemblyKillOrderAction::Execute(Event /*event*/)
{
    Unit* next = GetIronAssemblyNextKillTarget(botAI);
    if (!next)
        return false;

    MarkTargetWithSkull(bot, next);
    SetRtiTarget(botAI, "skull", next);
    return true;
}

bool IronAssemblyFusionPunchSwapAction::isUseful()
{
    // Coarse relevance gate; the paired trigger decides the actual swap.
    return IsSteelbreakerEmpowered(botAI);
}

bool IronAssemblyFusionPunchSwapAction::Execute(Event event)
{
    Unit* steelbreaker = GetFirstAliveUnitByEntry(botAI, NPC_STEELBREAKER);
    if (!steelbreaker)
        return false;

    if (AI_VALUE(Unit*, "current target") != steelbreaker)
        return Attack(steelbreaker);

    if (steelbreaker->GetVictim() != bot)
        return botAI->DoSpecificAction("taunt spell", event, true);

    return false;
}
