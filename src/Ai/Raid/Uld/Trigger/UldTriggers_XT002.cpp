#include "UldTriggers_XT002.h"

#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "RangeTriggers.h"
#include "UldBossHelper.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "Unit.h"

//
// XT-002 Deconstructor
//

bool XT002DebuffCarrierTrigger::IsActive()
{
    if (!GetXT002(botAI))
        return false;

    // No proximity check: nobody else is going to step aside, so the carrier leaves whether or not
    // someone happens to be standing next to it right now.
    return bot->HasAura(GetXT002GravityBombSpellId(bot)) || bot->HasAura(GetXT002SearingLightSpellId(bot));
}

bool XT002AvoidHazardTrigger::IsActive()
{
    if (!GetXT002(botAI))
        return false;

    // Ranged already stand outside the blast; pulling them out too would only break their casts.
    if (botAI->IsMelee(bot))
    {
        TooCloseToCreatureTrigger tooCloseToBoombot(botAI);
        if (tooCloseToBoombot.TooCloseToCreature(PB_NPC_XT002_BOOMBOT, ULDUAR_XT002_BOOMBOT_AVOID_RADIUS))
            return true;
    }

    // Has to match the action's own gate, or this fires for a case the action declines and the tick is
    // wasted. A carrier's puddles belong to "xt002 debuff carrier action", which is also what walks one
    // off its own bomb.
    if (bot->HasAura(GetXT002SearingLightSpellId(bot)) || bot->HasAura(GetXT002GravityBombSpellId(bot)))
        return false;

    TooCloseToCreatureTrigger tooCloseToVoidZone(botAI);
    return tooCloseToVoidZone.TooCloseToCreature(PB_NPC_XT002_VOID_ZONE, ULDUAR_XT002_VOID_ZONE_RADIUS);
}

bool XT002RaidPositionTrigger::IsActive()
{
    Unit* xt002 = GetXT002(botAI);

    // Combat-gated, unlike the rest of the encounter's triggers: an anchor that fires on sight has
    // the raid pre-positioning before anyone pulls.
    if (!xt002 || !xt002->IsInCombat())
        return false;

    // Anything the bot has to dodge outranks standing on a spot, and the carriers have destinations of
    // their own, so the anchor stands down rather than fighting them for the tick.
    if (bot->HasAura(GetXT002SearingLightSpellId(bot)) || bot->HasAura(GetXT002GravityBombSpellId(bot)))
        return false;

    XT002AvoidHazardTrigger avoidHazard(botAI);
    if (avoidHazard.IsActive())
        return false;

    if (botAI->IsMainTank(bot))
    {
        // Only while he is actually holding XT. The anchor outranks "reach melee", so a tank that has
        // lost aggro would walk to the spot and stand there out of range with no way back - XT is a
        // vehicle, and Vehicle::ApplyAllImmunities makes every one of them taunt-immune, so threat from
        // damage is the only route. No victim at all is the Heart window, where the spot is right.
        Unit* victim = xt002->GetVictim();
        if (victim && victim != bot)
            return false;

        return bot->GetExactDist(ULDUAR_XT002_MAINTANK_SPOT) > ULDUAR_XT002_MAINTANK_SPOT_TOLERANCE;
    }

    if (botAI->IsRangedDps(bot))
        return bot->GetExactDist(ULDUAR_XT002_RANGED_SPOT) > ULDUAR_XT002_RANGED_SPOT_TOLERANCE;

    if (botAI->IsHeal(bot))
    {
        // Stands down while anything is out of heal range. This node outranks "reach party member to
        // heal", so without the check a healer could never close on a carrier parked out in the lot -
        // the far cells sit 55 yd from the anchor against 40 yd of heal range.
        PartyMemberToHealOutOfSpellRangeTrigger outOfHealRange(botAI);
        if (outOfHealRange.IsActive())
            return false;

        // Healers share the ranged anchor but with a wide band, so heal range and the generic
        // disperse still choose the spot inside it.
        return bot->GetExactDist(ULDUAR_XT002_RANGED_SPOT) > ULDUAR_XT002_HEALER_SPOT_TOLERANCE;
    }

    return false;
}

bool XT002SetDpsPriorityTrigger::IsActive()
{
    Unit* xt002 = GetXT002(botAI);

    // The action calls Attack() directly, so without the combat gate the first bot to see XT pulls
    // him. Whoever pulls flips this for everyone, which is what makes the raid engage together.
    return xt002 && xt002->IsInCombat();
}

bool XT002PummellerTauntTrigger::IsActive()
{
    if (!GetXT002(botAI))
        return false;

    if (!IsXT002PummellerTank(botAI, bot))
        return false;

    // Taunt range, not the leash: this node sits at ACTION_RAID + 4, so firing it against an add the
    // cast cannot reach would burn the tick while a nearer one goes untaunted.
    Unit* pummeller = GetXT002EngageableAdd(botAI, bot, PB_NPC_XT002_PUMMELLER, ULDUAR_XT002_TAUNT_RANGE);
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
