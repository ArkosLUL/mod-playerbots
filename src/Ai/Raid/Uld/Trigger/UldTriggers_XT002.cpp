#include "UldTriggers_XT002.h"

#include "Group.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "RaidBossHelpers.h"
#include "RangeTriggers.h"
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

bool XT002SearingLightCarrierTrigger::IsActive()
{
    if (!GetXT002(botAI))
        return false;

    return bot->HasAura(GetXT002SearingLightSpellId(bot));
}

bool XT002RaidPositionTrigger::IsActive()
{
    if (!GetXT002(botAI))
        return false;

    // Anything the bot has to dodge outranks standing on a spot, and the carriers have destinations of
    // their own, so the anchor stands down rather than fighting them for the tick.
    if (bot->HasAura(GetXT002SearingLightSpellId(bot)) || bot->HasAura(GetXT002GravityBombSpellId(bot)))
        return false;

    XT002BoombotAvoidTrigger boombotAvoid(botAI);
    XT002VoidZoneTrigger voidZone(botAI);
    if (boombotAvoid.IsActive() || voidZone.IsActive())
        return false;

    if (botAI->IsMainTank(bot))
        return bot->GetExactDist(ULDUAR_XT002_MAINTANK_SPOT) > ULDUAR_XT002_MAINTANK_SPOT_TOLERANCE;

    if (botAI->IsRangedDps(bot))
        return bot->GetExactDist(ULDUAR_XT002_RANGED_SPOT) > ULDUAR_XT002_RANGED_SPOT_TOLERANCE;

    return false;
}

bool XT002SetDpsPriorityTrigger::IsActive()
{
    if (!GetXT002(botAI))
        return false;

    // Tanks are driven by the taunt action and the generic tank assist; this only owns DPS targeting.
    return !botAI->IsTank(bot);
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
