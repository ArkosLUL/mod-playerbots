/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "BeastMasteryHunterStrategy.h"
#include "Playerbots.h"

BeastMasteryHunterStrategy::BeastMasteryHunterStrategy(PlayerbotAI* botAI) : GenericHunterStrategy(botAI)
{
    // No custom ActionNodeFactory needed
}

// ===== Default Actions =====
std::vector<NextAction> BeastMasteryHunterStrategy::getDefaultActions()
{
    return {
        NextAction("kill command", ACTION_DEFAULT + 0.7f),
        NextAction("kill shot", ACTION_DEFAULT + 0.6f),
        NextAction("serpent sting", ACTION_DEFAULT + 0.5f),
        NextAction("aimed shot", ACTION_DEFAULT + 0.4f),
        NextAction("arcane shot", ACTION_DEFAULT + 0.3f),
        NextAction("steady shot", ACTION_DEFAULT + 0.2f),
        NextAction("auto shot", ACTION_DEFAULT + 0.1f)
    };
}

// ===== Trigger Initialization ===
void BeastMasteryHunterStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericHunterStrategy::InitTriggers(triggers);
    triggers.push_back(
        new TriggerNode(
            "intimidation",
            {
                NextAction("intimidation", ACTION_INTERRUPT)
            }
        )
    );
    // Next to Rapid Fire so both land inside the same burst window - The Beast Within wants them
    // stacked.
    triggers.push_back(
        new TriggerNode(
            "bestial wrath",
            {
                NextAction("bestial wrath", ACTION_HIGH + 8.5f)
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
            "low mana",
            {
                NextAction("viper sting", ACTION_NORMAL + 7.5f)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "no stings",
            {
                NextAction("serpent sting", ACTION_NORMAL + 7)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "serpent sting on attacker",
            {
                NextAction("serpent sting on attacker", ACTION_NORMAL + 6.5f)
            }
        )
    );
}
