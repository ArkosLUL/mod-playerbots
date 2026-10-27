#include "ToCDefinitions.h"

#include <functional>

#include "RaidOpener.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_Opener.h"

namespace Family = RaidEncounterRules::Family;

namespace
{
void DefineOpener(EncounterBuilder& e)
{
    e.Multiplier<RaidOpenerHoldMultiplier>(Family::Spell | Family::Attack, "toc opener hold",
                                           std::cref(TrialOfTheCrusaderHelpers::TOC_OPENER));
}
}  // namespace

EncounterDefinition const& ToCOpenerDefinition()
{
    static EncounterDefinition const definition(static_cast<uint32>(ToCEncounter::None), ToCRaidWideGate, nullptr,
                                                &DefineOpener);
    return definition;
}
