/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_NAXXACTIONCONTEXT_H
#define PLAYERBOTS_NAXXACTIONCONTEXT_H

#include "Action.h"
#include "NamedObjectContext.h"
#include "NaxxActions.h"
#include "NaxxDefinitions.h"

class RaidNaxxActionContext : public NamedObjectContext<Action>
{
public:
    RaidNaxxActionContext()
    {
        for (EncounterDefinition const* encounter : NaxxEncounterDefinitions())
            encounter->RegisterActions(creators);
    }
};

#endif
