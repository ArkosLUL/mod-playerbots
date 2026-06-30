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

// Anub'arak

class AnubarakEngagedByMainTankTrigger : public Trigger
{
public:
    AnubarakEngagedByMainTankTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "anubarak engaged by main tank") {}
    bool IsActive() override;
};

class AnubarakBurrowerNeedsAssistTankTrigger : public Trigger
{
public:
    AnubarakBurrowerNeedsAssistTankTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "anubarak burrower needs assist tank") {}
    bool IsActive() override;
};

class AnubarakBurrowerShouldBeFocusedTrigger : public Trigger
{
public:
    AnubarakBurrowerShouldBeFocusedTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "anubarak burrower should be focused") {}
    bool IsActive() override;
};

class AnubarakScarabOnRaidTrigger : public Trigger
{
public:
    AnubarakScarabOnRaidTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "anubarak scarab on raid") {}
    bool IsActive() override;
};

class AnubarakPursuedBySpikeTrigger : public Trigger
{
public:
    AnubarakPursuedBySpikeTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "anubarak pursued by spike") {}
    bool IsActive() override;
};

class AnubarakRangedShouldSeedPermafrostTrigger : public Trigger
{
public:
    AnubarakRangedShouldSeedPermafrostTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "anubarak ranged should seed permafrost") {}
    bool IsActive() override;
};

// Faction Champions

class FactionChampionsShouldFocusTrigger : public Trigger
{
public:
    FactionChampionsShouldFocusTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "faction champions should focus") {}
    bool IsActive() override;
};

// Twin Val'kyr

class TwinValkyrEngagedByMainTankTrigger : public Trigger
{
public:
    TwinValkyrEngagedByMainTankTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "twin valkyr engaged by main tank") {}
    bool IsActive() override;
};

class TwinValkyrDarkbaneNeedsAssistTankTrigger : public Trigger
{
public:
    TwinValkyrDarkbaneNeedsAssistTankTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "twin valkyr darkbane needs assist tank") {}
    bool IsActive() override;
};

class TwinValkyrVortexRequiresEssenceTrigger : public Trigger
{
public:
    TwinValkyrVortexRequiresEssenceTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "twin valkyr vortex requires essence") {}
    bool IsActive() override;
};

class TwinValkyrTouchedRequiresEssenceTrigger : public Trigger
{
public:
    TwinValkyrTouchedRequiresEssenceTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "twin valkyr touched requires essence") {}
    bool IsActive() override;
};

class TwinValkyrNeedsInitialEssenceTrigger : public Trigger
{
public:
    TwinValkyrNeedsInitialEssenceTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "twin valkyr needs initial essence") {}
    bool IsActive() override;
};

#endif
