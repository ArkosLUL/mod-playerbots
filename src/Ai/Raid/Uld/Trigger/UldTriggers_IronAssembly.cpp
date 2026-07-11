#include "UldTriggers_IronAssembly.h"

#include "GameObject.h"
#include "Object.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldBossHelper.h"
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
