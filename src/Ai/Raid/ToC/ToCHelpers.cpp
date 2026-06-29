#include "ToCHelpers.h"
#include "Playerbots.h"
#include "RaidBossHelpers.h"
#include "Creature.h"
#include "Unit.h"

namespace TrialOfTheCrusaderHelpers
{

const Position ARENA_CENTER = { 563.672974f, 139.571f, 393.837006f };

bool IsWormMobile(Unit* worm)
{
    if (!worm)
        return false;

    uint32 const displayId = worm->GetDisplayId();
    return displayId == static_cast<uint32>(ToCDisplayIds::MODEL_ACIDMAW_MOBILE) ||
           displayId == static_cast<uint32>(ToCDisplayIds::MODEL_DREADSCALE_MOBILE);
}

bool IsBotInChargeCorridor(Player* bot, Unit* icehowl, float halfWidth)
{
    if (!bot || !icehowl)
        return false;

    float const orientation = icehowl->GetOrientation();
    float const dirX = std::cos(orientation);
    float const dirY = std::sin(orientation);

    float const relX = bot->GetPositionX() - icehowl->GetPositionX();
    float const relY = bot->GetPositionY() - icehowl->GetPositionY();

    // Distance along the charge direction (in front of Icehowl) and perpendicular offset from the lane
    float const along = relX * dirX + relY * dirY;
    float const perpendicular = std::fabs(relX * dirY - relY * dirX);

    return along > -5.0f && along < 120.0f && perpendicular < halfWidth;
}

bool HasMassiveCrashAura(Player* bot)
{
    if (!bot)
        return false;

    return bot->HasAura(static_cast<uint32>(ToCSpells::SPELL_MASSIVE_CRASH_10N)) ||
           bot->HasAura(static_cast<uint32>(ToCSpells::SPELL_MASSIVE_CRASH_25N)) ||
           bot->HasAura(static_cast<uint32>(ToCSpells::SPELL_MASSIVE_CRASH_10H)) ||
           bot->HasAura(static_cast<uint32>(ToCSpells::SPELL_MASSIVE_CRASH_25H));
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

bool JaraxxusHasNetherPower(Unit* jaraxxus)
{
    if (!jaraxxus)
        return false;

    return jaraxxus->HasAura(static_cast<uint32>(ToCSpells::SPELL_NETHER_POWER_10N)) ||
           jaraxxus->HasAura(static_cast<uint32>(ToCSpells::SPELL_NETHER_POWER_10H)) ||
           jaraxxus->HasAura(static_cast<uint32>(ToCSpells::SPELL_NETHER_POWER_25N)) ||
           jaraxxus->HasAura(static_cast<uint32>(ToCSpells::SPELL_NETHER_POWER_25H));
}

Unit* GetPriorityJaraxxusAdd(PlayerbotAI* botAI)
{
    if (Unit* mistress = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_MISTRESS_OF_PAIN)))
        return mistress;

    return GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_FEL_INFERNAL));
}

Unit* GetSecondaryJaraxxusAdd(PlayerbotAI* botAI)
{
    Unit* mistress = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_MISTRESS_OF_PAIN));
    Unit* infernal = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_FEL_INFERNAL));

    // Only meaningful when both adds are up; the priority add (Mistress) is held elsewhere
    if (mistress && infernal)
        return infernal;

    return nullptr;
}

}
