#include "UldTriggers_Hodir.h"

#include <list>

#include "Object.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "SpellMgr.h"
#include "Timer.h"
#include "UldBossHelper.h"
#include "UldScripts.h"
#include "RaidBossHelpers.h"
#include "ScriptedCreature.h"
#include "SharedDefines.h"
#include "Trigger.h"

// The nearest live icicle of an entry, or nullptr. Both icicle entries are non-selectable, so they
// never reach "possible targets" and have to be found by a direct search.
static Creature* NearestHodirIcicle(Player* bot, uint32 entry, float radius)
{
    Creature* icicle = bot->FindNearestCreature(entry, radius);
    return icicle && icicle->IsAlive() ? icicle : nullptr;
}

bool HodirBitingColdTrigger::IsActive()
{
    if (!GetHodir(botAI))
        return false;

    if (bot->HasAura(SPELL_HODIR_FLASH_FREEZE_TRAPPED) || bot->HasAura(SPELL_HODIR_TOASTY_FIRE_AURA))
        return false;

    uint32 const now = getMSTime();
    if (bot->isMoving())
    {
        _stillSince = now;
        return false;
    }

    if (!_stillSince)
    {
        _stillSince = now;
        return false;
    }

    // Already ticking is reason enough; otherwise wait until standing still is about to cost a stack.
    if (bot->HasAura(SPELL_BITING_COLD_PLAYER_AURA))
        return true;

    return getMSTimeDiff(_stillSince, now) >= ULDUAR_HODIR_JUMP_IDLE_MS;
}

bool HodirNearSnowpackedIcicleTrigger::IsActive()
{
    if (!GetHodir(botAI))
        return false;

    // Keyed on the shelter existing, not on the boss casting: the drift is airborne for the first
    // two seconds and detonates for 14000 in 7 yd when it lands, so running at it early is what the
    // raid must not do. The shelter then lives 12s against a 9s cast, which is ample.
    Creature* shelter = GetHodirSharedShelter(botAI, bot);
    if (!shelter)
        return false;

    return bot->GetExactDist2d(shelter) > ULDUAR_HODIR_SAFE_AREA_TOLERANCE;
}

bool HodirIcicleDodgeTrigger::IsActive()
{
    if (!GetHodir(botAI))
        return false;

    if (NearestHodirIcicle(bot, NPC_HODIR_ICICLE_SMALL, ULDUAR_HODIR_ICE_SHARDS_CLEAR))
        return true;

    // A drift icicle is worth dodging only until it lands. After that the Snowpacked Icicle Target
    // it leaves behind is the shelter, and dodging would push the bot out of the one safe spot.
    Creature* drift = NearestHodirIcicle(bot, NPC_HODIR_ICICLE_DRIFT, ULDUAR_HODIR_ICE_SHARDS_CLEAR);
    if (!drift)
        return false;

    return drift->FindNearestCreature(NPC_SNOWPACKED_ICICLE, ULDUAR_HODIR_SAFE_AREA_RADIUS) == nullptr;
}

bool HodirRaidPositionTrigger::IsActive()
{
    if (!GetHodir(botAI))
        return false;

    if (bot->HasAura(SPELL_HODIR_FLASH_FREEZE_TRAPPED))
        return false;

    // Surviving beats standing on a spot, and letting the anchor fight a dodge is what makes bots
    // step out and get walked straight back in.
    HodirNearSnowpackedIcicleTrigger shelter(botAI);
    if (shelter.IsActive())
        return false;

    HodirIcicleDodgeTrigger dodge(botAI);
    if (dodge.IsActive())
        return false;

    Position anchor;
    float tolerance = 0.0f;
    if (!GetHodirAnchor(botAI, bot, anchor, tolerance))
        return false;

    return bot->GetExactDist2d(&anchor) > tolerance;
}

bool HodirSetDpsPriorityTrigger::IsActive()
{
    if (!GetHodir(botAI))
        return false;

    return !botAI->IsTank(bot) && !botAI->IsHeal(bot);
}

bool HodirFrozenBlowsSwapTrigger::IsActive()
{
    Unit* boss = GetHodir(botAI);
    if (!boss)
        return false;

    bool const frozenBlows = boss->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_HODIR_FROZEN_BLOWS, bot));
    bool const holding = boss->GetVictim() == bot;

    // The off-tank takes him for the 20s window; the main tank takes him back once it drops.
    if (botAI->IsAssistTankOfIndex(bot, 0, true))
        return frozenBlows && !holding;

    if (botAI->IsMainTank(bot))
        return !frozenBlows && !holding;

    return false;
}

bool HodirSpreadStormCloudTrigger::IsActive()
{
    if (!GetHodir(botAI))
        return false;

    // A tank leaving the corner mid-Frozen-Blows costs more than the buff is worth.
    if (botAI->IsTank(bot))
        return false;

    return bot->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_HODIR_STORM_CLOUD, bot));
}
