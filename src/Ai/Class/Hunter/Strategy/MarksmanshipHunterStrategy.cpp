/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "MarksmanshipHunterStrategy.h"
#include "Playerbots.h"

MarksmanshipHunterStrategy::MarksmanshipHunterStrategy(PlayerbotAI* botAI) : GenericHunterStrategy(botAI)
{
    // No custom ActionNodeFactory needed
}

// ===== Default Actions =====
std::vector<NextAction> MarksmanshipHunterStrategy::getDefaultActions()
{
    return {
        NextAction("kill command", ACTION_DEFAULT + 0.8f),
        NextAction("kill shot", ACTION_DEFAULT + 0.7f),
        NextAction("serpent sting", ACTION_DEFAULT + 0.6f),
        NextAction("chimera shot", ACTION_DEFAULT + 0.5f),
        NextAction("aimed shot", ACTION_DEFAULT + 0.4f),
        NextAction("arcane shot", ACTION_DEFAULT + 0.3f),
        NextAction("steady shot", ACTION_DEFAULT + 0.2f),
        NextAction("auto shot", ACTION_DEFAULT + 0.1f)
    };
}

// ===== Trigger Initialization ===
void MarksmanshipHunterStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericHunterStrategy::InitTriggers(triggers);

    triggers.push_back(
        new TriggerNode(
            "silencing shot",
            {
                NextAction("silencing shot", ACTION_INTERRUPT)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "readiness",
            {
                NextAction("readiness", ACTION_HIGH + 8.9f)
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
            "silencing shot on immune target",
            {
                NextAction("silencing shot", ACTION_NORMAL + 8.4f)
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
    // Sting first when none is up: Chimera Shot on an unstung target has nothing to refresh and loses
    // its serpent damage. With a sting up this never fires, so Chimera still does the refreshing.
    triggers.push_back(
        new TriggerNode(
            "no stings",
            {
                NextAction("serpent sting", ACTION_NORMAL + 7.1f)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "chimera shot no cd",
            {
                NextAction("chimera shot", ACTION_NORMAL + 6.9f)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "serpent sting on attacker",
            {
                NextAction("serpent sting on attacker", ACTION_NORMAL + 6)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "aimed shot no cd",
            {
                NextAction("aimed shot", ACTION_NORMAL + 5.5f)
            }
        )
    );
}
