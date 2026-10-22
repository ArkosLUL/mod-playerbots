#include "ToCDefinitions.h"

#include "ToCEncounterGate.h"
#include "ToCMultipliers_Opener.h"

namespace Family = RaidEncounterRules::Family;

namespace
{
void DefineOpener(EncounterBuilder& e)
{
    e.Multiplier<ToCOpenerHoldMultiplier>(Family::Spell | Family::Attack);
}
}  // namespace

EncounterDefinition const& ToCOpenerDefinition()
{
    static EncounterDefinition const definition(static_cast<uint32>(ToCEncounter::None), ToCRaidWideGate, nullptr,
                                                &DefineOpener);
    return definition;
}
