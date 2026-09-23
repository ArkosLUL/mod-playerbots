#include "ToCHelpers_TwinValkyr.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "Unit.h"

using namespace EncounterHelpers;

namespace TrialOfTheCrusaderHelpers
{

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

Unit* GetTwinCastingPact(PlayerbotAI* botAI)
{
    // Fjola casts Light Pact and Eydis casts Dark Pact, but check both ids on each twin so the detection
    // survives either twin being the surviving/controlling one.
    for (uint32 const entry : { static_cast<uint32>(ToCNpcs::NPC_FJOLA_LIGHTBANE),
                                static_cast<uint32>(ToCNpcs::NPC_EYDIS_DARKBANE) })
    {
        Unit* twin = GetFirstAliveUnitByEntry(botAI, entry);
        if (twin && (twin->FindCurrentSpellBySpellId(static_cast<uint32>(ToCSpells::SPELL_LIGHT_TWIN_PACT)) ||
                     twin->FindCurrentSpellBySpellId(static_cast<uint32>(ToCSpells::SPELL_DARK_TWIN_PACT))))
        {
            return twin;
        }
    }

    return nullptr;
}

}
