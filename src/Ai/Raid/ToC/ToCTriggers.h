#ifndef PLAYERBOTS_TOCTRIGGERS_H
#define PLAYERBOTS_TOCTRIGGERS_H

#include "Trigger.h"

// Gormok the Impaler

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

// Acidmaw & Dreadscale

class WormsMobileEngagedByMainTankTrigger : public Trigger
{
public:
    WormsMobileEngagedByMainTankTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "northrend worms mobile engaged by main tank") {}
    bool IsActive() override;
};

class WormsStationaryNeedsAssistTankTrigger : public Trigger
{
public:
    WormsStationaryNeedsAssistTankTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "northrend worms stationary needs assist tank") {}
    bool IsActive() override;
};

class WormsRangedShouldSpreadTrigger : public Trigger
{
public:
    WormsRangedShouldSpreadTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "northrend worms ranged should spread") {}
    bool IsActive() override;
};

class WormsAfflictedByBurningTrigger : public Trigger
{
public:
    WormsAfflictedByBurningTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "northrend worms afflicted by burning") {}
    bool IsActive() override;
};

// Icehowl

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

#endif
