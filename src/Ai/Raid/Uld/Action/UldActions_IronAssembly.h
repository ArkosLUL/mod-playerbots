#ifndef PLAYERBOTS_ULDACTIONS_IRONASSEMBLY_H
#define PLAYERBOTS_ULDACTIONS_IRONASSEMBLY_H

#include "Action.h"
#include "AttackAction.h"
#include "GenericSpellActions.h"
#include "MovementActions.h"
#include "PlayerbotAI.h"
#include "Playerbots.h"
#include "UldTriggers.h"

class IronAssemblyResetEncounterStateAction : public Action
{
public:
    IronAssemblyResetEncounterStateAction(PlayerbotAI* botAI)
        : Action(botAI, "iron assembly reset encounter state action")
    {
    }
    bool Execute(Event event) override;
};

class IronAssemblyOverwhelmingPowerRunOutAction : public MovementAction
{
public:
    IronAssemblyOverwhelmingPowerRunOutAction(PlayerbotAI* botAI)
        : MovementAction(botAI, "iron assembly overwhelming power run out action")
    {
    }
    bool Execute(Event event) override;
};

class IronAssemblyLightningTendrilsAction : public MovementAction
{
public:
    IronAssemblyLightningTendrilsAction(PlayerbotAI* botAI)
        : MovementAction(botAI, "iron assembly lightning tendrils action")
    {
    }
    bool Execute(Event event) override;
};

class IronAssemblyOverloadAction : public MovementAction
{
public:
    IronAssemblyOverloadAction(PlayerbotAI* botAI) : MovementAction(botAI, "iron assembly overload action") {}
    bool Execute(Event event) override;
};

class IronAssemblyRuneOfDeathAction : public MovementAction
{
public:
    IronAssemblyRuneOfDeathAction(PlayerbotAI* botAI) : MovementAction(botAI, "iron assembly rune of death action") {}
    bool Execute(Event event) override;
};

class IronAssemblyInterruptAction : public Action
{
public:
    IronAssemblyInterruptAction(PlayerbotAI* botAI) : Action(botAI, "iron assembly interrupt action") {}
    bool Execute(Event event) override;
};

// Target, taunt and park in one node: a tank that has its boss but is standing in the wrong place is
// as wrong as one that does not have it.
class IronAssemblyTankAssignmentAction : public AttackAction
{
public:
    IronAssemblyTankAssignmentAction(PlayerbotAI* botAI) : AttackAction(botAI, "iron assembly tank assignment action")
    {
    }
    bool Execute(Event event) override;

private:
    bool _spotReached = false;
};

class IronAssemblyOverwhelmingPowerSwapAction : public AttackAction
{
public:
    IronAssemblyOverwhelmingPowerSwapAction(PlayerbotAI* botAI)
        : AttackAction(botAI, "iron assembly overwhelming power swap action")
    {
    }
    bool Execute(Event event) override;
};

class IronAssemblyShieldOfRunesAction : public Action
{
public:
    IronAssemblyShieldOfRunesAction(PlayerbotAI* botAI) : Action(botAI, "iron assembly shield of runes action") {}
    bool Execute(Event event) override;
};

class IronAssemblyFusionPunchDispelAction : public Action
{
public:
    IronAssemblyFusionPunchDispelAction(PlayerbotAI* botAI)
        : Action(botAI, "iron assembly fusion punch dispel action")
    {
    }
    bool Execute(Event event) override;
};

// Standalone rather than built on the Naxx redirect base: Ulduar has no other Naxx include, and
// Freya's redirect is the same shape one file over.
class IronAssemblyRedirectThreatAction : public Action
{
public:
    IronAssemblyRedirectThreatAction(PlayerbotAI* botAI) : Action(botAI, "iron assembly redirect threat action") {}
    bool Execute(Event event) override;
    bool isUseful() override;

private:
    Player* GetRedirectTank();
};

class IronAssemblyRuneOfPowerAction : public MovementAction
{
public:
    IronAssemblyRuneOfPowerAction(PlayerbotAI* botAI) : MovementAction(botAI, "iron assembly rune of power action") {}
    bool Execute(Event event) override;
};

class IronAssemblyRuneOfPowerSoakAction : public MovementAction
{
public:
    IronAssemblyRuneOfPowerSoakAction(PlayerbotAI* botAI)
        : MovementAction(botAI, "iron assembly rune of power soak action")
    {
    }
    bool Execute(Event event) override;
};

class IronAssemblySetDpsPriorityAction : public AttackAction
{
public:
    IronAssemblySetDpsPriorityAction(PlayerbotAI* botAI) : AttackAction(botAI, "iron assembly set dps priority action")
    {
    }
    bool Execute(Event event) override;
};

class IronAssemblyRaidPositionAction : public MovementAction
{
public:
    IronAssemblyRaidPositionAction(PlayerbotAI* botAI) : MovementAction(botAI, "iron assembly raid position action") {}
    bool Execute(Event event) override;

private:
    bool _spotReached = false;
};

#endif
