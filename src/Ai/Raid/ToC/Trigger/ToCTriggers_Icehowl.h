#ifndef PLAYERBOTS_RAID_TOCTRIGGERS_ICEHOWL_H
#define PLAYERBOTS_RAID_TOCTRIGGERS_ICEHOWL_H

#include <vector>

#include "NamedObjectContext.h"
#include "Trigger.h"

class IcehowlTankDutyTrigger : public Trigger
{
public:
    IcehowlTankDutyTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "icehowl tank duty") {}
    bool IsActive() override;
};

class IcehowlChargeIncomingTrigger : public Trigger
{
public:
    IcehowlChargeIncomingTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "icehowl charge incoming") {}
    bool IsActive() override;
};

class IcehowlFrothingRageTrigger : public Trigger
{
public:
    IcehowlFrothingRageTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "icehowl frothing rage") {}
    bool IsActive() override;
};

class ToCIcehowlTriggerContext : public NamedObjectContext<Trigger>
{
public:
    ToCIcehowlTriggerContext() : NamedObjectContext<Trigger>()
    {
        creators["icehowl tank duty"] =
            &ToCIcehowlTriggerContext::icehowl_tank_duty;
        creators["icehowl charge incoming"] =
            &ToCIcehowlTriggerContext::icehowl_charge_incoming;
        creators["icehowl frothing rage"] =
            &ToCIcehowlTriggerContext::icehowl_frothing_rage;
    }

private:
    static Trigger* icehowl_tank_duty(PlayerbotAI* botAI) {
        return new IcehowlTankDutyTrigger(botAI);
    }

    static Trigger* icehowl_charge_incoming(PlayerbotAI* botAI) {
        return new IcehowlChargeIncomingTrigger(botAI);
    }

    static Trigger* icehowl_frothing_rage(PlayerbotAI* botAI) {
        return new IcehowlFrothingRageTrigger(botAI);
    }
};

void AddToCIcehowlTriggerNodes(std::vector<TriggerNode*>& triggers);

#endif
