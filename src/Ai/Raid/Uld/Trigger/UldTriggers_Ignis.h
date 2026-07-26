#ifndef PLAYERBOTS_ULDTRIGGERS_IGNIS_H
#define PLAYERBOTS_ULDTRIGGERS_IGNIS_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "UldBossHelper.h"
#include "Trigger.h"

//
// Ignis the Furnace Master
//
class IgnisScorchedGroundTrigger : public Trigger
{
public:
    IgnisScorchedGroundTrigger(PlayerbotAI* ai) : Trigger(ai, "ignis scorched ground trigger") {}
    bool IsActive() override;
};

class IgnisIronConstructTrigger : public Trigger
{
public:
    IgnisIronConstructTrigger(PlayerbotAI* ai) : Trigger(ai, "ignis iron construct trigger") {}
    bool IsActive() override;
};

#endif
