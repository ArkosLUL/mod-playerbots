#ifndef PLAYERBOTS_RAID_TOCTRIGGERS_GORMOK_H
#define PLAYERBOTS_RAID_TOCTRIGGERS_GORMOK_H

#include <vector>

#include "NamedObjectContext.h"
#include "Trigger.h"

class GormokTankDutyTrigger : public Trigger
{
public:
    GormokTankDutyTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "gormok tank duty") {}
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

class GormokTankDefensiveTrigger : public Trigger
{
public:
    GormokTankDefensiveTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "gormok tank defensive") {}
    bool IsActive() override;
};

class GormokSnobolledTrigger : public Trigger
{
public:
    GormokSnobolledTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "gormok snobolled") {}
    bool IsActive() override;
};

class GormokStompRangeTrigger : public Trigger
{
public:
    GormokStompRangeTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "gormok stomp range") {}
    bool IsActive() override;
};

class GormokFireBombIncomingTrigger : public Trigger
{
public:
    GormokFireBombIncomingTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "gormok fire bomb incoming") {}
    bool IsActive() override;
};

class ToCGormokTriggerContext : public NamedObjectContext<Trigger>
{
public:
    ToCGormokTriggerContext() : NamedObjectContext<Trigger>()
    {
        creators["gormok tank duty"] =
            &ToCGormokTriggerContext::gormok_tank_duty;
        creators["gormok snobold on raid"] =
            &ToCGormokTriggerContext::gormok_snobold_on_raid;
        creators["gormok tank swap needed"] =
            &ToCGormokTriggerContext::gormok_tank_swap_needed;
        creators["gormok tank defensive"] =
            &ToCGormokTriggerContext::gormok_tank_defensive;
        creators["gormok snobolled"] =
            &ToCGormokTriggerContext::gormok_snobolled;
        creators["gormok stomp range"] =
            &ToCGormokTriggerContext::gormok_stomp_range;
        creators["gormok fire bomb incoming"] =
            &ToCGormokTriggerContext::gormok_fire_bomb_incoming;
    }

private:
    static Trigger* gormok_tank_duty(PlayerbotAI* botAI) {
        return new GormokTankDutyTrigger(botAI);
    }

    static Trigger* gormok_snobold_on_raid(PlayerbotAI* botAI) {
        return new GormokSnoboldOnRaidTrigger(botAI);
    }

    static Trigger* gormok_tank_swap_needed(PlayerbotAI* botAI) {
        return new GormokTankSwapNeededTrigger(botAI);
    }

    static Trigger* gormok_tank_defensive(PlayerbotAI* botAI) {
        return new GormokTankDefensiveTrigger(botAI);
    }

    static Trigger* gormok_snobolled(PlayerbotAI* botAI) {
        return new GormokSnobolledTrigger(botAI);
    }

    static Trigger* gormok_stomp_range(PlayerbotAI* botAI) {
        return new GormokStompRangeTrigger(botAI);
    }

    static Trigger* gormok_fire_bomb_incoming(PlayerbotAI* botAI) {
        return new GormokFireBombIncomingTrigger(botAI);
    }
};

void AddToCGormokTriggerNodes(std::vector<TriggerNode*>& triggers);

#endif
