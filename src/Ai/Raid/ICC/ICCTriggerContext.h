/*
 * This file is part of the mod-playerbots module for AzerothCore. See AUTHORS file for Copyright
 * information; released under GNU GPL v2 license, redistribute/modify under version 2 of the License,
 * or (at your option) any later version.
 */

#ifndef PLAYERBOTS_ICCTRIGGERCONTEXT_H
#define PLAYERBOTS_ICCTRIGGERCONTEXT_H

#include "ICCDefinitions.h"
#include "ICCTriggers.h"
#include "NamedObjectContext.h"

class RaidIccTriggerContext : public NamedObjectContext<Trigger>
{
public:
    RaidIccTriggerContext()
    {
        for (EncounterDefinition const* encounter : IccEncounterDefinitions())
            encounter->RegisterTriggers(creators);
    }
};

#endif
