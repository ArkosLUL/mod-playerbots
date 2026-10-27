#ifndef PLAYERBOTS_RAID_TOCMULTIPLIERS_SHARED_H
#define PLAYERBOTS_RAID_TOCMULTIPLIERS_SHARED_H

#include "RaidBurstWindow.h"

class PlayerbotAI;

// The live encounter's burst row, answered by its stem's ToC<Stem>BurstWindow. Default between pulls.
RaidBurstWindow ToCLiveBurstWindow(PlayerbotAI* botAI);

#endif
