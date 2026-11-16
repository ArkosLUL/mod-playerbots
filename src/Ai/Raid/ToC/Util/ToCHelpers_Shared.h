#ifndef PLAYERBOTS_RAID_TOCHELPERS_SHARED_H
#define PLAYERBOTS_RAID_TOCHELPERS_SHARED_H

#include <vector>

#include "PlayerbotAI.h"
#include "RaidCombatHelpers.h"
#include "RaidDirectTargeting.h"
#include "ToCData.h"

class Creature;
class WorldObject;

namespace TrialOfTheCrusaderHelpers
{

// Fixed entry list with an O(1) membership test
class ToCEntrySet
{
public:
    explicit ToCEntrySet(std::vector<uint32> const& entries);

    bool Contains(uint32 entry) const
    {
        uint32 const offset = entry - base;  // wraps past bits.size() below base
        return offset < bits.size() && bits[offset];
    }

private:
    uint32 base = 0;
    std::vector<bool> bits;
};

// GetCreatureListWithEntryInGrid over an entry list, into a vector: same check, cells, phase test and
// order, without copying the list per call or testing every entry per creature
void GetCreatureListWithEntriesInGrid(WorldObject const* origin, std::vector<Creature*>& creatures,
                                      ToCEntrySet const& entries, float radius);

// The ToC spellings the Anub'arak helpers and triggers still call
inline constexpr auto& IsToCDpsBot = IsRaidDpsBot;
inline constexpr auto& GetToCHumanSkull = GetRaidHumanSkull;
inline constexpr auto& ToCPetOffPick = RaidPetOffPick;

// Nearest alive creature of the given entry within radius of the bot (nullptr if none)
Unit* GetNearestCreatureByEntry(Player* bot, uint32 entry, float radius);

// Average position of all alive creatures of the given entry within radius of the bot.
// Returns false (and leaves center untouched) when none are found. Used to flee away from a
// whole cluster/trail of hazards rather than a single nearest patch.
bool GetCreatureClusterCenter(Player* bot, uint32 entry, float radius, Position& center);

}

#endif
