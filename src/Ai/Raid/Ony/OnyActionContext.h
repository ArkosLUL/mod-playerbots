/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ONYACTIONCONTEXT_H
#define PLAYERBOTS_ONYACTIONCONTEXT_H

#include "Action.h"
#include "NamedObjectContext.h"
#include "OnyDefinitions.h"

class RaidOnyxiaActionContext : public NamedObjectContext<Action>
{
public:
    RaidOnyxiaActionContext() { OnyxiaDefinition().RegisterActions(creators); }
};

#endif
