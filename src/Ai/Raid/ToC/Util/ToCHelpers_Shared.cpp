#include "ToCHelpers_Shared.h"

#include <algorithm>
#include <list>

#include "CellImpl.h"
#include "Playerbots.h"
#include "Creature.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Unit.h"

namespace TrialOfTheCrusaderHelpers
{
namespace
{
// AllCreaturesMatchingOneEntryInRange with the set's entry test
class CreaturesOfEntrySetInRange
{
public:
    CreaturesOfEntrySetInRange(WorldObject const* object, ToCEntrySet const& entries, float maxRange)
        : _object(object), _entries(entries), _range(maxRange)
    {
    }

    bool operator()(Unit* unit) const
    {
        return _entries.Contains(unit->GetEntry()) && _object->IsWithinDist(unit, _range, false);
    }

private:
    WorldObject const* _object;
    ToCEntrySet const& _entries;
    float _range;
};
}

ToCEntrySet::ToCEntrySet(std::vector<uint32> const& entries)
{
    if (entries.empty())
        return;

    auto const [low, high] = std::minmax_element(entries.begin(), entries.end());
    base = *low;
    bits.assign(static_cast<size_t>(*high - *low) + 1, false);
    for (uint32 entry : entries)
        bits[entry - base] = true;
}

void GetCreatureListWithEntriesInGrid(WorldObject const* origin, std::vector<Creature*>& creatures,
                                      ToCEntrySet const& entries, float radius)
{
    CreaturesOfEntrySetInRange check(origin, entries, radius);
    Acore::CreatureListSearcher<CreaturesOfEntrySetInRange> searcher(origin, creatures, check);
    Cell::VisitObjects(origin, searcher, radius);
}

Unit* GetNearestCreatureByEntry(Player* bot, uint32 entry, float radius)
{
    if (!bot)
        return nullptr;

    std::list<Creature*> creatures;
    bot->GetCreatureListWithEntryInGrid(creatures, entry, radius);

    Unit* nearest = nullptr;
    float nearestDist = radius;
    for (Creature* creature : creatures)
    {
        if (!creature->IsAlive())
            continue;

        float const dist = bot->GetExactDist2d(creature);
        if (!nearest || dist < nearestDist)
        {
            nearest = creature;
            nearestDist = dist;
        }
    }

    return nearest;
}

bool GetCreatureClusterCenter(Player* bot, uint32 entry, float radius, Position& center)
{
    if (!bot)
        return false;

    std::list<Creature*> creatures;
    bot->GetCreatureListWithEntryInGrid(creatures, entry, radius);

    float sumX = 0.0f;
    float sumY = 0.0f;
    float sumZ = 0.0f;
    uint32 count = 0;
    for (Creature* creature : creatures)
    {
        if (!creature->IsAlive())
            continue;

        sumX += creature->GetPositionX();
        sumY += creature->GetPositionY();
        sumZ += creature->GetPositionZ();
        ++count;
    }

    if (!count)
        return false;

    center.Relocate(sumX / count, sumY / count, sumZ / count);
    return true;
}

}
