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

    // Boss context first: the name lookup below copies a std::string, and this runs for every action
    // of every bot on every tick. Anything that skips the dwell check has to clear the state too, or
    // the previous boss's timer satisfies the dwell instantly on the next pull.
    Unit* target = AI_VALUE(Unit*, "current target");
    Creature* creature = target ? target->ToCreature() : nullptr;
    if (!creature || !(creature->IsDungeonBoss() || creature->isWorldBoss()) || !bot->GetGroup() ||
        botAI->IsMainTank(bot))
    {
        holdState.Reset();
        return 1.0f;
    }

    std::string const name = action->getName();
    if (!IsBurstCooldownAction(name))
        return 1.0f;

    // "use trinket" is the generic trinket action, so for anyone but a dps it also covers survival
    // and mana trinkets - those have to stay available through the pull.
    if (name == "use trinket" && !PlayerbotAI::IsDps(bot))
        return 1.0f;

    uint32 dwellMs = (name == "bloodlust" || name == "heroism") ? LUST_DWELL_MS : BURST_DWELL_MS;

    return MainTankHasHeldBoss(bot, target, holdState, dwellMs) ? 1.0f : 0.0f;
}

void BurstWindowStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new HoldBurstUntilTankEngagedMultiplier(botAI));
}
