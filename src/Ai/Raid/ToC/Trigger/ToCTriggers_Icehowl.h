#ifndef PLAYERBOTS_RAID_TOCTRIGGERS_ICEHOWL_H
#define PLAYERBOTS_RAID_TOCTRIGGERS_ICEHOWL_H

#include <vector>

#include "NamedObjectContext.h"
#include "Trigger.h"

class IcehowlEngagedByMainTankTrigger : public Trigger
{
public:
    IcehowlEngagedByMainTankTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "icehowl engaged by main tank") {}
    bool IsActive() override;
};

class IcehowlChargeIncomingTrigger : public Trigger
{
public:
    IcehowlChargeIncomingTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "icehowl charge incoming") {}
    bool IsActive() override;
};

class ToCIcehowlTriggerContext : public NamedObjectContext<Trigger>
{
public:
    ToCIcehowlTriggerContext() : NamedObjectContext<Trigger>()
    {
        creators["icehowl engaged by main tank"] =
            &ToCIcehowlTriggerContext::icehowl_engaged_by_main_tank;
        creators["icehowl charge incoming"] =
            &ToCIcehowlTriggerContext::icehowl_charge_incoming;
    }

private:
    static Trigger* icehowl_engaged_by_main_tank(PlayerbotAI* botAI) {
        return new IcehowlEngagedByMainTankTrigger(botAI);
    }

    static Trigger* icehowl_charge_incoming(PlayerbotAI* botAI) {
        return new IcehowlChargeIncomingTrigger(botAI);
    }
};

void AddToCIcehowlTriggerNodes(std::vector<TriggerNode*>& triggers);

#endif
