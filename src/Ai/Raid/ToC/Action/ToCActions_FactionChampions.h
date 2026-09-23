#ifndef PLAYERBOTS_RAID_TOCACTIONS_FACTIONCHAMPIONS_H
#define PLAYERBOTS_RAID_TOCACTIONS_FACTIONCHAMPIONS_H

#include "NamedObjectContext.h"
#include "ToCActions_Shared.h"

class FactionChampionsFocusPriorityAction : public AttackAction
{
public:
    FactionChampionsFocusPriorityAction(
        PlayerbotAI* botAI, std::string const name = "faction champions focus priority") : AttackAction(botAI, name) {};
    bool Execute(Event event) override;
};

class ToCFactionChampionsActionContext : public NamedObjectContext<Action>
{
public:
    ToCFactionChampionsActionContext()
    {
        creators["faction champions focus priority"] =
            &ToCFactionChampionsActionContext::faction_champions_focus_priority;
    }

private:
    static Action* faction_champions_focus_priority(PlayerbotAI* botAI) {
        return new FactionChampionsFocusPriorityAction(botAI);
    }
};

#endif
