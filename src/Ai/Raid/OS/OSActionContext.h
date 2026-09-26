/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_OSACTIONCONTEXT_H
#define PLAYERBOTS_OSACTIONCONTEXT_H

#include "Action.h"
#include "NamedObjectContext.h"
#include "OSActions.h"
#include "OSDefinitions.h"

class RaidOsActionContext : public NamedObjectContext<Action>
{
public:
    RaidOsActionContext()
    {
        creators["sartharion attack priority"] = &RaidOsActionContext::attack_priority;
        creators["exit twilight portal"] = &RaidOsActionContext::exit_twilight_portal;

        OsSartharionDefinition().RegisterActions(creators);
    }

private:
    static Action* attack_priority(PlayerbotAI* ai) { return new SartharionAttackPriorityAction(ai); }
    static Action* exit_twilight_portal(PlayerbotAI* ai) { return new ExitTwilightPortalAction(ai); }
};

#endif
