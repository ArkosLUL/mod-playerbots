#ifndef PLAYERBOTS_RAID_TOCTRIGGERS_NORTHRENDBEASTS_H
#define PLAYERBOTS_RAID_TOCTRIGGERS_NORTHRENDBEASTS_H

#include <vector>

#include "NamedObjectContext.h"
#include "Trigger.h"

// Triggers shared by Gormok, the Jormungars and Icehowl.
class ToCNorthrendBeastsTriggerContext : public NamedObjectContext<Trigger>
{
};

inline void AddToCNorthrendBeastsTriggerNodes(std::vector<TriggerNode*>& /*triggers*/) {}

#endif
