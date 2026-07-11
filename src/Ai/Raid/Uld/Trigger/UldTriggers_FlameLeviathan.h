#ifndef PLAYERBOTS_ULDTRIGGERS_FLAMELEVIATHAN_H
#define PLAYERBOTS_ULDTRIGGERS_FLAMELEVIATHAN_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "UldBossHelper.h"
#include "Trigger.h"

//
// Flame Levi
//
class FlameLeviathanOnVehicleTrigger : public Trigger
{
public:
    FlameLeviathanOnVehicleTrigger(PlayerbotAI* ai) : Trigger(ai, "flame leviathan on vehicle") {}
    bool IsActive() override;
};

class FlameLeviathanVehicleNearTrigger : public Trigger
{
public:
    FlameLeviathanVehicleNearTrigger(PlayerbotAI* ai) : Trigger(ai, "flame leviathan vehicle near") {}
    bool IsActive() override;
};

#endif
