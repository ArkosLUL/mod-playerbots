/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "RaidHealerEngine.h"

#include "ChooseTargetActions.h"
#include "InstanceScript.h"
#include "Playerbots.h"
#include "RaidObs.h"

bool RaidHealerEngineWindow(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (!PlayerbotAI::IsHeal(bot) || !bot->IsAlive() || !bot->IsInCombat() || bot->GetVehicle())
        return false;

    InstanceScript* instance = bot->GetInstanceScript();
    return instance && instance->IsEncounterInProgress();
}

bool RaidHealerCombatEngineTrigger::IsActive()
{
    return botAI->GetState() == BOT_STATE_NON_COMBAT && RaidHealerEngineWindow(botAI);
}

bool RaidHealerCombatEngineAction::Execute(Event /*event*/)
{
    botAI->ChangeEngine(BOT_STATE_COMBAT);

    if (RaidObs::Active())
        RaidObs::Note(bot, "raid.healerengine", "combat");

    return true;
}

float RaidHealerTargetHoldMultiplier::GetValue(Action* action)
{
    if (!dynamic_cast<DropTargetAction*>(action))
        return 1.0f;

    // a dead or invalid target still drops, so a corpse never stays the target. The trigger
    // switches back next tick.
    if (AI_VALUE(Unit*, "current target") || !RaidHealerEngineWindow(botAI))
        return 1.0f;

    return 0.0f;
}

void RaidHealerEngineStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    // the +10/+11 escapes and timers still go first
    triggers.push_back(new TriggerNode("raid healer combat engine trigger",
                                       {NextAction("raid healer combat engine action", ACTION_EMERGENCY + 9)}));
}

void RaidHealerEngineStrategy::InitMultipliers(std::vector<Multiplier*>& multipliers)
{
    multipliers.push_back(new RaidHealerTargetHoldMultiplier(botAI));
}
