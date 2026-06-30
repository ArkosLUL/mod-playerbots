#include "ToCHelpers.h"
#include "Playerbots.h"
#include "RaidBossHelpers.h"
#include "Creature.h"
#include "Unit.h"

namespace TrialOfTheCrusaderHelpers
{

const Position ARENA_CENTER = { 563.672974f, 139.571f, 393.837006f };

const Position ANUBARAK_PIT_CENTER = { 722.65f, 135.41f, 142.16f };

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

// Anub'arak

bool AnubarakSubmerged(PlayerbotAI* botAI)
{
    // A Pursuing Spike only exists during submerge and is despawned on emerge, so it is a clean
    // phase-2 marker. (Swarm Scarabs are deliberately NOT used here: they are despawned only when
    // the boss dies, so they linger into the next surface phase and would falsely report submerge.)
    if (GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_PURSUING_SPIKE)))
        return true;

    // Covers the brief window at the start of submerge before the first spike spawns. The boss is
    // unselectable while submerged, so it may not resolve here, but the spike check above is the
    // primary signal for the rest of the phase.
    Unit* anubarak = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_ANUBARAK));
    return anubarak && anubarak->HasAura(static_cast<uint32>(ToCSpells::SPELL_SUBMERGE_ANUB));
}

bool AnubarakLeechingSwarmActive(PlayerbotAI* botAI)
{
    Unit* anubarak = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_ANUBARAK));
    if (!anubarak)
        return false;

    return anubarak->HasAura(static_cast<uint32>(ToCSpells::SPELL_LEECHING_SWARM)) ||
           anubarak->GetHealthPct() < 30.0f;
}

Unit* GetNearestPermafrost(Player* bot, float radius)
{
    if (!bot)
        return nullptr;

    std::list<Creature*> spheres;
    bot->GetCreatureListWithEntryInGrid(spheres, static_cast<uint32>(ToCNpcs::NPC_FROST_SPHERE), radius);

    Unit* nearest = nullptr;
    float nearestDist = radius;
    for (Creature* sphere : spheres)
    {
        // A grounded Permafrost patch carries the Permafrost aura; flying spheres do not despawn spikes
        if (!sphere->HasAura(static_cast<uint32>(ToCSpells::SPELL_PERMAFROST)))
            continue;

        float const dist = bot->GetExactDist2d(sphere);
        if (!nearest || dist < nearestDist)
        {
            nearest = sphere;
            nearestDist = dist;
        }
    }

    return nearest;
}

// Faction Champions

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

// Twin Val'kyr

bool TwinValkyrEncounterActive(PlayerbotAI* botAI)
{
    return GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_FJOLA_LIGHTBANE)) ||
           GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_EYDIS_DARKBANE));
}

bool HasLightEssence(Unit* unit)
{
    return unit && unit->HasAura(static_cast<uint32>(ToCSpells::SPELL_LIGHT_ESSENCE));
}

bool HasDarkEssence(Unit* unit)
{
    return unit && unit->HasAura(static_cast<uint32>(ToCSpells::SPELL_DARK_ESSENCE));
}

bool HasAnyEssence(Unit* unit)
{
    return HasLightEssence(unit) || HasDarkEssence(unit);
}

bool TwinValkyrLightVortexActive(PlayerbotAI* botAI)
{
    Unit* fjola = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_FJOLA_LIGHTBANE));
    return fjola && fjola->FindCurrentSpellBySpellId(static_cast<uint32>(ToCSpells::SPELL_LIGHT_VORTEX));
}

bool TwinValkyrDarkVortexActive(PlayerbotAI* botAI)
{
    Unit* eydis = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_EYDIS_DARKBANE));
    return eydis && eydis->FindCurrentSpellBySpellId(static_cast<uint32>(ToCSpells::SPELL_DARK_VORTEX));
}

}
