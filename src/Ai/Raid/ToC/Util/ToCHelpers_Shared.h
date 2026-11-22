#ifndef PLAYERBOTS_RAID_TOCHELPERS_SHARED_H
#define PLAYERBOTS_RAID_TOCHELPERS_SHARED_H

#include <vector>

#include "ObjectGuid.h"
#include "PlayerbotAI.h"
#include "RaidCombatHelpers.h"
#include "RaidDirectTargeting.h"
#include "Timer.h"
#include "ToCData.h"

class Creature;
class Player;
class WorldObject;

namespace TrialOfTheCrusaderHelpers
{

class ToCEntrySet
{
public:
    explicit ToCEntrySet(std::vector<uint32> const& entries);

    bool Contains(uint32 entry) const
    {
        uint32 const offset = entry - _base;
        return offset < _bits.size() && _bits[offset];
    }

private:
    uint32 _base = 0;
    std::vector<bool> _bits;
};

void GetCreatureListWithEntriesInGrid(WorldObject const* origin, std::vector<Creature*>& creatures,
                                      ToCEntrySet const& entries, float radius);

inline constexpr auto& IsToCDpsBot = IsRaidDpsBot;
inline constexpr auto& GetToCHumanSkull = GetRaidHumanSkull;
inline constexpr auto& ToCPetOffPick = RaidPetOffPick;

ObjectGuid ToCGroupGuid(Player* bot);

template <typename T>
class ToCGroupMemo
{
public:
    template <typename Fill>
    T const& Get(Player* bot, Fill&& fill, ObjectGuid const& key = ObjectGuid::Empty) const
    {
        ObjectGuid const group = ToCGroupGuid(bot);
        uint32 const now = getMSTime();
        if (group.IsEmpty() || !_valid || _ms != now || _group != group || _key != key)
        {
            _value = T();
            fill(_value);
            _valid = !group.IsEmpty();
            _ms = now;
            _group = group;
            _key = key;
        }

        return _value;
    }

private:
    mutable bool _valid = false;
    mutable uint32 _ms = 0;
    mutable ObjectGuid _group;
    mutable ObjectGuid _key;
    mutable T _value{};
};

}

#endif
