#include "UldTriggers_Hodir.h"

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

bool HodirBitingColdTrigger::IsActive()
{
    Unit* boss = AI_VALUE2(Unit*, "find target", "hodir");

    // Check boss and it is alive
    if (!boss || !boss->IsAlive())
    {
        return false;
    }

    Player* master = botAI->GetMaster();
    if (!master || !master->IsAlive())
        return false;

    return botAI->GetAura("biting cold", bot, false, false, 2) &&
           !botAI->GetAura("biting cold", master, false, false, 2);
}

// Snowpacked Icicle Target
bool HodirNearSnowpackedIcicleTrigger::IsActive()
{
    // Check boss and it is alive
    Unit* boss = AI_VALUE2(Unit*, "find target", "hodir");
    if (!boss || !boss->IsAlive())
    {
        return false;
    }

    // Check if boss is casting Flash Freeze
    if (!boss->HasUnitState(UNIT_STATE_CASTING) || !boss->FindCurrentSpellBySpellId(SPELL_FLASH_FREEZE))
    {
        return false;
    }

    // Prefer a Snowpacked Icicle; fall back to a Toasty Fire when none has dropped nearby
    Creature* target = bot->FindNearestCreature(NPC_SNOWPACKED_ICICLE, 100.0f);
    if (!target)
        target = bot->FindNearestCreature(NPC_TOASTY_FIRE, 100.0f);
    if (!target)
        return false;

    // Check that bot is stacked on the safe spot
    if (bot->GetDistance2d(target->GetPositionX(), target->GetPositionY()) <= 5.0f)
    {
        return false;
    }

    return true;
}
