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
        uint32 const offset = entry - _base;  // wraps past _bits.size() below _base
        return offset < _bits.size() && _bits[offset];
    }

private:
    uint32 _base = 0;
    std::vector<bool> _bits;
};

// GetCreatureListWithEntryInGrid over an entry list, into a vector: same check, cells, phase test and
// order, without copying the list per call or testing every entry per creature
void GetCreatureListWithEntriesInGrid(WorldObject const* origin, std::vector<Creature*>& creatures,
                                      ToCEntrySet const& entries, float radius);

// The ToC spellings the Anub'arak helpers and triggers call
inline constexpr auto& IsToCDpsBot = IsRaidDpsBot;
inline constexpr auto& GetToCHumanSkull = GetRaidHumanSkull;
inline constexpr auto& ToCPetOffPick = RaidPetOffPick;

// Nearest alive creature of the given entry within radius of the bot (nullptr if none)
Unit* GetNearestCreatureByEntry(Player* bot, uint32 entry, float radius);

// Average position of the alive creatures of the entry within radius of the bot, to flee a whole
// trail of hazards. False, center untouched, when there are none.
bool GetCreatureClusterCenter(Player* bot, uint32 entry, float radius, Position& center);

}

#endif
