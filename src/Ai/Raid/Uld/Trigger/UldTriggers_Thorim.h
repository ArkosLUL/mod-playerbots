#ifndef PLAYERBOTS_ULDTRIGGERS_THORIM_H
#define PLAYERBOTS_ULDTRIGGERS_THORIM_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "UldBossHelper.h"
#include "Trigger.h"

//
// Thorim
//
class ThorimUnbalancingStrikeTrigger : public Trigger
{
public:
    ThorimUnbalancingStrikeTrigger(PlayerbotAI* ai) : Trigger(ai, "thorim unbalancing strike trigger") {}
    bool IsActive() override;
};

class ThorimUnbalancingStrikeSwapTrigger : public Trigger
{
public:
    ThorimUnbalancingStrikeSwapTrigger(PlayerbotAI* ai) : Trigger(ai, "thorim unbalancing strike swap trigger") {}
    bool IsActive() override;
};

class ThorimMarkDpsTargetTrigger : public Trigger
{
public:
    ThorimMarkDpsTargetTrigger(PlayerbotAI* ai) : Trigger(ai, "thorim mark dps target trigger") {}
    bool IsActive() override;
};

class ThorimGauntletPositioningTrigger : public Trigger
{
public:
    ThorimGauntletPositioningTrigger(PlayerbotAI* ai) : Trigger(ai, "thorim gauntlet positioning trigger") {}
    bool IsActive() override;
};

class ThorimArenaPositioningTrigger : public Trigger
{
public:
    ThorimArenaPositioningTrigger(PlayerbotAI* ai) : Trigger(ai, "thorim arena positioning trigger") {}
    bool IsActive() override;
};

class ThorimFallFromFloorTrigger : public Trigger
{
public:
    ThorimFallFromFloorTrigger(PlayerbotAI* ai) : Trigger(ai, "thorim fall from floor trigger") {}
    bool IsActive() override;
};

class ThorimPhase2PositioningTrigger : public Trigger
{
public:
    ThorimPhase2PositioningTrigger(PlayerbotAI* ai) : Trigger(ai, "thorim phase 2 positioning trigger") {}
    bool IsActive() override;
};

#endif
