/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "BossResistanceMultipliers.h"

#include "Action.h"
#include "GenericBuffUtils.h"
#include "Player.h"
#include "Playerbots.h"
#include "Unit.h"

#include <set>

float BossNatureAspectHoldMultiplier::GetValue(Action* action)
{
    if (!action || bot->getClass() != CLASS_HUNTER)
        return 1.0f;

    // Every aspect, not just Dragonhawk: a vetoed node lands on the IMPOSSIBLE branch, which still
    // pushes the node's alternatives, and BuffHunterStrategyActionNodeFactory chains
    // dragonhawk -> hawk -> monkey. Monkey has no alternative, so the chain terminates here.
    static std::set<std::string> const competingAspects = {
        "aspect of the viper", "aspect of the dragonhawk", "aspect of the hawk",
        "aspect of the monkey", "aspect of the cheetah", "aspect of the pack"};

    // Name first: this runs for every action in the queue, and the hunter lookup walks the group.
    if (!competingAspects.count(action->getName()))
        return 1.0f;

    // Must stay identical to BossNatureResistanceTrigger's gate. If this stops vetoing while the
    // trigger still fires, the Wild cast at ACTION_RAID and the bdps Dragonhawk node at ACTION_HIGH
    // trade the slot every tick.
    Unit* boss = ai::buff::FindBossByName(botAI, bossName);
    if (!boss || !boss->IsAlive() || boss->IsFriendlyTo(bot))
        return 1.0f;

    return ai::buff::GetNatureResistanceHunter(botAI, bot) == bot ? 0.0f : 1.0f;
}
