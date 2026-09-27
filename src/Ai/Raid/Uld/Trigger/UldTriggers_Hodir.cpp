#include "UldTriggers_Hodir.h"

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

    // Stateless on purpose. The action owns the arm threshold and the shed-to-floor latch, because it
    // is the cached instance and anything held here is lost by a stack-allocated copy.
    return bot->HasAura(SPELL_BITING_COLD_PLAYER_AURA);
}

bool HodirNearSnowpackedIcicleTrigger::IsActive()
{
    if (!GetHodir(botAI))
        return false;

    // Only while the cast is up. A target outlives the freeze by about 7s, and running to it after the
    // landing bought nothing: 134 of 358 shelter moves in one pull, and the tank flipped between the
    // shelter and his spot 8 times in 7s, dragging Hodir 8 yd off the centre.
    if (!IsHodirFlashFreezeIncoming(botAI))
        return false;

    // A drift counts, at its own wider radius: it detonates for 14000 in 7 yd at 3.7s, so the raid
    // cannot run onto one, but it can stand on the edge of the blast for those 3.7s instead of waiting
    // them out and then crossing the room in the 5.1s that are left.
    Creature* shelter = GetHodirShelter(botAI, bot);
    if (!shelter)
        return false;

    // Release, not the park distance the action aims for. Testing the same number at both ends stands
    // this trigger down the tick the bot arrives, and the position anchor - which is suppressed only while
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

    // Tanks eat the icicle. Hodir follows, so a tank that steps 12 yd off its spot drags him with
    // it and the ranged are suddenly standing in his melee. The Biting Cold shuttle already
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

    // With a spot in hand - a tank's hold spot, a buff stand for everyone else - that is the whole
    // question: walk there or stay. Both finders re-check their latched stand against the caster band
    // every tick, so a stand that is still offered is still a spot worth standing on, and the rules
    // below are for bots that have none.
    Position anchor;
    float tolerance = 0.0f;
    if (GetHodirAnchor(botAI, bot, anchor, tolerance))
    {
        // The dodge stands down the moment the bot is clear, but the icicle it stepped out of stays
        // lethal for another 3.7s. Testing the whole walk back and not just the spot at the end of it
        // is what stops the anchor and the dodge trading the bot back and forth for that window.
        if (IsHodirWalkThroughLiveIcicle(bot, anchor, ULDUAR_HODIR_ICE_SHARDS_CLEAR,
                                         ULDUAR_HODIR_BIG_SHARDS_CLEAR))
            return false;

        return bot->GetExactDist2d(&anchor) > tolerance;
    }

    // Ranged and healers only. A tank that steps back drags the boss off the fire, and a melee sits
    // inside the gap rule below every tick it is doing its job, so this node and reach melee would
    // trade it back and forth all fight. Same predicate the anchor uses, so the two cannot drift.
    if (!botAI->IsRanged(bot))
        return false;

    // Reactive, not restoring: each of these fires on a constraint that is actually broken and the
    // action walks the shortest step that fixes it. A spot the bot is sent back to whenever it is off
    // makes the anchor a spring - every dodge displaces further than any tolerance, so every dodge
    // buys a return trip and the two actions trade the bot for the rest of the icicle's life.

    // In among the melee, where an icicle or a Freeze on either catches both.
    Unit* hodir = GetHodir(botAI);
    if (hodir && bot->GetExactDist2d(hodir) < ULDUAR_HODIR_RANGED_MIN_BOSS_GAP)
        return true;

    // Or out of range of him entirely. Nothing walks a bot home any more, so without this one that
    // dodged its way to the far wall stands there shooting nothing.
    if (hodir && bot->GetExactDist2d(hodir) > ULDUAR_HODIR_CASTER_MAX_BOSS_GAP)
        return true;

    // Nothing here tests Starlight or a fire. Both reach a few yards, so a bot is outside every zone
    // almost all of the time and a constraint on that can never be satisfied - it just walks.
    // GetHodirAnchor is what offers one, and only when it is worth the walk.

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

    // A tank leaving its spot mid-Frozen-Blows costs more than the buff is worth.
    if (botAI->IsTank(bot))
        return false;

    return bot->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_HODIR_STORM_CLOUD, bot));
}

bool HodirCollectStormPowerTrigger::IsActive()
{
    if (!GetHodir(botAI))
        return false;

    // Tanks never leave the boss for a damage buff, and healers are not worth a charge: they took
    // 22.6% of everything the carries handed out in one pull for 0.2% of the raid's damage.
    if (botAI->IsTank(bot) || botAI->IsHeal(bot))
        return false;

    // Already holding it - the pulse would be spent on a bot that cannot use it, and the walk is
    // downtime for nothing.
    if (bot->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_HODIR_STORM_POWER, bot)))
        return false;

    Player* carrier = GetHodirStormCloudCarrier(botAI, bot);
    if (!carrier || carrier == bot)
        return false;

    Position rally;
    if (!GetHodirStormCloudRally(botAI, bot, carrier, rally))
        return false;

    float const gap = bot->GetExactDist2d(&rally);
    if (gap > ULDUAR_HODIR_STORM_CLOUD_COLLECT_LEASH)
        return false;

    // Wait out an icicle between the bot and the rally rather than walk back through it: 9-45 collects
    // a pull went straight back into a pool the dodge had just stepped the bot out of.
    if (IsHodirWalkThroughLiveIcicle(bot, rally, ULDUAR_HODIR_ICE_SHARDS_RADIUS + ULDUAR_HODIR_DODGE_TRIGGER_MARGIN,
                                     ULDUAR_HODIR_BIG_SHARDS_RADIUS + ULDUAR_HODIR_DODGE_TRIGGER_MARGIN))
        return false;

    // Release rather than the park distance the action aims for. Testing one number at both ends
    // stands this down the tick the bot arrives and hands the next tick to the position anchor, which
    // walks it straight back out - the same trap the shelter run's park/release pair exists for.
    return gap > ULDUAR_HODIR_STORM_CLOUD_COLLECT_RELEASE;
}
