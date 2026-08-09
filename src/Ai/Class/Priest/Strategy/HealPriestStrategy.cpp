/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "HealPriestStrategy.h"
#include "GenericPriestStrategyActionNodeFactory.h"
#include "Playerbots.h"

HealPriestStrategy::HealPriestStrategy(PlayerbotAI* botAI) : GenericPriestStrategy(botAI)
{
    actionNodeFactories.Add(new GenericPriestStrategyActionNodeFactory());
}

std::vector<NextAction> HealPriestStrategy::getDefaultActions()
{
    return {
        NextAction("shoot", ACTION_DEFAULT)
    };
}

void HealPriestStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericPriestStrategy::InitTriggers(triggers);

    triggers.push_back(
        new TriggerNode(
            "group heal setting",
            {
                NextAction("prayer of mending on party", ACTION_MEDIUM_HEAL + 12.5f),
                NextAction("power word: shield on not full", ACTION_MEDIUM_HEAL + 12)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "medium group heal setting",
            {
                NextAction("divine hymn", ACTION_CRITICAL_HEAL + 6.8f),
                NextAction("power infusion", ACTION_CRITICAL_HEAL + 6.4f),
                // Inner Focus sits directly above Prayer of Healing so the free cast is spent on it.
                NextAction("inner focus", ACTION_CRITICAL_HEAL + 3.7f),
                NextAction("prayer of healing on party", ACTION_CRITICAL_HEAL + 3.5f),
                NextAction("power word: shield on not full", ACTION_CRITICAL_HEAL + 3)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "party member critical health",
            {
                NextAction("power word: shield on party", ACTION_CRITICAL_HEAL + 6),
                NextAction("penance on party", ACTION_CRITICAL_HEAL + 5.5f),
                NextAction("prayer of mending on party", ACTION_CRITICAL_HEAL + 5),
                NextAction("flash heal on party", ACTION_CRITICAL_HEAL + 4.5f)
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

    // The target is already shielded, so top it off with a direct heal instead of retrying PW:S.
    // Penance leads here or it never gets a look-in below the critical band: PW:S is on top of every
    // band, so Weakened Soul is up on the heal target almost permanently.
    triggers.push_back(
        new TriggerNode(
            "weakened soul on party member",
            {
                NextAction("penance on party", ACTION_MEDIUM_HEAL + 7.4f),
                NextAction("flash heal on party", ACTION_MEDIUM_HEAL + 7)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "party member low health",
            {
                NextAction("power word: shield on party", ACTION_MEDIUM_HEAL + 6),
                NextAction("penance on party", ACTION_MEDIUM_HEAL + 5.5f),
                NextAction("prayer of mending on party", ACTION_MEDIUM_HEAL + 5),
                NextAction("flash heal on party", ACTION_MEDIUM_HEAL + 4.5f)
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
                NextAction("power word: shield on party", ACTION_LIGHT_HEAL + 9),
                NextAction("penance on party", ACTION_LIGHT_HEAL + 8.5f),
                NextAction("prayer of mending on party", ACTION_LIGHT_HEAL + 8),
                NextAction("flash heal on party", ACTION_LIGHT_HEAL + 7.5f)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "party member almost full health",
            {
                NextAction("power word: shield on party", ACTION_LIGHT_HEAL + 3),
                NextAction("prayer of mending on party", ACTION_LIGHT_HEAL + 2),
                NextAction("renew on party", ACTION_LIGHT_HEAL + 1)
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

    triggers.push_back(
        new TriggerNode(
            "critical health", {
                NextAction("pain suppression", ACTION_EMERGENCY + 1)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "protect party member",
            {
                NextAction("pain suppression on party", ACTION_EMERGENCY)
            }
        )
    );
}
