/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "ICCTriggers_Valithria.h"
#include "ICCHelpers_Shared.h"
#include "ICCHelpers_Valithria.h"
#include "Playerbots.h"
#include "Trigger.h"

bool IccValithriaGroupTrigger::IsActive()
{
    return bot->FindNearestCreature(NPC_VALITHRIA_DREAMWALKER, 100.0f) != nullptr;
}

bool IccValithriaZombieKiteTrigger::IsActive()
{
    Unit* boss = bot->FindNearestCreature(NPC_VALITHRIA_DREAMWALKER, 100.0f);
    if (!boss)
        return false;

    if (PlayerbotAI::IsTank(bot))
        return false;

    std::list<Creature*> zombies;
    bot->GetCreatureListWithEntryInGrid(zombies, NPC_BLISTERING_ZOMBIE, 100.0f);
    for (Creature* z : zombies)
    {
        if (z && z->IsAlive() && z->GetVictim() == bot)
            return true;
    }

    return false;
}

bool IccValithriaPortalTrigger::IsActive()
{
    Unit* boss = bot->FindNearestCreature(NPC_VALITHRIA_DREAMWALKER, 100.0f);
    if (!boss)
        return false;

    // Only healers should use portals
    if (!PlayerbotAI::IsHeal(bot) || bot->HasAura(SPELL_DREAM_STATE))
        return false;

    // Read ahead of the checks below so every healer publishes the wave, the raid healers too.
    bool const portalUp = !IccValithriaPortals(bot).empty();

    Aura* aura = botAI->GetAura("Twisted Nightmares", bot, false, true);
    if (aura && aura->GetStackAmount() >= 25)
        return false;

    Creature* worm = bot->FindNearestCreature(NPC_ROT_WORM, 100.0f);
    Creature* zombie = bot->FindNearestCreature(NPC_BLISTERING_ZOMBIE, 100.0f);

    if ((worm && worm->GetVictim() == bot) || (zombie && zombie->GetVictim() == bot))
        return false;

    auto const shouldHealRaid = IccValithriaShouldHealRaid(bot);
    if (!shouldHealRaid.has_value() || *shouldHealRaid)
        return false;

    return portalUp;
}

bool IccValithriaHealTrigger::IsActive()
{
    Unit* boss = bot->FindNearestCreature(NPC_VALITHRIA_DREAMWALKER, 100.0f);
    if (!boss)
        return false;

    // Only healers should use healing
    if (!PlayerbotAI::IsHeal(bot) || bot->HasAura(SPELL_DREAM_STATE) || bot->HealthBelowPct(50))
        return false;

    Creature* worm = bot->FindNearestCreature(NPC_ROT_WORM, 100.0f);
    Creature* zombie = bot->FindNearestCreature(NPC_BLISTERING_ZOMBIE, 100.0f);

    if ((worm && worm->GetVictim() == bot) || (zombie && zombie->GetVictim() == bot))
        return false;

    auto const shouldHealRaid = IccValithriaShouldHealRaid(bot);
    if (!shouldHealRaid.has_value() || *shouldHealRaid)
        return false;

    if (bot->GetHealthPct() < 50.0f)
        return false;

    // For Valithria healers, check portal logic
    // If no portal is found within 100 yards, we should heal
    if (!bot->FindNearestCreature(NPC_DREAM_PORTAL, 100.0f) && !bot->FindNearestCreature(NPC_NIGHTMARE_PORTAL, 100.0f))
        return true;

    if (bot->FindNearestCreature(NPC_DREAM_PORTAL, 10.0f) || bot->FindNearestCreature(NPC_NIGHTMARE_PORTAL, 10.0f))
        return false;

    // If portal is far but within 100 yards, heal while moving to it
    return true;
}

bool IccValithriaDreamCloudTrigger::IsActive()
{
    if (!bot->HasAura(SPELL_DREAM_STATE) || bot->HealthBelowPct(50))
        return false;

    return true;
}
