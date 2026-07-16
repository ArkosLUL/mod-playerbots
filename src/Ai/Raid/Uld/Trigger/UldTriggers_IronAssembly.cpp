#include "UldTriggers_IronAssembly.h"

#include "GameObject.h"
#include "Object.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "Group.h"
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

bool IronAssemblyLightningTendrilsTrigger::IsActive()
{
    // Check boss and it is alive
    Unit* boss = AI_VALUE2(Unit*, "find target", "stormcaller brundir");
    if (!boss || !boss->IsAlive())
        return false;

    // Check if bot is within 35 yards of the boss
    if (boss->GetDistance(bot) > 35.0f)
        return false;

    // Check if the boss has the Lightning Tendrils aura
    return boss->HasAura(SPELL_LIGHTNING_TENDRILS_10_MAN) || boss->HasAura(SPELL_LIGHTNING_TENDRILS_25_MAN);
}

bool IronAssemblyOverloadTrigger::IsActive()
{
    // Check if bot is tank
    if (botAI->IsTank(bot))
        return false;

    // Check boss and it is alive
    Unit* boss = AI_VALUE2(Unit*, "find target", "stormcaller brundir");
    if (!boss || !boss->IsAlive())
        return false;

    // Check if bot is within 35 yards of the boss
    if (boss->GetDistance(bot) > 35.0f)
        return false;

    // Check if the boss has the Overload aura
    return boss->HasAura(SPELL_OVERLOAD_10_MAN) || boss->HasAura(SPELL_OVERLOAD_25_MAN) ||
           boss->HasAura(SPELL_OVERLOAD_10_MAN_2) || boss->HasAura(SPELL_OVERLOAD_25_MAN_2);
}

bool IronAssemblyRuneOfPowerTrigger::IsActive()
{
    Unit* target = botAI->GetUnit(bot->GetTarget());
    if (!target || !target->IsAlive())
        return false;

    if (!target->HasAura(SPELL_RUNE_OF_POWER))
        return false;

    if (target->GetVictim() != bot)
        return false;

    return botAI->IsTank(bot);
}

bool IronAssemblyKillOrderTrigger::IsActive()
{
    if (!IsIronAssemblyHardModeActive(botAI))
        return false;

    // One bot drives the marker to avoid contention.
    if (!botAI->IsMainTank(bot))
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    Unit* next = GetIronAssemblyNextKillTarget(botAI);
    if (!next)
        return false;

    // Already marked - nothing to do.
    return group->GetTargetIcon(RtiTargetValue::skullIndex) != next->GetGUID();
}

bool IronAssemblyFusionPunchSwapTrigger::IsActive()
{
    if (!IsSteelbreakerEmpowered(botAI))
        return false;

    Unit* steelbreaker = GetFirstAliveUnitByEntry(botAI, NPC_STEELBREAKER);
    if (!steelbreaker)
        return false;

    // Only the two designated swap partners (main tank + first assist tank) trade the boss.
    bool const isMainTank = botAI->IsMainTank(bot);
    bool const isFirstAssistTank = botAI->IsAssistTankOfIndex(bot, 0);
    if (!isMainTank && !isFirstAssistTank)
        return false;

    // bot must be the off-tank (not the one currently holding the boss).
    Unit* activeTank = steelbreaker->GetVictim();
    if (!activeTank || activeTank == bot)
        return false;

    Player* activeTankPlayer = activeTank->ToPlayer();
    if (!activeTankPlayer)
        return false;

    bool const partnerIsSwapTank = isMainTank ? PlayerbotAI::IsAssistTankOfIndex(activeTankPlayer, 0)
                                              : PlayerbotAI::IsMainTank(activeTankPlayer);
    if (!partnerIsSwapTank)
        return false;

    // Swap only for Overwhelming Power: it is the stacking phase-3 debuff that one-shots the
    // tank unless it is shed by dropping threat. Fusion Punch is a short, frequently recast DoT;
    // driving swaps off it would ping-pong the boss between the two tanks on every cast.
    if (bot->HasAura(SPELL_OVERWHELMING_POWER))
        return false;

    return activeTank->HasAura(SPELL_OVERWHELMING_POWER);
}
