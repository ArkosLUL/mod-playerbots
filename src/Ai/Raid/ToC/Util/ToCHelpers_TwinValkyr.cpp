#include "ToCHelpers_TwinValkyr.h"
#include "Playerbots.h"
#include "EncounterHelpers.h"
#include "SpellMgr.h"
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
    return unit && unit->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_LIGHT_ESSENCE, unit));
}

bool HasDarkEssence(Unit* unit)
{
    return unit && unit->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_DARK_ESSENCE, unit));
}

bool HasAnyEssence(Unit* unit)
{
    return HasLightEssence(unit) || HasDarkEssence(unit);
}

bool HasLightTouch(Unit* unit)
{
    return unit && unit->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_LIGHT_TOUCH, unit));
}

bool HasDarkTouch(Unit* unit)
{
    return unit && unit->HasAura(sSpellMgr->GetSpellIdForDifficulty(SPELL_DARK_TOUCH, unit));
}

bool TwinValkyrLightVortexActive(PlayerbotAI* botAI)
{
    Unit* fjola = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_FJOLA_LIGHTBANE));
    return fjola && fjola->FindCurrentSpellBySpellId(sSpellMgr->GetSpellIdForDifficulty(SPELL_LIGHT_VORTEX, fjola));
}

bool TwinValkyrDarkVortexActive(PlayerbotAI* botAI)
{
    Unit* eydis = GetFirstAliveUnitByEntry(botAI, static_cast<uint32>(ToCNpcs::NPC_EYDIS_DARKBANE));
    return eydis && eydis->FindCurrentSpellBySpellId(sSpellMgr->GetSpellIdForDifficulty(SPELL_DARK_VORTEX, eydis));
}

Unit* GetTwinCastingPact(PlayerbotAI* botAI)
{
    // Fjola casts Light Pact and Eydis casts Dark Pact, but check both ids on each twin so the detection
    // survives either twin being the surviving/controlling one.
    uint32 const lightPact = sSpellMgr->GetSpellIdForDifficulty(SPELL_LIGHT_TWIN_PACT, botAI->GetBot());
    uint32 const darkPact = sSpellMgr->GetSpellIdForDifficulty(SPELL_DARK_TWIN_PACT, botAI->GetBot());
    for (uint32 const entry : { static_cast<uint32>(ToCNpcs::NPC_FJOLA_LIGHTBANE),
                                static_cast<uint32>(ToCNpcs::NPC_EYDIS_DARKBANE) })
    {
        Unit* twin = GetFirstAliveUnitByEntry(botAI, entry);
        if (twin && (twin->FindCurrentSpellBySpellId(lightPact) || twin->FindCurrentSpellBySpellId(darkPact)))
        {
            return twin;
        }
    }

    return nullptr;
}

}
