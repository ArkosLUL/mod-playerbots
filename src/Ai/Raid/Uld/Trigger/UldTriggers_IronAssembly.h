#ifndef PLAYERBOTS_ULDTRIGGERS_IRONASSEMBLY_H
#define PLAYERBOTS_ULDTRIGGERS_IRONASSEMBLY_H

#include "EventMap.h"
#include "GenericTriggers.h"
#include "RaidResistAura.h"
#include "Trigger.h"
#include "UldEncounter_IronAssembly.h"

class IronAssemblyResetEncounterStateTrigger : public Trigger
{
public:
    static constexpr char const* Name = "iron assembly reset encounter state trigger";

    IronAssemblyResetEncounterStateTrigger(PlayerbotAI* botAI)
        : Trigger(botAI, Name)
    {
    }
    bool IsActive() override;
};

class IronAssemblyLightningTendrilsTrigger : public Trigger
{
public:
    static constexpr char const* Name = "iron assembly lightning tendrils trigger";

    IronAssemblyLightningTendrilsTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IronAssemblyOverloadTrigger : public Trigger
{
public:
    static constexpr char const* Name = "iron assembly overload trigger";

    IronAssemblyOverloadTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IronAssemblyRuneOfDeathTrigger : public Trigger
{
public:
    static constexpr char const* Name = "iron assembly rune of death trigger";

    IronAssemblyRuneOfDeathTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name, 200) {}
    bool IsActive() override;
};

class IronAssemblyInterruptTrigger : public Trigger
{
public:
    static constexpr char const* Name = "iron assembly interrupt trigger";

    IronAssemblyInterruptTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IronAssemblyTankAssignmentTrigger : public Trigger
{
public:
    static constexpr char const* Name = "iron assembly tank assignment trigger";

    IronAssemblyTankAssignmentTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IronAssemblyShieldOfRunesTrigger : public Trigger
{
public:
    static constexpr char const* Name = "iron assembly shield of runes trigger";

    IronAssemblyShieldOfRunesTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IronAssemblyFusionPunchDispelTrigger : public Trigger
{
public:
    static constexpr char const* Name = "iron assembly fusion punch dispel trigger";

    IronAssemblyFusionPunchDispelTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IronAssemblyRedirectThreatTrigger : public Trigger
{
public:
    static constexpr char const* Name = "iron assembly redirect threat trigger";

    IronAssemblyRedirectThreatTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IronAssemblyRuneOfPowerMeleeTrigger : public Trigger
{
public:
    static constexpr char const* Name = "iron assembly rune of power melee trigger";

    IronAssemblyRuneOfPowerMeleeTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IronAssemblyMeltdownRunOutTrigger : public Trigger
{
public:
    static constexpr char const* Name = "iron assembly meltdown run out trigger";

    IronAssemblyMeltdownRunOutTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IronAssemblyMeltdownStepOutTrigger : public Trigger
{
public:
    static constexpr char const* Name = "iron assembly meltdown step out trigger";

    IronAssemblyMeltdownStepOutTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IronAssemblyTankBattleRezTrigger : public Trigger
{
public:
    static constexpr char const* Name = "iron assembly tank battle rez trigger";

    IronAssemblyTankBattleRezTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IronAssemblyTankRebuffTrigger : public Trigger
{
public:
    static constexpr char const* Name = "iron assembly tank rebuff trigger";

    IronAssemblyTankRebuffTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IronAssemblyFusionPunchDefensiveTrigger : public Trigger
{
public:
    static constexpr char const* Name = "iron assembly fusion punch defensive trigger";

    IronAssemblyFusionPunchDefensiveTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IronAssemblyNatureAspectTrigger : public RaidResistAuraTrigger
{
public:
    static constexpr char const* Name = "iron assembly nature aspect trigger";

    IronAssemblyNatureAspectTrigger(PlayerbotAI* botAI)
        : RaidResistAuraTrigger(botAI, Name, ULD_IRON_ASSEMBLY_RESIST_AURA)
    {
    }
};

class IronAssemblyNatureTotemTrigger : public Trigger
{
public:
    static constexpr char const* Name = "iron assembly nature totem trigger";

    IronAssemblyNatureTotemTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IronAssemblyUnheldTauntTrigger : public Trigger
{
public:
    static constexpr char const* Name = "iron assembly unheld taunt trigger";

    IronAssemblyUnheldTauntTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IronAssemblySetDpsPriorityTrigger : public Trigger
{
public:
    static constexpr char const* Name = "iron assembly set dps priority trigger";

    IronAssemblySetDpsPriorityTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

class IronAssemblyRaidPositionTrigger : public Trigger
{
public:
    static constexpr char const* Name = "iron assembly raid position trigger";

    IronAssemblyRaidPositionTrigger(PlayerbotAI* botAI) : Trigger(botAI, Name) {}
    bool IsActive() override;
};

#endif
