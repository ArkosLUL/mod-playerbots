#ifndef PLAYERBOTS_RAID_TOCMULTIPLIERS_FACTIONCHAMPIONSDEFENCE_H
#define PLAYERBOTS_RAID_TOCMULTIPLIERS_FACTIONCHAMPIONSDEFENCE_H

#include <vector>

#include "Multiplier.h"

// Melee hold rather than walk into a Bladestorm or Hellfire around their target, the dodge would only
// walk them back out
class FactionChampionsAoeGuardMultiplier : public Multiplier
{
public:
    FactionChampionsAoeGuardMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "faction champions aoe guard multiplier") {}
    float GetValue(Action* action) override;
};

// The focus node would pull physical attackers back onto a Hand of Protection kill target
class FactionChampionsPhysicalSwitchGuardMultiplier : public Multiplier
{
public:
    FactionChampionsPhysicalSwitchGuardMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "faction champions physical switch guard multiplier") {}
    float GetValue(Action* action) override;
};

// Dispelling Unstable Affliction silences the dispeller and hits it hard, and a dispel picks its aura at
// random, so no magic dispel on its carrier
class FactionChampionsDispelGuardMultiplier : public Multiplier
{
public:
    FactionChampionsDispelGuardMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "faction champions dispel guard multiplier") {}
    float GetValue(Action* action) override;
};

// Class purge and spellsteal fire on any magic buff, Thorns included, so one duty bot purges instead
class FactionChampionsPurgeGuardMultiplier : public Multiplier
{
public:
    FactionChampionsPurgeGuardMultiplier(
        PlayerbotAI* botAI) : Multiplier(botAI, "faction champions purge guard multiplier") {}
    float GetValue(Action* action) override;
};

void AddToCFactionChampionsDefenceMultipliers(PlayerbotAI* botAI, std::vector<Multiplier*>& multipliers);

#endif
