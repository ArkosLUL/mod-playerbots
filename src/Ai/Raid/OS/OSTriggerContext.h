/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_OSTRIGGERCONTEXT_H
#define PLAYERBOTS_OSTRIGGERCONTEXT_H

#include "NamedObjectContext.h"
#include "OSDefinitions.h"
#include "OSTriggers.h"

class RaidOsTriggerContext : public NamedObjectContext<Trigger>
{
public:
    RaidOsTriggerContext()
    {
        creators["sartharion dps"] = &RaidOsTriggerContext::sartharion_dps;
        creators["twilight portal exit"] = &RaidOsTriggerContext::twilight_portal_exit;

        OsSartharionDefinition().RegisterTriggers(creators);
    }

private:
    static Trigger* sartharion_dps(PlayerbotAI* ai) { return new SartharionDpsTrigger(ai); }
    static Trigger* twilight_portal_exit(PlayerbotAI* ai) { return new TwilightPortalExitTrigger(ai); }
};

#endif
