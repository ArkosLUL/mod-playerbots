/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "CombatRogueStrategy.h"
#include "Playerbots.h"

class CombatRogueStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    CombatRogueStrategyActionNodeFactory()
    {
        creators["sinister strike"] = &sinister_strike;
        creators["rupture"] = &rupture;
    }

private:
    static ActionNode* sinister_strike([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode(
            "sinister strike",
            /*P*/ {},
            /*A*/ { NextAction("melee") },
            /*C*/ {}
        );
    }
    static ActionNode* rupture([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode(
            "rupture",
            /*P*/ {},
            /*A*/ { NextAction("eviscerate") },
            /*C*/ {}
        );
    }
};

CombatRogueStrategy::CombatRogueStrategy(PlayerbotAI* botAI) : GenericRogueStrategy(botAI)
{
    actionNodeFactories.Add(new CombatRogueStrategyActionNodeFactory());
}

std::vector<NextAction> CombatRogueStrategy::getDefaultActions()
{
    return {
        NextAction("melee", ACTION_DEFAULT)
    };
}

void CombatRogueStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
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
                NextAction("slice and dice", ACTION_HIGH + 5)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "riposte",
            {
                NextAction("riposte", ACTION_HIGH + 4.5f)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "combat rupture",
            {
                NextAction("rupture", ACTION_HIGH + 4)
            }
        )
    );

    // under Rupture, so a due Rupture keeps its 5 points and this takes the next 3
    triggers.push_back(
        new TriggerNode(
            "slice and dice before rupture",
            {
                NextAction("slice and dice", ACTION_HIGH + 3.5f)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "killing spree",
            {
                NextAction("killing spree", ACTION_HIGH + 3)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "blade flurry",
            {
                NextAction("blade flurry", ACTION_HIGH + 2)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "expose armor",
            {
                NextAction("expose armor", ACTION_HIGH + 1)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "target with combo points almost dead",
            {
                NextAction("eviscerate", ACTION_HIGH + 0.5f)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "combat eviscerate",
            {
                NextAction("eviscerate", ACTION_HIGH)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "medium threat",
            {
                NextAction("vanish", ACTION_HIGH - 3)
            }
        )
    );

    // Sinister Strike costs 40 energy; gating on the 60-energy trigger idled the bot through the
    // whole 40-59 band.
    triggers.push_back(
        new TriggerNode(
            "combo points not full and medium energy",
            {
                NextAction("sinister strike", ACTION_NORMAL + 3)
            }
        )
    );

    // at 5 points with Eviscerate held for the Rupture refresh, spend before energy caps, as the sim does
    triggers.push_back(
        new TriggerNode(
            "almost full energy available",
            {
                NextAction("sinister strike", ACTION_NORMAL + 2)
            }
        )
    );
}
