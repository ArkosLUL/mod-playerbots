#include "ToCHelpers_FactionChampions.h"
#include "Playerbots.h"
#include "Unit.h"

#include <vector>

namespace TrialOfTheCrusaderHelpers
{

bool IsFactionChampionHealer(uint32 entry)
{
    switch (entry)
    {
        case static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_DRUID_RESTORATION):
        case static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_SHAMAN_RESTORATION):
        case static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_PALADIN_HOLY):
        case static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_PRIEST_DISCIPLINE):
        case static_cast<uint32>(ToCFactionChampions::NPC_HORDE_DRUID_RESTORATION):
        case static_cast<uint32>(ToCFactionChampions::NPC_HORDE_SHAMAN_RESTORATION):
        case static_cast<uint32>(ToCFactionChampions::NPC_HORDE_PALADIN_HOLY):
        case static_cast<uint32>(ToCFactionChampions::NPC_HORDE_PRIEST_DISCIPLINE):
            return true;
        default:
            return false;
    }
}

bool IsFactionChampion(uint32 entry)
{
    if (IsFactionChampionHealer(entry))
        return true;

    switch (entry)
    {
        case static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_DEATH_KNIGHT):
        case static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_DRUID_BALANCE):
        case static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_HUNTER):
        case static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_MAGE):
        case static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_PALADIN_RETRIBUTION):
        case static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_PRIEST_SHADOW):
        case static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_ROGUE):
        case static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_SHAMAN_ENHANCEMENT):
        case static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_WARLOCK):
        case static_cast<uint32>(ToCFactionChampions::NPC_ALLIANCE_WARRIOR):
        case static_cast<uint32>(ToCFactionChampions::NPC_HORDE_DEATH_KNIGHT):
        case static_cast<uint32>(ToCFactionChampions::NPC_HORDE_DRUID_BALANCE):
        case static_cast<uint32>(ToCFactionChampions::NPC_HORDE_HUNTER):
        case static_cast<uint32>(ToCFactionChampions::NPC_HORDE_MAGE):
        case static_cast<uint32>(ToCFactionChampions::NPC_HORDE_PALADIN_RETRIBUTION):
        case static_cast<uint32>(ToCFactionChampions::NPC_HORDE_PRIEST_SHADOW):
        case static_cast<uint32>(ToCFactionChampions::NPC_HORDE_ROGUE):
        case static_cast<uint32>(ToCFactionChampions::NPC_HORDE_SHAMAN_ENHANCEMENT):
        case static_cast<uint32>(ToCFactionChampions::NPC_HORDE_WARLOCK):
        case static_cast<uint32>(ToCFactionChampions::NPC_HORDE_WARRIOR):
            return true;
        default:
            return false;
    }
}

namespace
{
// Collect every alive Faction Champion the bot can currently see. Walks the same target source as
// GetFirstAliveUnitByEntry so pets and summons (which are not in the champion entry set) are skipped.
void CollectAliveFactionChampions(PlayerbotAI* botAI, std::vector<Unit*>& champions)
{
    auto const& npcs = botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets no los")->Get();
    for (auto const& npcGuid : npcs)
    {
        Unit* unit = botAI->GetUnit(npcGuid);
        if (unit && unit->IsAlive() && IsFactionChampion(unit->GetEntry()))
            champions.push_back(unit);
    }
}
}

bool FactionChampionsEncounterActive(PlayerbotAI* botAI)
{
    // Existence check only: early-return on the first champion without collecting the full list, since
    // this runs on the AoE-suppression multiplier's hot path (every action, every bot, every tick) for
    // all four ToC encounters, not just this one.
    auto const& npcs = botAI->GetAiObjectContext()->GetValue<GuidVector>("possible targets no los")->Get();
    for (auto const& npcGuid : npcs)
    {
        Unit* unit = botAI->GetUnit(npcGuid);
        if (unit && unit->IsAlive() && IsFactionChampion(unit->GetEntry()))
            return true;
    }

    return false;
}

Unit* GetPriorityFactionChampion(PlayerbotAI* botAI)
{
    std::vector<Unit*> champions;
    CollectAliveFactionChampions(botAI, champions);

    Unit* lowestHealer = nullptr;
    Unit* lowestAny = nullptr;
    for (Unit* champion : champions)
    {
        if (!lowestAny || champion->GetHealth() < lowestAny->GetHealth())
            lowestAny = champion;

        if (IsFactionChampionHealer(champion->GetEntry()) &&
            (!lowestHealer || champion->GetHealth() < lowestHealer->GetHealth()))
            lowestHealer = champion;
    }

    // Burn a healer down first; once they are all dead, collapse onto the lowest-health champion.
    return lowestHealer ? lowestHealer : lowestAny;
}

Unit* GetCcFactionChampionHealer(PlayerbotAI* botAI, Unit* killTarget)
{
    std::vector<Unit*> champions;
    CollectAliveFactionChampions(botAI, champions);

    Unit* highestHealer = nullptr;
    for (Unit* champion : champions)
    {
        if (champion == killTarget || !IsFactionChampionHealer(champion->GetEntry()))
            continue;

        // Lock the off-target healer that will take longest to die, so CC buys the most time.
        if (!highestHealer || champion->GetHealth() > highestHealer->GetHealth())
            highestHealer = champion;
    }

    return highestHealer;
}

}
