#ifndef PLAYERBOTS_ULDTRIGGERS_IRONASSEMBLY_H
#define PLAYERBOTS_ULDTRIGGERS_IRONASSEMBLY_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "UldBossHelper.h"
#include "Trigger.h"

//
// Iron Assembly
//
class IronAssemblyLightningTendrilsTrigger : public Trigger
{
public:
    IronAssemblyLightningTendrilsTrigger(PlayerbotAI* ai) : Trigger(ai, "iron assembly lightning tendrils trigger") {}
    bool IsActive() override;
};

class IronAssemblyOverloadTrigger : public Trigger
{
public:
    IronAssemblyOverloadTrigger(PlayerbotAI* ai) : Trigger(ai, "iron assembly overload trigger") {}
    bool IsActive() override;
};

class IronAssemblyRuneOfPowerTrigger : public Trigger
{
public:
    IronAssemblyRuneOfPowerTrigger(PlayerbotAI* ai) : Trigger(ai, "iron assembly rune of power trigger") {}
    bool IsActive() override;
};

// Hard mode: enforce the "Steelbreaker last" kill order by skull-marking Brundir, then Molgeim.
class IronAssemblyKillOrderTrigger : public Trigger
{
public:
    IronAssemblyKillOrderTrigger(PlayerbotAI* ai) : Trigger(ai, "iron assembly kill order trigger") {}
    bool IsActive() override;
};

// Hard mode: swap the boss off a tank stacking Overwhelming Power once Steelbreaker is empowered
// (last alive).
class IronAssemblyFusionPunchSwapTrigger : public Trigger
{
public:
    IronAssemblyFusionPunchSwapTrigger(PlayerbotAI* ai) : Trigger(ai, "iron assembly fusion punch swap trigger") {}
    bool IsActive() override;
};

#endif
