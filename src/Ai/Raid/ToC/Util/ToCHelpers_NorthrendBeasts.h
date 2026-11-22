#ifndef PLAYERBOTS_RAID_TOCHELPERS_NORTHRENDBEASTS_H
#define PLAYERBOTS_RAID_TOCHELPERS_NORTHRENDBEASTS_H

#include <vector>

#include "Define.h"

class Player;
class PlayerbotAI;
class Unit;

namespace TrialOfTheCrusaderHelpers
{

enum class NorthrendBeast : uint8
{
    Gormok,
    Acidmaw,
    Dreadscale,
    Icehowl
};

enum class BeastsTankDuty : uint8
{
    None,
    Gormok,
    Icehowl,
    WormMobile,
    GormokSwap,
    WormStationary
};

enum class BeastsDpsBranch : uint8
{
    None,
    Skull,
    Snobold,
    Gormok,
    Acidmaw,
    MobileWorm,
    Icehowl
};

struct BeastsDpsPick
{
    Unit* unit = nullptr;
    BeastsDpsBranch branch = BeastsDpsBranch::None;
};

inline constexpr uint32 BEASTS_STAGE_GORMOK = 1;
inline constexpr uint32 BEASTS_STAGE_WORMS = 2;
inline constexpr uint32 BEASTS_STAGE_ICEHOWL = 4;

Unit* GetEngagedBeast(PlayerbotAI* botAI, NorthrendBeast beast);

uint32 GetBeastsStageMask(PlayerbotAI* botAI);

bool IsBeastsTank(Player* player);

std::vector<Player*> GetBeastsTankRoster(Player* bot);

BeastsTankDuty GetBeastsTankDuty(PlayerbotAI* botAI);

Player* GetBeastsDutyHolder(PlayerbotAI* botAI, BeastsTankDuty duty);

Unit* GetBeastOfDuty(PlayerbotAI* botAI, BeastsTankDuty duty);

BeastsTankDuty GetDutyOfBeast(PlayerbotAI* botAI, Unit* beast);

BeastsDpsPick GetBeastsDpsTarget(PlayerbotAI* botAI);

}

#endif
