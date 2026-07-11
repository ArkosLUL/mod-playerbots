#ifndef PLAYERBOTS_ULDTRIGGERS_RAZORSCALE_H
#define PLAYERBOTS_ULDTRIGGERS_RAZORSCALE_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "UldBossHelper.h"
#include "Trigger.h"

//
// Razorscale
//
class RazorscaleFlyingAloneTrigger : public Trigger
{
public:
    RazorscaleFlyingAloneTrigger(PlayerbotAI* ai) : Trigger(ai, "razorscale flying alone") {}
    bool IsActive() override;
};

class RazorscaleDevouringFlamesTrigger : public Trigger
{
public:
    RazorscaleDevouringFlamesTrigger(PlayerbotAI* ai) : Trigger(ai, "razorscale avoid devouring flames") {}
    bool IsActive() override;
};

class RazorscaleAvoidSentinelTrigger : public Trigger
{
public:
    RazorscaleAvoidSentinelTrigger(PlayerbotAI* ai) : Trigger(ai, "razorscale avoid sentinel") {}
    bool IsActive() override;
};

class RazorscaleAvoidWhirlwindTrigger : public Trigger
{
public:
    RazorscaleAvoidWhirlwindTrigger(PlayerbotAI* ai) : Trigger(ai, "razorscale avoid whirlwind") {}
    bool IsActive() override;
};

class RazorscaleGroundedTrigger : public Trigger
{
public:
    RazorscaleGroundedTrigger(PlayerbotAI* ai) : Trigger(ai, "razorscale grounded") {}
    bool IsActive() override;
};

class RazorscaleHarpoonAvailableTrigger : public Trigger
{
public:
    RazorscaleHarpoonAvailableTrigger(PlayerbotAI* ai) : Trigger(ai, "razorscale harpoon trigger") {}
    bool IsActive() override;
};

class RazorscaleFuseArmorTrigger : public Trigger
{
public:
    RazorscaleFuseArmorTrigger(PlayerbotAI* ai) : Trigger(ai, "razorscale fuse armor trigger") {}
    bool IsActive() override;
};

class RazorscaleFocusCasterTrigger : public Trigger
{
public:
    RazorscaleFocusCasterTrigger(PlayerbotAI* ai) : Trigger(ai, "razorscale focus caster trigger") {}
    bool IsActive() override;
};

class RazorscaleFlameBreathTrigger : public Trigger
{
public:
    RazorscaleFlameBreathTrigger(PlayerbotAI* ai) : Trigger(ai, "razorscale flame breath trigger") {}
    bool IsActive() override;
};

#endif
