#include "UldTriggers_Hodir.h"

#include <list>

#include "Object.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "SpellMgr.h"
#include "UldBossHelper.h"
#include "UldScripts.h"
#include "RaidBossHelpers.h"
#include "ScriptedCreature.h"
#include "SharedDefines.h"
#include "Trigger.h"

// The nearest icicle of an entry that has not detonated yet, or nullptr. Both icicle entries are
// non-selectable, so they never reach "possible targets" and have to be found by a direct search.
static Creature* NearestHodirIcicle(Player* bot, uint32 entry, float radius)
{
    Creature* icicle = bot->FindNearestCreature(entry, radius);
    return IsHodirIcicleLethal(icicle) ? icicle : nullptr;
}

bool HodirBitingColdTrigger::IsActive()
{
    if (!IsHodirEngaged(botAI))
        return false;

    if (bot->HasAura(SPELL_HODIR_FLASH_FREEZE_TRAPPED))
        return false;

    // A Toasty Fire counts as movement on every tick, so a bot standing in one is already shedding a
    // stack every two seconds without going anywhere.
    if (bot->HasAura(SPELL_HODIR_TOASTY_FIRE_AURA))
        return false;

    // Stateless on purpose. The action owns the arm-at-two-stacks threshold and the shed-until-clear
    // latch, because it is the cached instance - anything held here is lost by a stack-allocated copy.
    return bot->HasAura(SPELL_BITING_COLD_PLAYER_AURA);
}

bool HodirNearSnowpackedIcicleTrigger::IsActive()
{
    if (!GetHodir(botAI))
        return false;

    // Keyed on the shelter existing, not on the boss casting: the drift detonates for 14000 in 7 yd
    // at 3.7s, so running at it early is what the raid must not do. The shelter it leaves lives 12s
    // and the freeze lands at 9s, which is 5.3s to cross the room.
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
    // Combat-gated: an anchor that fires on sight has the raid walking to its spots before anyone
    // has pulled.
    if (!IsHodirEngaged(botAI))
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

    HodirBitingColdTrigger bitingCold(botAI);
    if (bitingCold.IsActive())
        return false;

    Position anchor;
    float tolerance = 0.0f;
    if (!GetHodirAnchor(botAI, bot, anchor, tolerance))
        return false;

    // The dodge stands down once the bot itself is clear, but the icicle is still counting down on
    // the spot it left. Without this the anchor walks it straight back under the blast.
    for (uint32 entry : {NPC_HODIR_ICICLE_SMALL, NPC_HODIR_ICICLE_DRIFT})
    {
        std::list<Creature*> icicles;
        bot->GetCreatureListWithEntryInGrid(icicles, entry, ULDUAR_HODIR_ROOM_SEARCH_RADIUS);
        for (Creature* icicle : icicles)
            if (IsHodirIcicleLethal(icicle) && icicle->GetExactDist2d(&anchor) < ULDUAR_HODIR_ICE_SHARDS_CLEAR)
                return false;
    }

    return bot->GetExactDist2d(&anchor) > tolerance;
}

bool HodirSetDpsPriorityTrigger::IsActive()
{
    // The action calls Attack() directly, so without the combat gate the first bot to lay eyes on him
    // pulls. Whoever pulls flips this for everyone, which is what makes the raid engage together.
    if (!IsHodirEngaged(botAI))
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
