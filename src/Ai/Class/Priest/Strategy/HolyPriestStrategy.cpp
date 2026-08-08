/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "HolyPriestStrategy.h"
#include "Playerbots.h"

class HolyPriestStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    HolyPriestStrategyActionNodeFactory() { creators["smite"] = &smite; }

private:
    static ActionNode* smite([[maybe_unused]] PlayerbotAI* botAI)
    {
        return new ActionNode(
            "smite",
            /*P*/ {},
            /*A*/ { NextAction("shoot") },
            /*C*/ {}
        );
    }
};

HolyPriestStrategy::HolyPriestStrategy(PlayerbotAI* botAI) : HealPriestStrategy(botAI)
{
    actionNodeFactories.Add(new HolyPriestStrategyActionNodeFactory());
}

std::vector<NextAction> HolyPriestStrategy::getDefaultActions()
{
    return {
        NextAction("smite", ACTION_DEFAULT + 0.2f),
        NextAction("mana burn", ACTION_DEFAULT + 0.1f),
        NextAction("starshards", ACTION_DEFAULT)
    };
}

void HolyPriestStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    HealPriestStrategy::InitTriggers(triggers);

    triggers.push_back(
        new TriggerNode(
            "holy fire",
            {
                NextAction("holy fire", ACTION_NORMAL + 9)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "shadowfiend",
            {
                NextAction("shadowfiend", ACTION_HIGH + 0.5f)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "low mana",
            {
                NextAction("mana burn", ACTION_HIGH)
            }
        )
    );
}

HolyHealPriestStrategy::HolyHealPriestStrategy(PlayerbotAI* botAI) : GenericPriestStrategy(botAI)
{
    actionNodeFactories.Add(new GenericPriestStrategyActionNodeFactory());
}

std::vector<NextAction> HolyHealPriestStrategy::getDefaultActions()
{
    return { NextAction("shoot", ACTION_DEFAULT) };
}

void HolyHealPriestStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericPriestStrategy::InitTriggers(triggers);

    triggers.push_back(
        new TriggerNode(
            "group heal setting",
            {
                NextAction("prayer of mending on party", ACTION_MEDIUM_HEAL + 12.5f),
                NextAction("circle of healing on party", ACTION_MEDIUM_HEAL + 12)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "medium group heal setting",
            {
                NextAction("divine hymn", ACTION_CRITICAL_HEAL + 6.8f),
                // Inner Focus sits directly above Prayer of Healing so the free cast is spent on it.
                NextAction("inner focus", ACTION_CRITICAL_HEAL + 3.7f),
                NextAction("prayer of healing on party", ACTION_CRITICAL_HEAL + 3.5f),
                NextAction("circle of healing on party", ACTION_CRITICAL_HEAL + 3)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "party member critical health",
            {
                NextAction("guardian spirit on party", ACTION_CRITICAL_HEAL + 6.2f),
                NextAction("power word: shield on party", ACTION_CRITICAL_HEAL + 6),
                NextAction("circle of healing on party", ACTION_CRITICAL_HEAL + 5.5f),
                NextAction("prayer of mending on party", ACTION_CRITICAL_HEAL + 5),
                NextAction("flash heal on party", ACTION_CRITICAL_HEAL + 4.5f),
                NextAction("greater heal on party", ACTION_CRITICAL_HEAL + 4.2f)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "binding heal",
            {
                NextAction("binding heal", ACTION_CRITICAL_HEAL + 4)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "critical health",
            {
                NextAction("desperate prayer", ACTION_MEDIUM_HEAL + 8)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "low health",
            {
                NextAction("power word: shield", ACTION_MEDIUM_HEAL + 7.5f)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "party member low health",
            {
                NextAction("circle of healing on party", ACTION_MEDIUM_HEAL + 6),
                NextAction("prayer of mending on party", ACTION_MEDIUM_HEAL + 5.5f),
                NextAction("flash heal on party", ACTION_MEDIUM_HEAL + 5),
                NextAction("greater heal on party", ACTION_MEDIUM_HEAL + 4.5f)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "renew on main tank",
            {
                NextAction("renew on main tank", ACTION_MEDIUM_HEAL + 4)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "being attacked",
            {
                NextAction("power word: shield", ACTION_MEDIUM_HEAL + 3.5f)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "shadowfiend",
            {
                NextAction("shadowfiend", ACTION_MEDIUM_HEAL + 3)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "hymn of hope",
            {
                NextAction("hymn of hope", ACTION_MEDIUM_HEAL + 2)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "party member medium health",
            {
                NextAction("circle of healing on party", ACTION_LIGHT_HEAL + 9),
                NextAction("prayer of mending on party", ACTION_LIGHT_HEAL + 8.5f),
                NextAction("greater heal on party", ACTION_LIGHT_HEAL + 8),
                NextAction("renew on party", ACTION_LIGHT_HEAL + 7.5f)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "party member almost full health",
            {
                NextAction("prayer of mending on party", ACTION_LIGHT_HEAL + 3),
                NextAction("renew on party", ACTION_LIGHT_HEAL + 2)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "party member to heal out of spell range",
            {
                NextAction("reach party member to heal", ACTION_CRITICAL_HEAL + 8)
            }
        )
    );
}
