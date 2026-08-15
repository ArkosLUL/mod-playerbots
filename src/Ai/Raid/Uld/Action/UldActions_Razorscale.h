#ifndef PLAYERBOTS_ULDACTIONS_RAZORSCALE_H
#define PLAYERBOTS_ULDACTIONS_RAZORSCALE_H

#include "Action.h"
#include "AttackAction.h"
#include "GenericActions.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldBossHelper.h"
#include "UldTriggers.h"
#include "Vehicle.h"

//
//  Razorscale
//

class RazorscaleAvoidDevouringFlameAction : public MovementAction
{
public:
    RazorscaleAvoidDevouringFlameAction(PlayerbotAI* botAI) : MovementAction(botAI, "razorscale avoid devouring flames") {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    // Widened for the main tank while she is airborne so he can hold the Dark Rune adds away from the
    // patches; on the ground he only has to clear his own footprint.
    float ClearRadius();
    bool StepClearOfFlames(Unit* flame, float clearRadius);

    // True when the bot is standing clear but the spot it would walk back to is on fire.
    bool ReturnSpotBlocked();
};

class RazorscaleAvoidSentinelAction : public MovementAction
{
public:
    RazorscaleAvoidSentinelAction(PlayerbotAI* botAI) : MovementAction(botAI, "razorscale avoid sentinel") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class RazorscaleIgnoreBossAction : public AttackAction
{
public:
    RazorscaleIgnoreBossAction(PlayerbotAI* botAI) : AttackAction(botAI, "razorscale ignore flying alone") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class RazorscaleAvoidWhirlwindAction : public MovementAction
{
public:
    RazorscaleAvoidWhirlwindAction(PlayerbotAI* botAI) : MovementAction(botAI, "razorscale avoid whirlwind") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class RazorscaleGroundedAction : public AttackAction
{
public:
    RazorscaleGroundedAction(PlayerbotAI* botAI) : AttackAction(botAI, "razorscale grounded") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class RazorscaleHarpoonAction : public MovementAction
{
public:
    RazorscaleHarpoonAction(PlayerbotAI* botAI) : MovementAction(botAI, "razorscale harpoon action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class RazorscaleFuseArmorAction : public MovementAction
{
public:
    RazorscaleFuseArmorAction(PlayerbotAI* botAI) : MovementAction(botAI, "razorscale fuse armor action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

// Sole owner of the skull icon for this encounter: adds while she is airborne, the boss herself the
// moment she is on the floor. DpsTargetValue prefers the RTI target, so this is what the raid hits.
class RazorscaleKillTargetAction : public Action
{
public:
    RazorscaleKillTargetAction(PlayerbotAI* botAI) : Action(botAI, "razorscale kill target action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

// The generic pet-attack node is commented out engine-wide, so a pet keeps whatever it last hit
// unless a script hands it a new order.
class RazorscalePetControlAction : public Action
{
public:
    RazorscalePetControlAction(PlayerbotAI* botAI) : Action(botAI, "razorscale pet control action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class RazorscaleFlameBreathAction : public MovementAction
{
public:
    RazorscaleFlameBreathAction(PlayerbotAI* botAI) : MovementAction(botAI, "razorscale flame breath action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

#endif
