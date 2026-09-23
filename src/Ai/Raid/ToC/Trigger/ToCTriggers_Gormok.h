#ifndef PLAYERBOTS_RAID_TOCTRIGGERS_GORMOK_H
#define PLAYERBOTS_RAID_TOCTRIGGERS_GORMOK_H

#include <vector>

#include "NamedObjectContext.h"
#include "Trigger.h"

class GormokEngagedByMainTankTrigger : public Trigger
{
public:
    GormokEngagedByMainTankTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "gormok engaged by main tank") {}
    bool IsActive() override;
};

class GormokSnoboldOnRaidTrigger : public Trigger
{
public:
    GormokSnoboldOnRaidTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "gormok snobold on raid") {}
    bool IsActive() override;
};

class GormokTankSwapNeededTrigger : public Trigger
{
public:
    GormokTankSwapNeededTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "gormok tank swap needed") {}
    bool IsActive() override;
};

class ToCGormokTriggerContext : public NamedObjectContext<Trigger>
{
public:
    ToCGormokTriggerContext() : NamedObjectContext<Trigger>()
    {
        creators["gormok engaged by main tank"] =
            &ToCGormokTriggerContext::gormok_engaged_by_main_tank;
        creators["gormok snobold on raid"] =
            &ToCGormokTriggerContext::gormok_snobold_on_raid;
        creators["gormok tank swap needed"] =
            &ToCGormokTriggerContext::gormok_tank_swap_needed;
    }

private:
    static Trigger* gormok_engaged_by_main_tank(PlayerbotAI* botAI) {
        return new GormokEngagedByMainTankTrigger(botAI);
    }

    static Trigger* gormok_snobold_on_raid(PlayerbotAI* botAI) {
        return new GormokSnoboldOnRaidTrigger(botAI);
    }

    static Trigger* gormok_tank_swap_needed(PlayerbotAI* botAI) {
        return new GormokTankSwapNeededTrigger(botAI);
    }
};

void AddToCGormokTriggerNodes(std::vector<TriggerNode*>& triggers);

#endif
