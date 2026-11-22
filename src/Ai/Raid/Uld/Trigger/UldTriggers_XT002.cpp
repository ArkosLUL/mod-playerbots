#include "UldTriggers_XT002.h"

#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "PlayerbotAIConfig.h"
#include "UldEncounter_XT002.h"
#include "UldHardMode.h"
#include "UldScripts.h"
#include "Unit.h"

namespace
{
bool InsideXT002Hazard(PlayerbotAI* botAI, Player* bot)
{
    GuidVector const& npcs = botAI->GetAiObjectContext()->GetValue<GuidVector>("nearest npcs")->Get();
    for (ObjectGuid const& guid : npcs)
    {
        Unit* unit = botAI->GetUnit(guid);
        if (!unit || !unit->IsAlive())
            continue;

        if (unit->GetEntry() == PB_NPC_XT002_VOID_ZONE &&
            unit->GetExactDist2d(bot) < ULDUAR_XT002_VOID_ZONE_RADIUS)
            return true;

        if (unit->GetEntry() == PB_NPC_XT002_BOOMBOT && PlayerbotAI::IsMelee(bot) &&
            unit->GetExactDist2d(bot) < ULDUAR_XT002_BOOMBOT_AVOID_RADIUS)
            return true;
    }

    return false;
}
}

bool XT002DebuffCarrierTrigger::IsActive()
{
    if (!bot->HasAura(GetXT002GravityBombSpellId(bot)) && !bot->HasAura(GetXT002SearingLightSpellId(bot)))
        return false;

    return GetXT002(botAI) != nullptr;
}

bool XT002AvoidHazardTrigger::IsActive()
{
    bool hazardNear = false;

    if (PlayerbotAI::IsMelee(bot) && bot->FindNearestCreature(PB_NPC_XT002_BOOMBOT, ULDUAR_XT002_BOOMBOT_AVOID_RADIUS))
        hazardNear = true;
    else if (!bot->HasAura(GetXT002SearingLightSpellId(bot)) && !bot->HasAura(GetXT002GravityBombSpellId(bot)) &&
             bot->FindNearestCreature(PB_NPC_XT002_VOID_ZONE, ULDUAR_XT002_VOID_ZONE_RADIUS))
        hazardNear = true;

    return hazardNear && GetXT002(botAI) != nullptr;
}

bool XT002RaidPositionTrigger::IsActive()
{
    bool const tank = PlayerbotAI::IsTank(bot);
    if (!tank && !PlayerbotAI::IsRangedDps(bot) && !PlayerbotAI::IsHeal(bot))
        return false;

    Unit* xt002 = GetXT002(botAI);

    if (!xt002 || !xt002->IsInCombat())
        return false;

    if (tank)
    {
        Unit* victim = xt002->GetVictim();
        if (victim ? victim != bot : GetXT002TankRoles(bot, xt002).holder != bot)
            return false;
    }

    if (bot->HasAura(GetXT002SearingLightSpellId(bot)) || bot->HasAura(GetXT002GravityBombSpellId(bot)))
        return false;

    XT002AvoidHazardTrigger avoidHazard(botAI);
    if (avoidHazard.IsActive() && InsideXT002Hazard(botAI, bot))
        return false;

    if (tank)
        return bot->GetExactDist(ULDUAR_XT002_MAINTANK_SPOT) > ULDUAR_XT002_MAINTANK_SPOT_TOLERANCE;

    if (PlayerbotAI::IsHeal(bot) && GetXT002BombHealLatch(bot).sent)
        return false;

    Position slot;
    if (!GetXT002RangedSlot(bot, xt002, slot))
        return false;

    return bot->GetExactDist(slot) > ULDUAR_XT002_RANGED_SPOT_TOLERANCE;
}

bool XT002BombHealTrigger::IsActive()
{
    if (!PlayerbotAI::IsHeal(bot))
        return false;

    XT002BombHealLatch const latch = GetXT002BombHealLatch(bot);
    return latch.carrier && latch.sent && bot->GetExactDist2d(latch.carrier) > sPlayerbotAIConfig.HealDistance;
}

bool XT002BombCarrierHealTrigger::IsActive()
{
    return PlayerbotAI::IsHeal(bot) && GetXT002BombHealTarget(bot) != nullptr;
}

bool XT002SetDpsPriorityTrigger::IsActive()
{
    Unit* xt002 = GetXT002(botAI);

    return xt002 && xt002->IsInCombat();
}

bool XT002PummellerTauntTrigger::IsActive()
{
    if (!PlayerbotAI::IsTank(bot))
        return false;

    Unit* xt002 = GetXT002(botAI);
    if (!xt002)
        return false;

    if (!IsXT002PummellerTank(bot, xt002))
        return false;

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

    return !IsXT002Submerged(xt002);
}

bool XT002HolderBuffTrigger::IsActive()
{
    if (bot->getClass() != classId || !PlayerbotAI::IsHeal(bot))
        return false;

    if (strategy && !botAI->HasStrategy(strategy, BOT_STATE_COMBAT))
        return false;

    return GetXT002BuffTarget(botAI, spell) != nullptr;
}
