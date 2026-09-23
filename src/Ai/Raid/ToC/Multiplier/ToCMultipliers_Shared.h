#ifndef PLAYERBOTS_RAID_TOCMULTIPLIERS_SHARED_H
#define PLAYERBOTS_RAID_TOCMULTIPLIERS_SHARED_H

// What one encounter lets through the burst gate right now, answered by its stem's
// ToC<Stem>BurstWindow. Default lets everything through, so only the base tank-hold gate applies.
// Lust is its own flag: one 10-minute raid cooldown, personal cooldowns come back mid-fight.
struct ToCBurstWindow
{
    bool allowAll = true;
    bool allowLust = true;
};

#endif
