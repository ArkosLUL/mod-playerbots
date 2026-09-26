#ifndef PLAYERBOTS_RAID_TOCHELPERS_NORTHRENDBEASTS_H
#define PLAYERBOTS_RAID_TOCHELPERS_NORTHRENDBEASTS_H

#include "Define.h"

class Player;
class PlayerbotAI;
class Unit;

// Which beasts are up and which tank holds each. Heroic brings the next beast in on a timer, so a
// slow stage overlaps the next one. Read from the world once per instance per ms, so every bot gets
// the same answer. Nothing resolves off map 649.
namespace TrialOfTheCrusaderHelpers
{

enum class NorthrendBeast : uint8
{
    Gormok,
    Acidmaw,
    Dreadscale,
    Icehowl
};

// Deal order after None is the priority order: a duty with no tank left takes the one holding the
// lowest-priority duty below it.
enum class BeastsTankDuty : uint8
{
    None,
    Gormok,
    Icehowl,
    WormMobile,
    GormokSwap,
    WormStationary
};

// GetBeastsStageMask bits, also the nb.stage value
constexpr uint32 BEASTS_STAGE_GORMOK = 1;
constexpr uint32 BEASTS_STAGE_WORMS = 2;
constexpr uint32 BEASTS_STAGE_ICEHOWL = 4;

// Alive and in combat, else nullptr. A beast still walking in isn't engaged.
Unit* GetEngagedBeast(PlayerbotAI* botAI, NorthrendBeast beast);

uint32 GetBeastsStageMask(PlayerbotAI* botAI);

// Bot or human, by strategy or by spec
bool IsBeastsTank(Player* player);

// None for a tank the deal left out and for every non-tank
BeastsTankDuty GetBeastsTankDuty(PlayerbotAI* botAI);

// Null while the duty's beast isn't engaged or no tank was left for it. Can be a human, but only for
// a beast he's already tanking.
Player* GetBeastsDutyHolder(PlayerbotAI* botAI, BeastsTankDuty duty);

// GormokSwap answers Gormok. The worm duties follow the form, so their beast changes when the worms
// swap.
Unit* GetBeastOfDuty(PlayerbotAI* botAI, BeastsTankDuty duty);

// The main duty on that beast, so Gormok never answers GormokSwap. None for anything that isn't an
// engaged beast, null included.
BeastsTankDuty GetDutyOfBeast(PlayerbotAI* botAI, Unit* beast);

}

#endif
