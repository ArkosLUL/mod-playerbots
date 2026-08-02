/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "BurstWindowStrategy.h"

#include "BurstCooldowns.h"
#include "Playerbots.h"

float HoldBurstUntilTankEngagedMultiplier::GetValue(Action* action)
{
    if (!action)
        return 1.0f;

    // This multiplier only governs burst throughput cooldowns; leave everything else (rotation
    // spells, tank mitigation, healer mana) untouched. The name lookup copies a std::string and
    // runs for every action of every bot on every tick, so bail here before any target work.
    std::string const name = action->getName();
    if (!IsBurstCooldownAction(name))
        return 1.0f;

    // "use trinket" is the generic trinket action, so for anyone but a dps it also covers survival
    // and mana trinkets - those have to stay available through the pull.
    if (name == "use trinket" && !PlayerbotAI::IsDps(bot))
        return 1.0f;

    // For a healer these are throughput cooldowns for the raid damage window, not the pull:
    // Avenging Wrath is +20% healing, Power Infusion +20% haste on whoever gets it.
    if ((name == "avenging wrath" || name == "power infusion") && PlayerbotAI::IsHeal(bot))
        return 1.0f;

    // Anything that skips the dwell check has to clear the state too, or the previous boss's timer
    // satisfies the dwell instantly on the next pull.
    Unit* target = AI_VALUE(Unit*, "current target");
    Creature* creature = target ? target->ToCreature() : nullptr;
    bool const isBoss = creature && (creature->IsDungeonBoss() || creature->isWorldBoss());

    if (!isBoss)
    {
        holdState.Reset();

        // With the config off, when soloing, or for shadowfiend (a mana return that just happens to
        // be a burst cooldown), keep firing on whatever is being fought. Otherwise a grouped bot
        // saves the cooldown for the boss instead of blowing it on trash.
        if (!sPlayerbotAIConfig.burstOnBossOnly || !bot->GetGroup() || name == "shadowfiend")
            return 1.0f;

        return 0.0f;
    }

    if (!bot->GetGroup() || botAI->IsMainTank(bot))
    {
        holdState.Reset();
        return 1.0f;
    }

    uint32 dwellMs = (name == "bloodlust" || name == "heroism") ? LUST_DWELL_MS : BURST_DWELL_MS;

    return MainTankHasHeldBoss(bot, target, holdState, dwellMs) ? 1.0f : 0.0f;
}

void BurstWindowStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new HoldBurstUntilTankEngagedMultiplier(botAI));
}
