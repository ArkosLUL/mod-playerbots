/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "GenericRogueStrategy.h"
#include "AiFactory.h"
#include "Playerbots.h"

class GenericRogueStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    GenericRogueStrategyActionNodeFactory()
    {
        creators["use deadly poison on off hand"] = &use_deadly_poison_on_off_hand;
        creators["use deadly poison on main hand"] = &use_deadly_poison_on_main_hand;
        creators["kick"] = &kick;
    }

private:
    static ActionNode* use_deadly_poison_on_off_hand([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode(
            "use deadly poison on off hand",
            /*P*/ {},
            /*A*/ { NextAction("use instant poison on off hand") },
            /*C*/ {}
        );
    }
    static ActionNode* use_deadly_poison_on_main_hand([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode(
            "use deadly poison on main hand",
            /*P*/ {},
            /*A*/ { NextAction("use instant poison on main hand") },
            /*C*/ {}
        );
    }
    static ActionNode* kick([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode(
            "kick",
            /*P*/ {},
            /*A*/ { NextAction("kidney shot") },
            /*C*/ {}
        );
    }
};

GenericRogueStrategy::GenericRogueStrategy(PlayerbotAI* botAI) : MeleeCombatStrategy(botAI)
{
    actionNodeFactories.Add(new GenericRogueStrategyActionNodeFactory());
}

void GenericRogueStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    MeleeCombatStrategy::InitTriggers(triggers);

    // Assassination runs Deadly on the main hand. At 5 stacks each extra Deadly proc casts the off-hand
    // poison instead, so Instant goes there.
    if (AiFactory::GetPlayerSpecTab(botAI->GetBot()) == ROGUE_TAB_ASSASSINATION)
    {
        triggers.push_back(
            new TriggerNode(
                "main hand weapon no enchant",
                {
                    NextAction("use deadly poison on main hand", ACTION_HIGH + 6)
                }
            )
        );
        triggers.push_back(
            new TriggerNode(
                "off hand weapon no enchant",
                {
                    NextAction("use instant poison on off hand", ACTION_HIGH + 5.5f)
                }
            )
        );
    }
    else
    {
        triggers.push_back(
            new TriggerNode(
                "main hand weapon no enchant",
                {
                    NextAction("use instant poison on main hand", ACTION_HIGH + 6)
                }
            )
        );
        triggers.push_back(
            new TriggerNode(
                "off hand weapon no enchant",
                {
                    NextAction("use deadly poison on off hand", ACTION_HIGH + 5.5f)
                }
            )
        );
    }

    triggers.push_back(
        new TriggerNode(
            "kick",
            {
                NextAction("kick", ACTION_INTERRUPT + 2)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "kick on enemy healer",
            {
                NextAction("kick on enemy healer", ACTION_INTERRUPT + 1)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "low health",
            {
                NextAction("evasion", ACTION_HIGH + 9),
                NextAction("feint", ACTION_HIGH + 8)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "critical health",
            {
                NextAction("cloak of shadows", ACTION_HIGH + 7)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "tricks of the trade",
            {
                NextAction("tricks of the trade", ACTION_HIGH + 6.5f)
            }
        )
    );

    // Has to outrank the inherited "reach melee" on the same trigger, or that always wins the tick
    // and we never sprint to close the gap.
    triggers.push_back(
        new TriggerNode(
            "enemy out of melee",
            {
                NextAction("sprint", ACTION_HIGH + 2)
            }
        )
    );
}

class StealthedRogueStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    StealthedRogueStrategyActionNodeFactory()
    {
        creators["ambush"] = &ambush;
        creators["sinister strike"] = &sinister_strike;
    }

private:
    static ActionNode* ambush([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode(
            "ambush",
            /*P*/ {},
            /*A*/ { NextAction("garrote") },
            /*C*/ {}
        );
    }

    static ActionNode* sinister_strike([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode(
            "sinister strike",
            /*P*/ {},
            /*A*/ { NextAction("cheap shot") },
            /*C*/ {}
        );
    }
};

StealthedRogueStrategy::StealthedRogueStrategy(PlayerbotAI* botAI) : Strategy(botAI)
{
    actionNodeFactories.Add(new StealthedRogueStrategyActionNodeFactory());
}

std::vector<NextAction> StealthedRogueStrategy::getDefaultActions()
{
    return {
        NextAction("ambush", ACTION_NORMAL + 4),
        NextAction("backstab", ACTION_NORMAL + 3),
        NextAction("cheap shot", ACTION_NORMAL + 2),
        NextAction("sinister strike", ACTION_NORMAL + 1),
        NextAction("melee", ACTION_NORMAL)
    };
}

void StealthedRogueStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(
        new TriggerNode(
            "combo points 5 available",
            {
                NextAction("eviscerate", ACTION_HIGH)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "kick",
            {
                NextAction("cheap shot", ACTION_INTERRUPT)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "kick on enemy healer",
            {
                NextAction("cheap shot", ACTION_INTERRUPT)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "behind target",
            {
                NextAction("ambush", ACTION_HIGH)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "not behind target",
            {
                NextAction("cheap shot", ACTION_HIGH)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "enemy flagcarrier near",
            {
                NextAction("sprint", ACTION_EMERGENCY + 1)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "unstealth",
            {
                NextAction("unstealth", ACTION_NORMAL)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "no stealth",
            {
                NextAction("check stealth", ACTION_EMERGENCY)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "sprint",
            {
                NextAction("sprint", ACTION_INTERRUPT)
            }
        )
    );
}

void StealthStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(
        new TriggerNode(
            "stealth",
            {
                NextAction("stealth", ACTION_INTERRUPT)
            }
        )
    );
}

void RogueAoeStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(
        new TriggerNode(
            "light aoe",
            {
                NextAction("blade flurry", ACTION_HIGH + 4)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "medium aoe",
            {
                NextAction("fan of knives", ACTION_NORMAL + 5)
            }
        )
    );
}

void RogueBoostStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(
        new TriggerNode(
            "adrenaline rush",
            {
                NextAction("adrenaline rush", ACTION_HIGH + 2)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "blade flurry",
            {
                NextAction("blade flurry", ACTION_HIGH + 4)
            }
        )
    );
}

void RogueCcStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    triggers.push_back(
        new TriggerNode(
            "sap",
            {
                NextAction("stealth", ACTION_INTERRUPT),
                NextAction("sap", ACTION_INTERRUPT)
            }
        )
    );
}
