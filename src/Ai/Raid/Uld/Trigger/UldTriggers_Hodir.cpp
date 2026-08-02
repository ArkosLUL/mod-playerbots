#include "UldTriggers_Hodir.h"

#include "GameObject.h"
#include "Object.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "SpellMgr.h"
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

    // Only a Snowpacked Icicle blocks line of sight to Flash Freeze; a Toasty Fire does not
    Creature* target = bot->FindNearestCreature(NPC_SNOWPACKED_ICICLE, 100.0f);
    if (!target)
        return false;

    // Check that bot is stacked on the Snowpacked Icicle
    if (bot->GetDistance2d(target->GetPositionX(), target->GetPositionY()) <= 5.0f)
    {
        return false;
    }

    return true;
}

bool HodirFreeFrozenHelperTrigger::IsActive()
{
    if (!IsHodirHardModeActive(botAI))
        return false;

    // Helpers spawn encased in an ice block; the whole raid frees them fast at the pull so their buffs
    // are up for the timer. A wide search keeps it working wherever the raid stacked on engage.
    Creature* block = bot->FindNearestCreature(NPC_HODIR_FLASH_FREEZE_BLOCK, 40.0f);
    return block != nullptr && block->IsAlive();
}

bool HodirSpreadStormCloudTrigger::IsActive()
{
    if (!IsHodirHardModeActive(botAI))
        return false;

    uint32 const stormCloudId = sSpellMgr->GetSpellIdForDifficulty(SPELL_HODIR_STORM_CLOUD, bot);
    if (!bot->HasAura(stormCloudId))
        return false;

    // Storm Power radiates from the carrier, so it only matters when the carrier is off on its own.
    uint32 nearby = 0;
    for (auto const& guid : AI_VALUE(GuidVector, "nearest friendly players"))
    {
        Unit* ally = botAI->GetUnit(guid);
        if (ally && ally->IsAlive() && bot->GetExactDist2d(ally) <= ULDUAR_HODIR_STORM_CLOUD_STACK_RADIUS)
            ++nearby;
    }

    return nearby < 2;
}

bool HodirMoveToToastyFireTrigger::IsActive()
{
    if (!IsHodirHardModeActive(botAI))
        return false;

    // Only worth moving once Biting Cold is actually ticking on the bot; the fire prevents further stacks.
    if (!bot->HasAura(SPELL_BITING_COLD_PLAYER_AURA))
        return false;

    // Already protected by a fire - nothing to do.
    if (bot->FindNearestCreature(NPC_TOASTY_FIRE, ULDUAR_HODIR_TOASTY_FIRE_RADIUS))
        return false;

    // A fire has to be within reach for this to be worth abandoning the current spot.
    return bot->FindNearestCreature(NPC_TOASTY_FIRE, 60.0f) != nullptr;
}
