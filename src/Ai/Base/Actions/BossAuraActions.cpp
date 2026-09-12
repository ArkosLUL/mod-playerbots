/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "BossAuraActions.h"
#include "BossAuraTriggers.h"
#include "Group.h"
#include "PaladinBuffStrategies.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"

const std::string ADD_STRATEGY_CHAR = "+";

bool BossFireResistanceAction::isUseful()
{
    BossFireResistanceTrigger bossFireResistanceTrigger(botAI, bossName);
    return bossFireResistanceTrigger.IsActive();
}

bool BossFireResistanceAction::Execute(Event /*event*/)
{
    PaladinFireResistanceStrategy paladinFireResistanceStrategy(botAI);
    botAI->ChangeStrategy(ADD_STRATEGY_CHAR + paladinFireResistanceStrategy.getName(), BotState::BOT_STATE_COMBAT);
    botAI->DoSpecificAction("fire resistance aura", Event(), true);
    return true;
}

bool BossFrostResistanceAction::isUseful()
{
    BossFrostResistanceTrigger bossFrostResistanceTrigger(botAI, bossName);
    return bossFrostResistanceTrigger.IsActive();
}

bool BossFrostResistanceAction::Execute(Event /*event*/)
{
    PaladinFrostResistanceStrategy paladinFrostResistanceStrategy(botAI);
    botAI->ChangeStrategy(ADD_STRATEGY_CHAR + paladinFrostResistanceStrategy.getName(), BotState::BOT_STATE_COMBAT);
    botAI->DoSpecificAction("frost resistance aura", Event(), true);
    return true;
}

bool BossNatureResistanceAction::isUseful()
{
    BossNatureResistanceTrigger bossNatureResistanceTrigger(botAI, bossName);
    return bossNatureResistanceTrigger.IsActive();
}

bool BossNatureResistanceAction::Execute(Event /*event*/)
{
    // No ChangeStrategy: "+rnature" was never removed again, and because it is a sibling of "bdps" it
    // also evicted the hunter's Dragonhawk node. Worse, the strategy is what made
    // HunterAspectOfTheViperTrigger return false, so every hunter that ever held the aura lost Aspect
    // of the Viper for the rest of the session. BossNatureAspectHoldMultiplier does the suppression
    // instead, and reverts on its own when the boss dies.
    return botAI->DoSpecificAction("aspect of the wild", Event(), true);
}

bool BossShadowResistanceAction::isUseful()
{
    BossShadowResistanceTrigger bossShadowResistanceTrigger(botAI, bossName);
    return bossShadowResistanceTrigger.IsActive();
}

bool BossShadowResistanceAction::Execute(Event /*event*/)
{
    PaladinShadowResistanceStrategy paladinShadowResistanceStrategy(botAI);
    botAI->ChangeStrategy(ADD_STRATEGY_CHAR + paladinShadowResistanceStrategy.getName(), BotState::BOT_STATE_COMBAT);
    botAI->DoSpecificAction("shadow resistance aura", Event(), true);
    return true;
}

bool BossMarkSkullAction::isUseful()
{
    BossMarkSkullTrigger bossMarkSkullTrigger(botAI, bossName);
    return bossMarkSkullTrigger.IsActive();
}

bool BossMarkSkullAction::Execute(Event /*event*/)
{
    Unit* boss = AI_VALUE2(Unit*, "find target", bossName);
    if (!boss || !boss->IsAlive())
        return false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    // When the main tank is a real player it won't mark itself, so the first bot assist tank
    // (of the first three) takes over; otherwise the main-tank bot marks the boss directly.
    Unit* mainTankUnit = AI_VALUE(Unit*, "main tank");
    Player* mainTank = mainTankUnit ? mainTankUnit->ToPlayer() : nullptr;
    bool mainTankIsRealPlayer = mainTank && !GET_PLAYERBOT_AI(mainTank);

    if (mainTankIsRealPlayer)
    {
        if (!botAI->IsAssistTankOfIndex(bot, 0) && !botAI->IsAssistTankOfIndex(bot, 1) &&
            !botAI->IsAssistTankOfIndex(bot, 2))
            return false;
    }
    else if (!botAI->IsMainTank(bot))
    {
        return false;
    }

    int8 skullIndex = 7;  // Skull
    ObjectGuid currentSkullTarget = group->GetTargetIcon(skullIndex);

    // If there's no skull set yet, or the skull is on a different target, set the boss
    if (!currentSkullTarget || (boss->GetGUID() != currentSkullTarget))
    {
        group->SetTargetIcon(skullIndex, bot->GetGUID(), boss->GetGUID());
        return true;
    }

    return false;
}
