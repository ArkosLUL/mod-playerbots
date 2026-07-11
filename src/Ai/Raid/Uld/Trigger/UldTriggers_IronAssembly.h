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

#endif
