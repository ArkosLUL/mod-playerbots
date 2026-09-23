#include "ToCHelpers_Anubarak.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "Creature.h"
#include "SpellMgr.h"
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
    return anubarak && anubarak->HasAura(SPELL_SUBMERGE_ANUB);
}

bool AnubarakLeechingSwarmActive(PlayerbotAI* botAI)
{
    Player* bot = botAI->GetBot();
    if (bot->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_LEECHING_SWARM, bot)))
        return true;

    Unit* anubarak = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_ANUBARAK));
    return anubarak && anubarak->GetHealthPct() < 30.0f;
}

bool IsFrostSphereFlying(Unit* sphere)
{
    return sphere && sphere->IsAlive() && sphere->HasAura(SPELL_FROST_SPHERE) &&
           !sphere->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE);
}

bool IsPermafrostPatch(Unit* sphere)
{
    return sphere && sphere->IsAlive() && !sphere->HasAura(SPELL_FROST_SPHERE);
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
        if (!IsPermafrostPatch(sphere))
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
