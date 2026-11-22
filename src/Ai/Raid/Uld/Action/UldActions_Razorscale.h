#ifndef PLAYERBOTS_ULDACTIONS_RAZORSCALE_H
#define PLAYERBOTS_ULDACTIONS_RAZORSCALE_H

#include "Action.h"
#include "AttackAction.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldTriggers.h"
#include "Vehicle.h"

class RazorscaleAvoidDevouringFlameAction : public MovementAction
{
public:
    static constexpr char const* Name = "razorscale avoid devouring flames";

    RazorscaleAvoidDevouringFlameAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    struct FlameScan
    {
        uint32 atMs = 0;
        ObjectGuid flame;
        float clearRadius = 0.0f;
    };

    FlameScan const& Scan();
    bool StepClearOfFlames(Unit* flame, float clearRadius);

    FlameScan _scan;
};

class RazorscaleAvoidSentinelAction : public MovementAction
{
public:
    static constexpr char const* Name = "razorscale avoid sentinel";

    RazorscaleAvoidSentinelAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class RazorscaleIgnoreBossAction : public AttackAction
{
public:
    static constexpr char const* Name = "razorscale ignore flying alone";

    RazorscaleIgnoreBossAction(PlayerbotAI* botAI) : AttackAction(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class RazorscaleAvoidWhirlwindAction : public MovementAction
{
public:
    static constexpr char const* Name = "razorscale avoid whirlwind";

    RazorscaleAvoidWhirlwindAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class RazorscaleGroundedAction : public AttackAction
{
public:
    static constexpr char const* Name = "razorscale grounded";

    RazorscaleGroundedAction(PlayerbotAI* botAI) : AttackAction(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class RazorscaleHarpoonAction : public MovementAction
{
public:
    static constexpr char const* Name = "razorscale harpoon action";

    RazorscaleHarpoonAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class RazorscaleFuseArmorAction : public MovementAction
{
public:
    static constexpr char const* Name = "razorscale fuse armor action";

    RazorscaleFuseArmorAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class RazorscaleKillTargetAction : public Action
{
public:
    static constexpr char const* Name = "razorscale kill target action";

    RazorscaleKillTargetAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class RazorscalePetControlAction : public Action
{
public:
    static constexpr char const* Name = "razorscale pet control action";

    RazorscalePetControlAction(PlayerbotAI* botAI) : Action(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class RazorscaleFlameBreathAction : public MovementAction
{
public:
    static constexpr char const* Name = "razorscale flame breath action";

    RazorscaleFlameBreathAction(PlayerbotAI* botAI) : MovementAction(botAI, Name) {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

#endif
