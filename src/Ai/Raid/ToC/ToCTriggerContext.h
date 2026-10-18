#ifndef PLAYERBOTS_RAID_TOCTRIGGERCONTEXT_H
#define PLAYERBOTS_RAID_TOCTRIGGERCONTEXT_H

#include "Log.h"
#include "NamedObjectContext.h"
#include "ToCEncounterGate.h"
#include "ToCRaidTriggers.h"

// Boss stems register their triggers in their own contexts, this one only merges them.
class RaidTrialOfTheCrusaderTriggerContext : public NamedObjectContext<Trigger>
{
public:
    RaidTrialOfTheCrusaderTriggerContext() : NamedObjectContext<Trigger>()
    {
        Absorb(ToCGormokTriggerContext());
        Absorb(ToCJormungarsTriggerContext());
        Absorb(ToCIcehowlTriggerContext());
        Absorb(ToCNorthrendBeastsTriggerContext());
        Absorb(ToCJaraxxusTriggerContext());
        Absorb(ToCAnubarakTriggerContext());
        Absorb(ToCFactionChampionsTriggerContext());
        Absorb(ToCFactionChampionsDefenceTriggerContext());
        Absorb(ToCFactionChampionsBallTriggerContext());
        Absorb(ToCTwinValkyrTriggerContext());
        Absorb(ToCStagingTriggerContext());
        Absorb(ToCResistAuraTriggerContext());
    }

private:
    // In the raid, a trigger whose name leads with an encounter only runs during that encounter's stage.
    void Absorb(NamedObjectContext<Trigger> const& stem)
    {
        for (auto const& [name, creator] : stem.creators)
        {
            ObjectCreator built = creator;
            ToCEncounter encounter = ToCEncounter::None;
            if (ToCEncounterOfTrigger(name, encounter))
            {
                ObjectCreator inner = creator;
                built = [inner, encounter](PlayerbotAI* botAI) -> Trigger*
                { return new ToCGatedTrigger(botAI, inner(botAI), encounter); };
            }

            if (!creators.emplace(name, built).second)
                LOG_ERROR("playerbots", "ToC trigger context: duplicate creator '{}'", name);
        }
    }
};

#endif
