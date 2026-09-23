#ifndef PLAYERBOTS_RAID_TOCMULTIPLIERS_SHARED_H
#define PLAYERBOTS_RAID_TOCMULTIPLIERS_SHARED_H

#include "Define.h"
#include "Multiplier.h"

// What one encounter lets through the burst gate right now, answered by its stem's
// ToC<Stem>BurstWindow. Default lets everything through, so only the base tank-hold gate applies.
// Lust is its own flag: one 10-minute raid cooldown, personal cooldowns come back mid-fight.
struct ToCBurstWindow
{
    bool allowAll = true;
    bool allowLust = true;
};

// Holds burst cooldowns to the live encounter's row. One multiplier for every encounter, so the
// IsBurstCooldownAction early-out runs once per action rather than once per boss.
class ToCBurstWindowMultiplier : public Multiplier
{
public:
    ToCBurstWindowMultiplier(PlayerbotAI* botAI) : Multiplier(botAI, "toc burst window") {}
    float GetValue(Action* action) override;

private:
    uint32 cachedAtMs = 0;
    ToCBurstWindow cachedValue;
};

#endif
