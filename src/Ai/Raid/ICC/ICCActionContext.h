/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCACTIONCONTEXT_H
#define PLAYERBOTS_ICCACTIONCONTEXT_H

#include "Action.h"
#include "ICCActions.h"
#include "ICCDefinitions.h"
#include "NamedObjectContext.h"

class RaidIccActionContext : public NamedObjectContext<Action>
{
public:
    RaidIccActionContext()
    {
        for (EncounterDefinition const* encounter : IccEncounterDefinitions())
            encounter->RegisterActions(creators);
    }
};

#endif
