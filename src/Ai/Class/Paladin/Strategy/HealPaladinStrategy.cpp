/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "HealPaladinStrategy.h"
#include "Playerbots.h"
#include "Strategy.h"

HealPaladinStrategy::HealPaladinStrategy(PlayerbotAI* botAI) : GenericPaladinStrategy(botAI)
{
    // No custom ActionNodeFactory needed
}

std::vector<NextAction> HealPaladinStrategy::getDefaultActions()
{
    // Default actions are pushed every tick with no trigger, so they bypass the mana floor on
    // "healer should attack". The gated healer dps nodes are the idle behaviour we want instead.
    return {};
}

void HealPaladinStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericPaladinStrategy::InitTriggers(triggers);

    triggers.push_back(
        new TriggerNode(
            "beacon of light on tank",
            {
                NextAction("beacon of light on tank", ACTION_CRITICAL_HEAL + 7)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "party member critical health",
            {
                NextAction("holy shock on party", ACTION_CRITICAL_HEAL + 6),
                NextAction("divine favor", ACTION_CRITICAL_HEAL + 5.5f),
                NextAction("holy light on party", ACTION_CRITICAL_HEAL + 4)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "medium group heal setting",
            {
                NextAction("divine sacrifice", ACTION_CRITICAL_HEAL + 5),
                NextAction("avenging wrath", ACTION_CRITICAL_HEAL + 1),
                NextAction("divine illumination", ACTION_CRITICAL_HEAL + 0.5f),
                NextAction("aura mastery", ACTION_CRITICAL_HEAL)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "sacred shield on tank",
            {
                NextAction("sacred shield on tank", ACTION_CRITICAL_HEAL + 2)
            }
        )
    );
    // Judgements of the Pure is 15% haste for 60s on everything the paladin casts, and Judgement of
    // Light heals the raid off melee swings, so one global every 20s pays for itself. Priced under
    // the critical band and the raid cooldown window, over the low band.
    triggers.push_back(
        new TriggerNode(
            "paladin judgement of light",
            {
                NextAction("judgement of light", ACTION_MEDIUM_HEAL + 7)
            }
        )
    );
    // The proc cuts the cast time of the next Flash of Light, not Holy Light - at 2/2 that is the
    // full 1.5s, so Flash becomes instant and is the only heal besides Holy Shock the bot can land
    // while moving. On a Sacred Shielded target it also drops a 12s HoT (spell_pal_infusion_of_light).
    // Priced under the critical band on purpose: Holy Shock and Holy Light still lead on a dying
    // target, and this node picks up the global when they are vetoed - which is the moving case.
    triggers.push_back(
        new TriggerNode(
            "infusion of light",
            {
                NextAction("flash of light on party", ACTION_MEDIUM_HEAL + 6.5f)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "party member low health",
            {
                NextAction("holy shock on party", ACTION_MEDIUM_HEAL + 6),
                NextAction("holy light on party", ACTION_MEDIUM_HEAL + 5)
            }
        )
    );
    // Nearly empty: refill above the medium band, because heals from an empty bar are worth less
    // than the 50% healing penalty costs. Divine Illumination normally waits for Avenging Wrath
    // (see CastDivineIlluminationAction::isUseful), this is its bail-out node.
    triggers.push_back(
        new TriggerNode(
            "low mana",
            {
                NextAction("divine plea", ACTION_HIGH + 3),
                NextAction("divine illumination", ACTION_HIGH + 2)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "seal",
            {
                NextAction("seal of wisdom", ACTION_HIGH),
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "party member medium health",
            {
                NextAction("holy shock on party", ACTION_LIGHT_HEAL + 9.5f),
                NextAction("holy light on party", ACTION_LIGHT_HEAL + 9)
            }
        )
    );
    // Glyphed Holy Light splashes 10% to five nearby allies and Beacon mirrors it, so it stays the
    // main heal right up to the top-off band.
    triggers.push_back(
        new TriggerNode(
            "party member almost full health",
            {
                NextAction("holy light on party", ACTION_LIGHT_HEAL + 4)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "paladin divine plea",
            {
                NextAction("divine plea", ACTION_LIGHT_HEAL + 2)
            }
        )
    );
    // Below the interrupt band: chasing a target must not outrank Lay on Hands on someone dying
    // within range.
    triggers.push_back(
        new TriggerNode(
            "party member to heal out of spell range",
            {
                NextAction("reach party member to heal", ACTION_INTERRUPT - 0.5f)
            }
        )
    );
}
