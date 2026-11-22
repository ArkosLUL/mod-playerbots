#include "UldTriggers_Hodir.h"

#include "Object.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidObs.h"
#include "SpellMgr.h"
#include "UldEncounter_Hodir.h"
#include "UldScripts.h"
#include "EncounterHelpers.h"
#include "ScriptedCreature.h"
#include "SharedDefines.h"
#include "Trigger.h"

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

    if (bot->HasAura(SPELL_HODIR_TOASTY_FIRE_AURA))
        return false;

    return bot->HasAura(SPELL_BITING_COLD_PLAYER_AURA);
}

bool HodirNearSnowpackedIcicleTrigger::IsActive()
{
    if (!GetHodir(botAI))
        return false;

    if (!IsHodirFlashFreezeIncoming(botAI))
        return false;

    Creature* shelter = GetHodirShelter(botAI, bot);
    if (!shelter)
        return false;

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

    if (PlayerbotAI::IsTank(bot))
        return false;

    if (NearestHodirIcicle(bot, NPC_HODIR_ICICLE_SMALL,
                           ULDUAR_HODIR_ICE_SHARDS_RADIUS + ULDUAR_HODIR_DODGE_TRIGGER_MARGIN))
        return true;

    Creature* drift = NearestHodirIcicle(bot, NPC_HODIR_ICICLE_DRIFT,
                                         ULDUAR_HODIR_BIG_SHARDS_RADIUS + ULDUAR_HODIR_DODGE_TRIGGER_MARGIN);
    if (!drift)
        return false;

    return drift->FindNearestCreature(NPC_SNOWPACKED_ICICLE, ULDUAR_HODIR_SAFE_AREA_RADIUS) == nullptr;
}

bool HodirRaidPositionTrigger::IsActive()
{
    if (!IsHodirEngaged(botAI))
        return false;

    if (bot->HasAura(SPELL_HODIR_FLASH_FREEZE_TRAPPED))
        return false;

    if (PlayerbotAI::IsRanged(bot) && !PlayerbotAI::IsHeal(bot))
    {
        Unit* target = AI_VALUE(Unit*, "current target");
        if (target && target->IsAlive() &&
            (target->GetEntry() == NPC_HODIR_FLASH_FREEZE_BLOCK || target->GetEntry() == NPC_HODIR_FLASH_FREEZE_PLAYER))
        {
            if (RaidObs::Active())
                RaidObs::NoteDerived(bot, "hodir.stand", "block");

            return false;
        }
    }

    HodirNearSnowpackedIcicleTrigger shelter(botAI);
    if (shelter.IsActive())
        return false;

    HodirIcicleDodgeTrigger dodge(botAI);
    if (dodge.IsActive())
        return false;

    if (IsHodirBitingColdShedArmed(botAI, bot))
        return false;

    HodirSpreadStormCloudTrigger spread(botAI);
    if (spread.IsActive())
        return false;

    HodirStormGather gather;
    if (GetHodirStormGather(botAI, bot, gather) && gather.dist <= ULDUAR_HODIR_STORM_CLOUD_COLLECT_RELEASE)
        return false;

    Position anchor;
    float tolerance = 0.0f;
    if (GetHodirAnchor(botAI, bot, anchor, tolerance))
    {
        if (IsHodirWalkThroughLiveIcicle(bot, anchor, ULDUAR_HODIR_ICE_SHARDS_CLEAR,
                                         ULDUAR_HODIR_BIG_SHARDS_CLEAR))
            return false;

        return bot->GetExactDist2d(&anchor) > tolerance;
    }

    if (!PlayerbotAI::IsRanged(bot))
        return false;

    Unit* hodir = GetHodir(botAI);
    if (hodir && bot->GetExactDist2d(hodir) < ULDUAR_HODIR_RANGED_MIN_BOSS_GAP)
        return true;

    if (hodir && bot->GetExactDist2d(hodir) > ULDUAR_HODIR_CASTER_MAX_BOSS_GAP)
        return true;

    for (ObjectGuid const& guid : AI_VALUE(GuidVector, "nearest friendly players"))
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
    if (!IsHodirEngaged(botAI))
        return false;

    return !PlayerbotAI::IsHeal(bot);
}

bool HodirFrozenBlowsSwapTrigger::IsActive()
{
    Unit* boss = GetHodir(botAI);
    if (!boss)
        return false;

    bool const frozenBlows = HodirFrozenBlowsActive(botAI, bot);
    bool const holding = boss->GetVictim() == bot;

    if (PlayerbotAI::IsAssistTankOfIndex(bot, 0, true))
        return frozenBlows && !holding && bot->GetHealthPct() >= ULDUAR_HODIR_TAUNT_HEALTH_FLOOR;

    if (PlayerbotAI::IsMainTank(bot))
        return !frozenBlows && !holding;

    return false;
}

bool HodirTankDefensiveTrigger::IsActive() { return GetHodirTankDefensive(botAI, bot, "hodir.defensive") != 0; }

bool HodirRedirectThreatTrigger::IsActive()
{
    if (!IsHodirEngaged(botAI))
        return false;

    if (bot->getClass() != CLASS_HUNTER && bot->getClass() != CLASS_ROGUE)
        return false;

    return !IsHodirBitingColdShedArmed(botAI, bot);
}

bool HodirSpreadStormCloudTrigger::IsActive()
{
    if (!GetHodir(botAI))
        return false;

    if (PlayerbotAI::IsTank(bot))
        return false;

    return bot->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_HODIR_STORM_CLOUD, bot));
}

bool HodirCollectStormPowerTrigger::IsActive()
{
    HodirStormGather gather;
    bool const receiver = GetHodirStormGather(botAI, bot, gather);
    bool const in = receiver && gather.dist <= ULDUAR_HODIR_STORM_CLOUD_COLLECT_RELEASE;

    if (RaidObs::Active())
        RaidObs::NoteDerived(bot, "hodir.gather", !receiver ? "none" : in ? "in" : gather.msToWalk > 0 ? "wait" : "walk");

    if (!receiver || in || gather.msToWalk > 0)
        return false;

    return !IsHodirWalkThroughLiveIcicle(bot, gather.rally,
                                         ULDUAR_HODIR_ICE_SHARDS_RADIUS + ULDUAR_HODIR_DODGE_TRIGGER_MARGIN,
                                         ULDUAR_HODIR_BIG_SHARDS_RADIUS + ULDUAR_HODIR_DODGE_TRIGGER_MARGIN);
}
