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

// Lord Jaraxxus

class JaraxxusEngagedByMainTankTrigger : public Trigger
{
public:
    JaraxxusEngagedByMainTankTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "jaraxxus engaged by main tank") {}
    bool IsActive() override;
};

class JaraxxusAddNeedsAssistTankTrigger : public Trigger
{
public:
    JaraxxusAddNeedsAssistTankTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "jaraxxus add needs assist tank") {}
    bool IsActive() override;
};

class JaraxxusSecondAddNeedsAssistTankTrigger : public Trigger
{
public:
    JaraxxusSecondAddNeedsAssistTankTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "jaraxxus second add needs assist tank") {}
    bool IsActive() override;
};

class JaraxxusAddShouldBeFocusedTrigger : public Trigger
{
public:
    JaraxxusAddShouldBeFocusedTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "jaraxxus add should be focused") {}
    bool IsActive() override;
};

class JaraxxusLegionFlameNearbyTrigger : public Trigger
{
public:
    JaraxxusLegionFlameNearbyTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "jaraxxus legion flame nearby") {}
    bool IsActive() override;
};

class JaraxxusIncinerateFleshOnRaidTrigger : public Trigger
{
public:
    JaraxxusIncinerateFleshOnRaidTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "jaraxxus incinerate flesh on raid") {}
    bool IsActive() override;
};

class JaraxxusNetherPowerActiveTrigger : public Trigger
{
public:
    JaraxxusNetherPowerActiveTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "jaraxxus nether power active") {}
    bool IsActive() override;
};

class JaraxxusFelFireballInterruptibleTrigger : public Trigger
{
public:
    JaraxxusFelFireballInterruptibleTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "jaraxxus fel fireball interruptible") {}
    bool IsActive() override;
};

#endif
