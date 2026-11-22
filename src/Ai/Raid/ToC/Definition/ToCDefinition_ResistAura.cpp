#include "ToCDefinitions.h"

#include "PlayerbotAI.h"
#include "RaidResistAura.h"
#include "Strategy.h"
#include "ToCEncounterGate.h"
#include "ToCHelpers_ResistAura.h"

using namespace TrialOfTheCrusaderHelpers;

namespace Role = RaidEncounterRules::Role;

namespace
{
void DefineResistAura(EncounterBuilder& e)
{
    e.Node(
        "toc resistance aura",
        [](PlayerbotAI* botAI) -> Trigger*
        { return new RaidResistAuraTrigger(botAI, "toc resistance aura", TOC_RESIST_AURA); },
        "toc raise resistance aura",
        [](PlayerbotAI* botAI) -> Action*
        { return new RaidRaiseResistAuraAction(botAI, "toc raise resistance aura", TOC_RESIST_AURA); },
        ACTION_RAID + 1);

    e.Block("toc resistance aura hold", Role::Any, IsToCResistPaladinHolding, 0,
            {"devotion aura", "retribution aura", "concentration aura", "crusader aura", "sanctity aura",
             "fire resistance aura", "frost resistance aura", "shadow resistance aura"});

    e.Block("toc nature aspect hold", Role::Any, IsToCResistHunterHolding, 0,
            {"aspect of the viper", "aspect of the dragonhawk", "aspect of the hawk", "aspect of the monkey",
             "aspect of the cheetah", "aspect of the pack"});
}
}

EncounterDefinition const& ToCResistAuraDefinition()
{
    static EncounterDefinition const definition(static_cast<uint32>(ToCEncounter::None), ToCRaidWideGate, nullptr,
                                                &DefineResistAura);
    return definition;
}
