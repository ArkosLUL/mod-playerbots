#ifndef PLAYERBOTS_RAID_TOCTRIGGERS_FACTIONCHAMPIONS_H
#define PLAYERBOTS_RAID_TOCTRIGGERS_FACTIONCHAMPIONS_H

#include <vector>

#include "NamedObjectContext.h"
#include "Trigger.h"

class FactionChampionsShouldFocusTrigger : public Trigger
{
public:
    FactionChampionsShouldFocusTrigger(
        PlayerbotAI* botAI) : Trigger(botAI, "faction champions should focus") {}
    bool IsActive() override;
};

class ToCFactionChampionsTriggerContext : public NamedObjectContext<Trigger>
{
public:
    ToCFactionChampionsTriggerContext()
    {
        creators["faction champions should focus"] =
            &ToCFactionChampionsTriggerContext::faction_champions_should_focus;
    }

private:
    static Trigger* faction_champions_should_focus(PlayerbotAI* botAI) {
        return new FactionChampionsShouldFocusTrigger(botAI);
    }
};

void AddToCFactionChampionsTriggerNodes(std::vector<TriggerNode*>& triggers);

#endif
