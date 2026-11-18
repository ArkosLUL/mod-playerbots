#ifndef PLAYERBOTS_RAID_TOCTRIGGERS_NORTHRENDBEASTS_H
#define PLAYERBOTS_RAID_TOCTRIGGERS_NORTHRENDBEASTS_H

#include <string>

#include "Define.h"
#include "Trigger.h"

enum class NorthrendBeastsStem : uint8
{
    Gormok,  // snobolds too
    Worms,
    Icehowl
};

// One per stem, firing only for the stem's own beasts, so the three never act on one pick.
class NorthrendBeastsDpsTargetTrigger : public Trigger
{
public:
    NorthrendBeastsDpsTargetTrigger(PlayerbotAI* botAI, std::string const name, NorthrendBeastsStem ownStem)
        : Trigger(botAI, name), _stem(ownStem) {}
    bool IsActive() override;

private:
    NorthrendBeastsStem const _stem;
};

#endif
