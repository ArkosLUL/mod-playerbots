#ifndef PLAYERBOTS_ULDTRIGGERS_IRONASSEMBLY_H
#define PLAYERBOTS_ULDTRIGGERS_IRONASSEMBLY_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "Trigger.h"

//
// Iron Assembly
//
// Killing one member restores the other two to full, so focus fire is the encounter and everything
// else is keeping people alive long enough to finish it.
//

// All three back at full health, which only happens on a fresh pull.
class IronAssemblyResetEncounterStateTrigger : public Trigger
{
public:
    IronAssemblyResetEncounterStateTrigger(PlayerbotAI* ai)
        : Trigger(ai, "iron assembly reset encounter state trigger")
    {
    }
    bool IsActive() override;
};

// Overwhelming Power is not dispellable, so the carrier dies either way. Walking them out is what
// stops Meltdown's 15 yd blast taking the melee with them - and every death it causes is another
// permanent +25% on Steelbreaker.
class IronAssemblyOverwhelmingPowerRunOutTrigger : public Trigger
{
public:
    IronAssemblyOverwhelmingPowerRunOutTrigger(PlayerbotAI* ai)
        : Trigger(ai, "iron assembly overwhelming power run out trigger")
    {
    }
    bool IsActive() override;
};

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

// 200 ms rather than every tick: the rune lives 30s on a 30-40s cadence, so a fraction of a second
// late costs one tick of damage and the sweep is a grid search across the whole raid.
class IronAssemblyRuneOfDeathTrigger : public Trigger
{
public:
    IronAssemblyRuneOfDeathTrigger(PlayerbotAI* ai) : Trigger(ai, "iron assembly rune of death trigger", 200) {}
    bool IsActive() override;
};

// Brundir is the only member that can be interrupted or stunned at all; Steelbreaker and Molgeim are
// immune to both.
class IronAssemblyInterruptTrigger : public Trigger
{
public:
    IronAssemblyInterruptTrigger(PlayerbotAI* ai) : Trigger(ai, "iron assembly interrupt trigger") {}
    bool IsActive() override;
};

// One tank per living member, Brundir's spot first.
class IronAssemblyTankAssignmentTrigger : public Trigger
{
public:
    IronAssemblyTankAssignmentTrigger(PlayerbotAI* ai) : Trigger(ai, "iron assembly tank assignment trigger") {}
    bool IsActive() override;
};

// Swap the empowered Steelbreaker off the tank carrying Overwhelming Power. Deliberately not driven
// off Fusion Punch: that is a short, frequently recast DoT and swapping on it would ping-pong the
// boss between the two tanks on every cast.
class IronAssemblyOverwhelmingPowerSwapTrigger : public Trigger
{
public:
    IronAssemblyOverwhelmingPowerSwapTrigger(PlayerbotAI* ai)
        : Trigger(ai, "iron assembly overwhelming power swap trigger")
    {
    }
    bool IsActive() override;
};

// Shield of Runes pays Molgeim +50% damage for 15s if it is drained rather than removed, so
// stripping it is worth more than the global costs.
class IronAssemblyShieldOfRunesTrigger : public Trigger
{
public:
    IronAssemblyShieldOfRunesTrigger(PlayerbotAI* ai) : Trigger(ai, "iron assembly shield of runes trigger") {}
    bool IsActive() override;
};

class IronAssemblyFusionPunchDispelTrigger : public Trigger
{
public:
    IronAssemblyFusionPunchDispelTrigger(PlayerbotAI* ai) : Trigger(ai, "iron assembly fusion punch dispel trigger") {}
    bool IsActive() override;
};

class IronAssemblyRedirectThreatTrigger : public Trigger
{
public:
    IronAssemblyRedirectThreatTrigger(PlayerbotAI* ai) : Trigger(ai, "iron assembly redirect threat trigger") {}
    bool IsActive() override;
};

// The tank half of Rune of Power: walk the boss off the rune so he does not get the +50% too.
class IronAssemblyRuneOfPowerTrigger : public Trigger
{
public:
    IronAssemblyRuneOfPowerTrigger(PlayerbotAI* ai) : Trigger(ai, "iron assembly rune of power trigger") {}
    bool IsActive() override;
};

// The raid half: ranged and healers walk into it. Melee lose it when the tank pulls the boss out,
// which is the accepted cost of denying the boss the same buff.
class IronAssemblyRuneOfPowerSoakTrigger : public Trigger
{
public:
    IronAssemblyRuneOfPowerSoakTrigger(PlayerbotAI* ai) : Trigger(ai, "iron assembly rune of power soak trigger") {}
    bool IsActive() override;
};

class IronAssemblySetDpsPriorityTrigger : public Trigger
{
public:
    IronAssemblySetDpsPriorityTrigger(PlayerbotAI* ai) : Trigger(ai, "iron assembly set dps priority trigger") {}
    bool IsActive() override;
};

class IronAssemblyRaidPositionTrigger : public Trigger
{
public:
    IronAssemblyRaidPositionTrigger(PlayerbotAI* ai) : Trigger(ai, "iron assembly raid position trigger") {}
    bool IsActive() override;
};

#endif
