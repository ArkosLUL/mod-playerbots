/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "SurvivalHunterStrategy.h"
#include "Playerbots.h"

// ===== Action Node Factory =====
class SurvivalHunterStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    SurvivalHunterStrategyActionNodeFactory()
    {
        creators["explosive shot rank 4"] = &explosive_shot_rank_4;
        creators["explosive shot rank 3"] = &explosive_shot_rank_3;
        creators["explosive shot rank 2"] = &explosive_shot_rank_2;
    }

private:
    static ActionNode* explosive_shot_rank_4([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("explosive shot rank 4",
                              /*P*/ {},
                              /*A*/ { NextAction("explosive shot rank 3") },
                              /*C*/ {});
    }
    static ActionNode* explosive_shot_rank_3([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("explosive shot rank 3",
                              /*P*/ {},
                              /*A*/ { NextAction("explosive shot rank 2") },
                              /*C*/ {});
    }
    static ActionNode* explosive_shot_rank_2([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode("explosive shot rank 2",
                              /*P*/ {},
                              /*A*/ { NextAction("explosive shot rank 1") },
                              /*C*/ {});
    }
};

SurvivalHunterStrategy::SurvivalHunterStrategy(PlayerbotAI* botAI) : GenericHunterStrategy(botAI)
{
    actionNodeFactories.Add(new SurvivalHunterStrategyActionNodeFactory());
}

// ===== Default Actions =====
std::vector<NextAction> SurvivalHunterStrategy::getDefaultActions()
{
    // No Arcane Shot here: Survival always has Explosive Shot, which the Arcane Shot action refuses to
    // compete with.
    return {
        NextAction("kill command", ACTION_DEFAULT + 0.9f),
        NextAction("kill shot", ACTION_DEFAULT + 0.8f),
        NextAction("explosive shot", ACTION_DEFAULT + 0.7f),
        NextAction("black arrow", ACTION_DEFAULT + 0.6f),
        NextAction("serpent sting", ACTION_DEFAULT + 0.5f),
        NextAction("aimed shot", ACTION_DEFAULT + 0.4f),
        NextAction("steady shot", ACTION_DEFAULT + 0.2f),
        NextAction("auto shot", ACTION_DEFAULT + 0.1f)
    };
}

// ===== Trigger Initialization ===
void SurvivalHunterStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericHunterStrategy::InitTriggers(triggers);

    triggers.push_back(
        new TriggerNode(
            "lock and load",
            {
                NextAction("explosive shot rank 4", ACTION_HIGH + 8)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "kill command",
            {
                NextAction("kill command", ACTION_NORMAL + 8.5f)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "target critical health",
            {
                NextAction("kill shot", ACTION_NORMAL + 8)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "explosive shot",
            {
                NextAction("explosive shot", ACTION_NORMAL + 7.5f)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "black arrow",
            {
                NextAction("black arrow", ACTION_NORMAL + 6.5f)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "low mana",
            {
                NextAction("viper sting", ACTION_NORMAL + 6)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "no stings",
            {
                NextAction("serpent sting", ACTION_NORMAL + 5.5f)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "serpent sting on attacker",
            {
                NextAction("serpent sting on attacker", ACTION_NORMAL + 5)
            }
        )
    );
}
