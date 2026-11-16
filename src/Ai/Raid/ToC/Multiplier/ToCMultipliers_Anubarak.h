#ifndef PLAYERBOTS_RAID_TOCMULTIPLIERS_ANUBARAK_H
#define PLAYERBOTS_RAID_TOCMULTIPLIERS_ANUBARAK_H

#include <unordered_map>
#include <vector>

#include "Multiplier.h"
#include "RaidBurstWindow.h"
#include "RaidCombatHelpers.h"
#include "RaidSplash.h"

// Keep tanks anchored on Anub'arak and his burrowers instead of drifting with the combat formation
class AnubarakControlTankMovementMultiplier : public Multiplier
{
public:
    AnubarakControlTankMovementMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "anubarak control tank movement multiplier") {}
    float GetValue(Action* action) override;
};

// The spike's target moves only through the kite, and never feigns death. Attack actions stay so it
// keeps a target.
class AnubarakProtectSpikeKiteMultiplier : public Multiplier
{
public:
    AnubarakProtectSpikeKiteMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "anubarak protect spike kite multiplier") {}
    float GetValue(Action* action) override;

private:
    // Fixed by the action's type and name, so worked out once per action
    static bool HeldWhileMarked(Action* action);

    RaidActionCache<bool> held;
};

// A DPS bot's damage on him, and its area damage within 30 yd of him, while its threat cap latch holds.
// Heals and anything aimed elsewhere go through. Wired from the definition.
class AnubarakThreatCapMultiplier : public Multiplier
{
public:
    AnubarakThreatCapMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "anubarak threat cap") {}
    float GetValue(Action* action) override;

private:
    RaidSplashCache splash;
};

// Nothing aimed at a Frost Sphere but the bot's own duty sphere, and on that only direct damage: no
// crowd control, DoT, channel or area spell. Heroic: no Starfall at all. Other area spells have no
// target and stay unguarded. Wired from the definition.
class AnubarakSphereGuardMultiplier : public Multiplier
{
public:
    AnubarakSphereGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "anubarak sphere guard") {}
    float GetValue(Action* action) override;

private:
    // An action's name never changes, so neither do these
    struct Names
    {
        bool starfall = false;
        bool crowdControl = false;
    };

    RaidSplashCache splash;
    RaidActionCache<Names> names;
};

// A non-tank's damage on a burrower only on its own dps pick, so a multidot doesn't pull a loose one
// onto a raider, and no area damage within 10 yd of the bot or its target while a burrower there is
// still settling on its side tank, or, for melee, is one it has ANUBARAK_MELEE_AREA_CAP of the tank's
// threat on. Interrupts and stuns on a casting one go through; a taunt or Death Grip never. A tank
// holding no side holds its area damage with any burrower within 10 yd. No Bladestorm with a burrower
// within 25 yd. Wired from the definition.
class AnubarakBurrowerGuardMultiplier : public Multiplier
{
public:
    AnubarakBurrowerGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "anubarak burrower guard") {}
    float GetValue(Action* action) override;

private:
    // An action's name never changes, so neither do these
    struct Names
    {
        bool bladestorm = false;
        bool mover = false;
    };

    RaidSplashCache splash;
    RaidActionCache<Names> names;
};

// A DPS bot's damage on a burrower it has 90% of the side tank's threat on, until under 80%, and its
// area damage within 10 yd of the bot or its target with one such burrower there. Heals pass, and white
// swings go on. Wired from the definition.
class AnubarakBurrowerThreatCapMultiplier : public Multiplier
{
public:
    AnubarakBurrowerThreatCapMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "anubarak burrower threat cap") {}
    float GetValue(Action* action) override;

private:
    RaidSplashCache splash;
};

// A side tank's area and chain spells only where AnubarakSideTankAreaAllowed passes them: none of the
// other side's burrowers hit, him only under the side tank's cap. Wired from the definition.
class AnubarakSideTankAreaGuardMultiplier : public Multiplier
{
public:
    AnubarakSideTankAreaGuardMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "anubarak side tank area guard") {}
    float GetValue(Action* action) override;

private:
    RaidSplashCache splash;
};

// Heroic phase 3: no heal that only feeds Leeching Swarm, on any non-tank without Penetrating Cold, the
// caster itself too, and no area heal. Wired from the definition.
class AnubarakSwarmHealMultiplier : public Multiplier
{
public:
    AnubarakSwarmHealMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "anubarak swarm heal") {}
    float GetValue(Action* action) override;

private:
    std::unordered_map<Action*, bool> heals;
};

void AddToCAnubarakMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers);

RaidBurstWindow ToCAnubarakBurstWindow(PlayerbotAI* botAI);

#endif
