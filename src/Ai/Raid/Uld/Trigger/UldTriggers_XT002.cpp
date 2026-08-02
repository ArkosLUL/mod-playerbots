#include "UldTriggers_XT002.h"

#include "Group.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidBossHelpers.h"
#include "RangeTriggers.h"
#include "RtiTargetValue.h"
#include "UldBossHelper.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "Unit.h"

//
// XT-002 Deconstructor
//

// TooCloseToPlayerWithDebuffTrigger counts the bot itself, which would make every carrier think it
// has to run from its own debuff, so the group is walked here with the bot skipped.
static bool HasDebuffedAllyInRange(Player* bot, uint32 spellId, float range)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || member == bot || !member->IsAlive())
            continue;

        if (!member->HasAura(spellId))
            continue;

        if (member->GetExactDist2d(bot) < range)
            return true;
    }

    return false;
}

// True when the skull is on one of XT's adds, so the generic "attack rti target" is safe to run.
static bool IsSkullOnXT002Add(PlayerbotAI* botAI, Player* bot)
{
    Group* group = bot->GetGroup();
    if (!group)
        return false;

    Unit* marked = botAI->GetUnit(group->GetTargetIcon(RtiTargetValue::skullIndex));
    if (!marked || !marked->IsAlive())
        return false;

    uint32 const entry = marked->GetEntry();
    return entry == PB_NPC_XT002_LIFE_SPARK || entry == NPC_XS013_SCRAPBOT || entry == PB_NPC_XT002_BOOMBOT ||
           entry == PB_NPC_XT002_PUMMELLER;
}

bool XT002SearingLightSpreadTrigger::IsActive()
{
    if (!GetXT002(botAI))
        return false;

    return HasDebuffedAllyInRange(bot, GetXT002SearingLightSpellId(bot), ULDUAR_XT002_DEBUFF_SPREAD_RADIUS);
}

bool XT002GravityBombSpreadTrigger::IsActive()
{
    if (!GetXT002(botAI))
        return false;

    return HasDebuffedAllyInRange(bot, GetXT002GravityBombSpellId(bot), ULDUAR_XT002_DEBUFF_SPREAD_RADIUS);
}

bool XT002GravityBombCarrierTrigger::IsActive()
{
    if (!GetXT002(botAI))
        return false;

    if (!bot->HasAura(GetXT002GravityBombSpellId(bot)))
        return false;

    return GetNearestPlayerInRadius(bot, ULDUAR_XT002_DEBUFF_SPREAD_RADIUS) != nullptr;
}

bool XT002BoombotAvoidTrigger::IsActive()
{
    if (!GetXT002(botAI))
        return false;

    // Ranged already stand outside the blast; pulling them out too would only break their casts.
    if (!botAI->IsMelee(bot))
        return false;

    TooCloseToCreatureTrigger tooCloseToBoombot(botAI);
    return tooCloseToBoombot.TooCloseToCreature(PB_NPC_XT002_BOOMBOT, ULDUAR_XT002_BOOMBOT_AVOID_RADIUS);
}

bool XT002VoidZoneTrigger::IsActive()
{
    if (!GetXT002(botAI))
        return false;

    TooCloseToCreatureTrigger tooCloseToVoidZone(botAI);
    return tooCloseToVoidZone.TooCloseToCreature(PB_NPC_XT002_VOID_ZONE, ULDUAR_XT002_VOID_ZONE_RADIUS);
}

bool XT002MarkKillTargetTrigger::IsActive()
{
    if (!GetXT002(botAI))
        return false;

    if (!IsMechanicTrackerBot(botAI, bot, ULDUAR_MAP_ID))
        return false;

    Unit* killTarget = GetXT002KillTarget(botAI);
    if (!killTarget)
        return false;

    Group* group = bot->GetGroup();
    if (group && group->GetTargetIcon(RtiTargetValue::skullIndex) == killTarget->GetGUID())
        return false;

    return true;
}

bool XT002AttackKillTargetTrigger::IsActive()
{
    if (!GetXT002(botAI))
        return false;

    // Melee stay off Boombots entirely - the avoid trigger outranks this, but a Boombot must never
    // become their attack target in the first place.
    Group* group = bot->GetGroup();
    if (group && botAI->IsMelee(bot))
    {
        Unit* marked = botAI->GetUnit(group->GetTargetIcon(RtiTargetValue::skullIndex));
        if (marked && marked->GetEntry() == PB_NPC_XT002_BOOMBOT)
            return false;
    }

    return IsSkullOnXT002Add(botAI, bot);
}

bool XT002BoombotRangedKillTrigger::IsActive()
{
    if (!GetXT002(botAI))
        return false;

    if (!botAI->IsRanged(bot))
        return false;

    Unit* boombot = GetFirstAliveUnitByEntry(botAI, PB_NPC_XT002_BOOMBOT);
    if (!boombot)
        return false;

    // Only worth taking on from outside the blast radius; closer than that the bot should be moving.
    return boombot->GetExactDist2d(bot) >= ULDUAR_XT002_BOOMBOT_AVOID_RADIUS;
}

bool XT002AttackHeartTrigger::IsActive()
{
    if (!GetXT002(botAI))
        return false;

    Unit* heart = GetXT002ExposedHeart(botAI);
    if (!heart)
        return false;

    // Hitting the Heart is what spawns the adds - Exposed Heart fires an orb at the Toy Piles on
    // every hit - so waiting for a clear field would mean never touching it again after the first
    // tick. Only a Life Spark is worth breaking off for, since Static Charged chains through the raid.
    if (GetFirstAliveUnitByEntry(botAI, PB_NPC_XT002_LIFE_SPARK))
        return false;

    if (IsXT002HardModeActive(botAI))
        return true;

    return heart->GetHealthPct() > ULDUAR_XT002_HEART_SAFE_HP_PCT;
}

bool XT002PummellerTauntTrigger::IsActive()
{
    if (!GetXT002(botAI))
        return false;

    if (!botAI->IsTank(bot))
        return false;

    // Whoever holds XT keeps holding him: the Pummeller belongs to the first assist tank, and only
    // falls to the main tank when the raid has no second tank left.
    if (Player* assistTank = GetGroupAssistTank(botAI, bot, 0))
    {
        if (assistTank != bot)
            return false;
    }
    else if (Player* mainTank = GetGroupMainTank(botAI, bot))
    {
        if (mainTank != bot)
            return false;
    }

    Unit* pummeller = GetFirstAliveUnitByEntry(botAI, PB_NPC_XT002_PUMMELLER);
    if (!pummeller)
        return false;

    return botAI->GetPlayer(pummeller->GetTarget()) != bot;
}

bool XT002RedirectThreatTrigger::IsActive()
{
    if (bot->getClass() != CLASS_HUNTER && bot->getClass() != CLASS_ROGUE)
        return false;

    Unit* xt002 = GetXT002(botAI);
    if (!xt002 || !xt002->IsAlive())
        return false;

    // Submerged XT is REACT_PASSIVE and off everyone's threat list, so a redirect fired there would
    // burn its charges on nothing.
    return !IsXT002Submerged(botAI);
}
