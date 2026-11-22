#ifndef PLAYERBOTS_ULDTRIGGERS_FLAMELEVIATHAN_H
#define PLAYERBOTS_ULDTRIGGERS_FLAMELEVIATHAN_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "Trigger.h"

class FlameLeviathanOnVehicleTrigger : public Trigger
{
public:
    static constexpr char const* Name = "flame leviathan on vehicle";

    FlameLeviathanOnVehicleTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class FlameLeviathanVehicleNearTrigger : public Trigger
{
public:
    static constexpr char const* Name = "flame leviathan vehicle near";

    FlameLeviathanVehicleNearTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class FlameLeviathanFlameVentsTrigger : public Trigger
{
public:
    static constexpr char const* Name = "flame leviathan flame vents";

    FlameLeviathanFlameVentsTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class FlameLeviathanDriveUrgentTrigger : public Trigger
{
public:
    static constexpr char const* Name = "flame leviathan drive urgent";

    FlameLeviathanDriveUrgentTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

#endif
