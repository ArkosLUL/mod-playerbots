#ifndef PLAYERBOTS_TOCMULTIPLIERS_H
#define PLAYERBOTS_TOCMULTIPLIERS_H

#include "Multiplier.h"

// Make sure clearing Icehowl's charge lane overrides formation/avoidance/chase movement
class IcehowlSuppressMovementDuringChargeMultiplier : public Multiplier
{
public:
    IcehowlSuppressMovementDuringChargeMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "icehowl suppress movement during charge multiplier") {}
    float GetValue(Action* action) override;
};

// Keep beast tanks anchored instead of drifting with the combat formation
class NorthrendBeastsControlTankMovementMultiplier : public Multiplier
{
public:
    NorthrendBeastsControlTankMovementMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "northrend beasts control tank movement multiplier") {}
    float GetValue(Action* action) override;
};

// Keep tanks anchored on Jaraxxus and his adds instead of drifting with the combat formation
class JaraxxusControlTankMovementMultiplier : public Multiplier
{
public:
    JaraxxusControlTankMovementMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "jaraxxus control tank movement multiplier") {}
    float GetValue(Action* action) override;
};

// Keep tanks anchored on Anub'arak and his burrowers instead of drifting with the combat formation
class AnubarakControlTankMovementMultiplier : public Multiplier
{
public:
    AnubarakControlTankMovementMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "anubarak control tank movement multiplier") {}
    float GetValue(Action* action) override;
};

// While this bot is the spike-chase target, suppress every other movement so nothing competes with
// the kite to Permafrost
class AnubarakProtectSpikeKiteMultiplier : public Multiplier
{
public:
    AnubarakProtectSpikeKiteMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "anubarak protect spike kite multiplier") {}
    float GetValue(Action* action) override;
};

// Hold Bloodlust/Heroism until the phase 3 Leeching Swarm burn
class AnubarakDelayBloodlustUntilLeechingSwarmMultiplier : public Multiplier
{
public:
    AnubarakDelayBloodlustUntilLeechingSwarmMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "anubarak delay bloodlust until leeching swarm multiplier") {}
    float GetValue(Action* action) override;
};

// Suppress AoE damage during Faction Champions so they never gain the stacking Anti-AoE mitigation
class FactionChampionsSuppressAoeMultiplier : public Multiplier
{
public:
    FactionChampionsSuppressAoeMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "faction champions suppress aoe multiplier") {}
    float GetValue(Action* action) override;
};

// Keep the twin tanks anchored on Fjola/Eydis instead of drifting with the combat formation
class TwinValkyrControlTankMovementMultiplier : public Multiplier
{
public:
    TwinValkyrControlTankMovementMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "twin valkyr control tank movement multiplier") {}
    float GetValue(Action* action) override;
};

// While a non-tank bot must swap essence (vortex/touch colour mismatch), suppress every other movement
// so nothing competes with the run to the portal
class TwinValkyrPrioritizeEssenceSwapMultiplier : public Multiplier
{
public:
    TwinValkyrPrioritizeEssenceSwapMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "twin valkyr prioritize essence swap multiplier") {}
    float GetValue(Action* action) override;
};

#endif
