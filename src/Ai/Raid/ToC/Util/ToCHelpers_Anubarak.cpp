#include "ToCHelpers_Anubarak.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "Creature.h"
#include "Unit.h"

using namespace EncounterHelpers;

namespace TrialOfTheCrusaderHelpers
{

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

}
