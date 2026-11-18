#include "ToCDefinitions.h"

#include <functional>

#include "RaidOpener.h"
#include "Strategy.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_Opener.h"

using TrialOfTheCrusaderHelpers::TOC_OPENER;

namespace Family = RaidEncounterRules::Family;

namespace
{
void DefineOpener(EncounterBuilder& e)
{
    // Above the resistance aura: the swing lands on the next attack timer, not the next GCD
    e.Node(
        "toc opener swing",
        [](PlayerbotAI* botAI) -> Trigger* { return new RaidOpenerSwingTrigger(botAI, "toc opener swing", TOC_OPENER); },
        "toc opener stop swing",
        [](PlayerbotAI* botAI) -> Action*
        { return new RaidOpenerStopSwingAction(botAI, "toc opener stop swing", TOC_OPENER); },
        ACTION_RAID + 2);
    e.Node(
        "toc opener redirect",
        [](PlayerbotAI* botAI) -> Trigger*
        { return new RaidOpenerRedirectTrigger(botAI, "toc opener redirect", TOC_OPENER); },
        "toc opener redirect",
        [](PlayerbotAI* botAI) -> Action* { return new RaidOpenerRedirectAction(botAI, "toc opener redirect", TOC_OPENER); },
        ACTION_RAID + 1);

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
