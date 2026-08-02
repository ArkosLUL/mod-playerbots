#ifndef PLAYERBOTS_ULDACTIONS_IRONASSEMBLY_H
#define PLAYERBOTS_ULDACTIONS_IRONASSEMBLY_H

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

class IronAssemblyLightningTendrilsAction : public MovementAction
{
public:
    IronAssemblyLightningTendrilsAction(PlayerbotAI* botAI) : MovementAction(botAI, "iron assembly lightning tendrils action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class IronAssemblyOverloadAction : public MovementAction
{
public:
    IronAssemblyOverloadAction(PlayerbotAI* botAI) : MovementAction(botAI, "iron assembly overload action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

class IronAssemblyRuneOfPowerAction : public MovementAction
{
public:
    IronAssemblyRuneOfPowerAction(PlayerbotAI* botAI) : MovementAction(botAI, "iron assembly rune of power action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

// Hard mode: mark the next council member to kill (Brundir, then Molgeim, then Steelbreaker).
class IronAssemblyKillOrderAction : public Action
{
public:
    IronAssemblyKillOrderAction(PlayerbotAI* botAI) : Action(botAI, "iron assembly kill order action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

// Hard mode: taunt the empowered Steelbreaker off a tank stacked with Fusion Punch / Overwhelming Power.
class IronAssemblyFusionPunchSwapAction : public AttackAction
{
public:
    IronAssemblyFusionPunchSwapAction(PlayerbotAI* botAI) : AttackAction(botAI, "iron assembly fusion punch swap action") {}
    bool Execute(Event event) override;
    bool isUseful() override;
};

#endif
