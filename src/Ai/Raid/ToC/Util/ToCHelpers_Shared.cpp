#include "ToCHelpers_Shared.h"
#include "Playerbots.h"
#include "Creature.h"
#include "Unit.h"

namespace TrialOfTheCrusaderHelpers
{

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
