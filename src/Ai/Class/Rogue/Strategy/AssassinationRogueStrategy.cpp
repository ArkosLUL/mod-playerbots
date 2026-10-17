/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "AssassinationRogueStrategy.h"
#include "Playerbots.h"

class AssassinationRogueStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    AssassinationRogueStrategyActionNodeFactory()
    {
        creators["mutilate"] = &mutilate;
        creators["envenom"] = &envenom;
        creators["backstab"] = &backstab;
    }

private:
    static ActionNode* mutilate([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode(
            "mutilate",
            /*P*/ {},
            /*A*/ { NextAction("backstab") },
            /*C*/ {}
        );
    }
    static ActionNode* envenom([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode(
            "envenom",
            /*P*/ {},
            /*A*/ { NextAction("eviscerate") },
            /*C*/ {}
        );
    }
    static ActionNode* backstab([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode(
            "backstab",
            /*P*/ {},
            /*A*/ { NextAction("sinister strike") },
            /*C*/ {}
        );
    }
};

AssassinationRogueStrategy::AssassinationRogueStrategy(PlayerbotAI* botAI)
    : GenericRogueStrategy(botAI)
{
    actionNodeFactories.Add(new AssassinationRogueStrategyActionNodeFactory());
}

std::vector<NextAction> AssassinationRogueStrategy::getDefaultActions()
{
    return {
        NextAction("melee", ACTION_DEFAULT)
    };
}

void AssassinationRogueStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericRogueStrategy::InitTriggers(triggers);

    // Both openers need stealth, so they sit above the survival band - nothing is hitting us yet.
    triggers.push_back(
        new TriggerNode(
            "in stealth",
            {
                NextAction("garrote", ACTION_HIGH + 9.5f),
                NextAction("ambush", ACTION_HIGH + 9.4f)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "slice and dice",
            {
                NextAction("slice and dice", ACTION_HIGH + 5),
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "assassination rupture",
            {
                NextAction("rupture", ACTION_HIGH + 4),
            }
        )
    );

    // Hunger for Blood needs a bleed on the target, so it has to sit under Rupture.
    triggers.push_back(
        new TriggerNode(
            "hunger for blood",
            {
                NextAction("hunger for blood", ACTION_HIGH + 3),
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "expose armor",
            {
                NextAction("expose armor", ACTION_HIGH + 2),
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "envenom",
            {
                NextAction("cold blood", ACTION_HIGH + 1.5f),
                NextAction("envenom", ACTION_HIGH + 1)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "target with combo points almost dead",
            {
                NextAction("envenom", ACTION_HIGH)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "medium aoe",
            {
                NextAction("fan of knives", ACTION_NORMAL + 5),
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "combo points not full and high energy",
            {
                NextAction("mutilate", ACTION_NORMAL + 3)
            }
        )
    );
}
