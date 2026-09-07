#include "UldTriggers_Hodir.h"

#include <algorithm>
#include <cmath>
#include <list>
#include <utility>

#include "Object.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "SpellMgr.h"
#include "UldEncounter_Hodir.h"
#include "UldScripts.h"
#include "EncounterHelpers.h"
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

// 2D distance from a point to the walk between two positions, endpoints included, so one test
// covers where the bot is, where it is headed, and everything it crosses on the way.
static float DistToSegment2d(Position const& point, Position const& from, Position const& to)
{
    float const dx = to.GetPositionX() - from.GetPositionX();
    float const dy = to.GetPositionY() - from.GetPositionY();
    float const lengthSq = dx * dx + dy * dy;

    float t = 0.0f;
    if (lengthSq > 0.0f)
    {
        t = ((point.GetPositionX() - from.GetPositionX()) * dx +
             (point.GetPositionY() - from.GetPositionY()) * dy) /
            lengthSq;
        t = std::clamp(t, 0.0f, 1.0f);
    }

    float const nearestX = from.GetPositionX() + dx * t;
    float const nearestY = from.GetPositionY() + dy * t;
    return std::sqrt((point.GetPositionX() - nearestX) * (point.GetPositionX() - nearestX) +
                     (point.GetPositionY() - nearestY) * (point.GetPositionY() - nearestY));
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

    // A drift counts, at its own wider radius: it detonates for 14000 in 7 yd at 3.7s, so the raid
    // cannot run onto one, but it can stand on the edge of the blast for those 3.7s instead of waiting
    // them out and then crossing the room in the 5.1s that are left.
    Creature* shelter = GetHodirShelter(botAI, bot);
    if (!shelter)
        return false;

    // Release, not the park distance the action aims for. Testing the same number at both ends stands
    // this trigger down the tick the bot arrives, and the ring anchor - which is suppressed only while
    // this is active - takes the very next tick and walks it back out of the shelter.
    return bot->GetExactDist2d(shelter) > GetHodirShelterRelease(shelter);
}

bool HodirFrostResistanceTrigger::IsActive()
{
    if (!IsHodirEngaged(botAI))
        return false;

    if (botAI->HasAura("frost resistance aura", bot))
        return false;

    return GetHodirResistancePaladin(botAI, bot) == bot;
}

bool HodirIcicleDodgeTrigger::IsActive()
{
    if (!GetHodir(botAI))
        return false;

    // Tanks eat the icicle. Hodir follows, so a tank that steps 12 yd off its corner drags him with
    // it and the ranged formation is suddenly standing in melee. The Biting Cold shuttle already
    // gives them the movement they need without leaving the spot.
    if (botAI->IsTank(bot))
        return false;

    // Leaving is decided on the radius that kills; the clear is margin for where the bot lands, and
    // testing it at both ends had bots stepping out of pools they were never standing in.
    if (NearestHodirIcicle(bot, NPC_HODIR_ICICLE_SMALL,
                           ULDUAR_HODIR_ICE_SHARDS_RADIUS + ULDUAR_HODIR_DODGE_TRIGGER_MARGIN))
        return true;

    // A drift icicle is worth dodging only until it lands. After that the Snowpacked Icicle Target
    // it leaves behind is the shelter, and dodging would push the bot out of the one safe spot.
    Creature* drift = NearestHodirIcicle(bot, NPC_HODIR_ICICLE_DRIFT,
                                         ULDUAR_HODIR_BIG_SHARDS_RADIUS + ULDUAR_HODIR_DODGE_TRIGGER_MARGIN);
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

    // The arm threshold, not HodirBitingColdTrigger. That one fires on any stack because the action
    // owns the shed-to-zero latch, but 87% of the time a bot holds Biting Cold it holds exactly one -
    // 32.8% of the fight each - and there the shuttle does nothing at all. Standing the anchor down
    // for it is what kept the ranged out of Starlight: a usable zone was in range on 85% of their
    // ticks and they were standing in one for 22%.
    if (IsHodirBitingColdShedArmed(bot))
        return false;

    Position anchor;
    float tolerance = 0.0f;
    if (!GetHodirAnchor(botAI, bot, anchor, tolerance))
        return false;

    // The dodge stands down the moment the bot is clear, but the icicle it stepped out of stays lethal
    // for another 3.7s. Testing the whole walk back and not just the spot at the end of it is what
    // stops the anchor and the dodge trading the bot back and forth for the rest of that window.
    Position const self = bot->GetPosition();
    for (auto const& entry : {std::make_pair(static_cast<uint32>(NPC_HODIR_ICICLE_SMALL),
                                             ULDUAR_HODIR_ICE_SHARDS_CLEAR),
                              std::make_pair(static_cast<uint32>(NPC_HODIR_ICICLE_DRIFT),
                                             ULDUAR_HODIR_BIG_SHARDS_CLEAR)})
    {
        std::list<Creature*> icicles;
        bot->GetCreatureListWithEntryInGrid(icicles, entry.first, ULDUAR_HODIR_ROOM_SEARCH_RADIUS);
        for (Creature* icicle : icicles)
            if (IsHodirIcicleLethal(icicle) &&
                DistToSegment2d(icicle->GetPosition(), self, anchor) < entry.second)
                return false;
    }

    // Reactive, not restoring. Walking to the slot whenever the bot is off it makes the anchor a
    // spring: every dodge displaces further than the tolerance, so every dodge buys a return trip and
    // the two actions trade the bot for the rest of the icicle's life. Firing only on a broken
    // constraint issues one destination and then goes quiet, and the movement layer walks it out.
    if (bot->GetExactDist2d(&anchor) <= tolerance)
        return false;

    // Tanks keep the spring. Their anchor is a fixed corner and Hodir follows whoever holds him, so a
    // tank that drifts takes the boss with it and lands him on the ranged formation. They also never
    // run the generic dodge, so nothing is fighting them for the spot.
    if (botAI->IsTank(bot))
        return true;

    // Home regardless. Without a hard leash nothing pulls a bot back that dodged its way out of heal
    // range, because none of the constraints below care how far from the raid it ended up.
    if (bot->GetExactDist2d(&anchor) > ULDUAR_HODIR_RETURN_LEASH)
        return true;

    // Standing in his melee with Frozen Blows up is one swing from dead. Gated on the slot being
    // clear as well: when he has walked onto the formation the slot is no better than here, and
    // firing anyway just slides the bot around inside his reach.
    Unit* hodir = GetHodir(botAI);
    if (hodir && bot->GetExactDist2d(hodir) < ULDUAR_HODIR_RANGED_MIN_BOSS_GAP &&
        anchor.GetExactDist2d(hodir) >= ULDUAR_HODIR_RANGED_MIN_BOSS_GAP)
        return true;

    // Nothing here tests Starlight. 62807 reaches about 4 yd, not the 8 its DBC row claims, so a bot
    // is outside every zone almost all of the time and a constraint on it can never be satisfied -
    // it just walks. GetHodirAnchor hands a zone to the one bot whose slot is already beside it.

    // Clumped. One icicle splashes 4 yd, so neighbours inside the declump radius mean one landing
    // catches both.
    for (auto const& guid : AI_VALUE(GuidVector, "nearest friendly players"))
    {
        Unit* ally = botAI->GetUnit(guid);
        if (!ally || ally == bot || !ally->IsAlive() || !ally->IsPlayer())
            continue;

        if (!PlayerbotAI::IsRanged(ally->ToPlayer()) && !PlayerbotAI::IsHeal(ally->ToPlayer()))
            continue;

        if (bot->GetExactDist2d(ally) < ULDUAR_HODIR_DECLUMP_RADIUS)
            return true;
    }

    return false;
}

bool HodirSetDpsPriorityTrigger::IsActive()
{
    // The action calls Attack() directly, so without the combat gate the first bot to lay eyes on him
    // pulls. Whoever pulls flips this for everyone, which is what makes the raid engage together.
    if (!IsHodirEngaged(botAI))
        return false;

    // Tanks are on this node too, for Hodir only - the action hands them the boss and nothing else.
    // Healers keep the generic picker so nothing pulls them off healing.
    return !botAI->IsHeal(bot);
}

bool HodirFrozenBlowsSwapTrigger::IsActive()
{
    Unit* boss = GetHodir(botAI);
    if (!boss)
        return false;

    bool const frozenBlows = HodirFrozenBlowsActive(botAI, bot);
    bool const holding = boss->GetVictim() == bot;

    // The off-tank takes him for the 20s window; the main tank takes him back once it drops. Taking the
    // window under the floor is a death rather than a swap - two hits of 63511 arrive 2.4s apart and
    // either one is most of a tank's pool.
    if (botAI->IsAssistTankOfIndex(bot, 0, true))
        return frozenBlows && !holding && bot->GetHealthPct() >= ULDUAR_HODIR_TAUNT_HEALTH_FLOOR;

    if (botAI->IsMainTank(bot))
        return !frozenBlows && !holding;

    return false;
}

bool HodirRedirectThreatTrigger::IsActive()
{
    if (!IsHodirEngaged(botAI))
        return false;

    return bot->getClass() == CLASS_HUNTER || bot->getClass() == CLASS_ROGUE;
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
