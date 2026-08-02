/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#include "AfflictionWarlockStrategy.h"
#include "Playerbots.h"

AfflictionWarlockStrategy::AfflictionWarlockStrategy(PlayerbotAI* botAI) : GenericWarlockStrategy(botAI)
{
    // No custom ActionNodeFactory needed
}

// ===== Default Actions =====
std::vector<NextAction> AfflictionWarlockStrategy::getDefaultActions()
{
    return {
       NextAction("corruption", 5.5f),
       NextAction("unstable affliction", 5.4f),
       NextAction("haunt", 5.3f),
       NextAction("shadow bolt", 5.2f),
       NextAction("shoot", 5.0f)
    };
}

// ===== Trigger Initialization ===
void AfflictionWarlockStrategy::InitTriggers(std::vector<TriggerNode*>& triggers)
{
    GenericWarlockStrategy::InitTriggers(triggers);

    // Main DoT triggers for high uptime
    triggers.push_back(
        new TriggerNode(
            "corruption on attacker",
            {
                NextAction("corruption on attacker", 19.5f)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "unstable affliction on attacker",
            {
                NextAction("unstable affliction on attacker", 19.0f)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "corruption",
            {
                NextAction("corruption", 18.0f)
            }
        )
    );
    // Haunt outranks Unstable Affliction: it is the bigger throughput gain and every cast rolls
    // Corruption forward through Everlasting Affliction.
    triggers.push_back(
        new TriggerNode(
            "haunt",
            {
                NextAction("haunt", 17.75f)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "unstable affliction",
            {
                NextAction("unstable affliction", 17.5f)
            }
        )
    );

    // Corruption is only ever cast once per fight, so its crit snapshot has to be refreshed by hand
    // once the bot's real crit is up. Sits under the DoT block so it never delays a missing DoT.
    triggers.push_back(
        new TriggerNode(
            "corruption snapshot",
            {
                NextAction("corruption resnapshot", 16.5f)
            }
        )
    );

    // Drain Soul as execute if target is low HP // Shadow Trance for free casts
    triggers.push_back(
        new TriggerNode(
            "shadow trance",
            {
                NextAction("shadow bolt", 16.0f)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "drain soul execute",
            {
                NextAction("drain soul", 15.5f)
            }
        )
    );

    // Life Tap glyph buff, and Life Tap as filler
    triggers.push_back(
        new TriggerNode(
            "life tap glyph buff",
            {
                NextAction("life tap", 29.5f)
            }
        )
    );
    triggers.push_back(
        new TriggerNode(
            "life tap",
            {
                NextAction("life tap", 5.1f)
            }
        )
    );

    triggers.push_back(
        new TriggerNode(
            "enemy too close for spell",
            {
                NextAction("flee", 39.0f)
            }
        )
    );
}
