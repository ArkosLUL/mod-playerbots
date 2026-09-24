#ifndef PLAYERBOTS_ULDTRIGGERS_RAZORSCALE_H
#define PLAYERBOTS_ULDTRIGGERS_RAZORSCALE_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "Trigger.h"

//
// Razorscale
//
class RazorscaleFlyingAloneTrigger : public Trigger
{
public:
    static constexpr char const* Name = "razorscale flying alone";

    RazorscaleFlyingAloneTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class RazorscaleDevouringFlamesTrigger : public Trigger
{
public:
    static constexpr char const* Name = "razorscale avoid devouring flames";

    RazorscaleDevouringFlamesTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class RazorscaleAvoidSentinelTrigger : public Trigger
{
public:
    static constexpr char const* Name = "razorscale avoid sentinel";

    RazorscaleAvoidSentinelTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class RazorscaleAvoidWhirlwindTrigger : public Trigger
{
public:
    static constexpr char const* Name = "razorscale avoid whirlwind";

    RazorscaleAvoidWhirlwindTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class RazorscaleGroundedTrigger : public Trigger
{
public:
    static constexpr char const* Name = "razorscale grounded";

    RazorscaleGroundedTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class RazorscaleHarpoonAvailableTrigger : public Trigger
{
public:
    static constexpr char const* Name = "razorscale harpoon trigger";

    RazorscaleHarpoonAvailableTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class RazorscaleFuseArmorTrigger : public Trigger
{
public:
    static constexpr char const* Name = "razorscale fuse armor trigger";

    RazorscaleFuseArmorTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class RazorscaleKillTargetTrigger : public Trigger
{
public:
    static constexpr char const* Name = "razorscale kill target trigger";

    RazorscaleKillTargetTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class RazorscalePetControlTrigger : public Trigger
{
public:
    static constexpr char const* Name = "razorscale pet control trigger";

    RazorscalePetControlTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

class RazorscaleFlameBreathTrigger : public Trigger
{
public:
    static constexpr char const* Name = "razorscale flame breath trigger";

    RazorscaleFlameBreathTrigger(PlayerbotAI* ai) : Trigger(ai, Name) {}
    bool IsActive() override;
};

#endif
