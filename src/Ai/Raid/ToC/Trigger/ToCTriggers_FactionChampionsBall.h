#ifndef PLAYERBOTS_RAID_TOCTRIGGERS_FACTIONCHAMPIONSBALL_H
#define PLAYERBOTS_RAID_TOCTRIGGERS_FACTIONCHAMPIONSBALL_H

#include <vector>

#include "NamedObjectContext.h"
#include "Trigger.h"

class FactionChampionsPackTankReturnTrigger : public Trigger
{
public:
    FactionChampionsPackTankReturnTrigger(PlayerbotAI* botAI) : Trigger(botAI, "faction champions pack tank return") {}
    bool IsActive() override;
};

class FactionChampionsPackTankAttackTrigger : public Trigger
{
public:
    FactionChampionsPackTankAttackTrigger(PlayerbotAI* botAI) : Trigger(botAI, "faction champions pack tank attack") {}
    bool IsActive() override;
};

class FactionChampionsBallStandTrigger : public Trigger
{
public:
    FactionChampionsBallStandTrigger(PlayerbotAI* botAI) : Trigger(botAI, "faction champions ball stand") {}
    bool IsActive() override;
};

class FactionChampionsOpenerTrigger : public Trigger
{
public:
    FactionChampionsOpenerTrigger(PlayerbotAI* botAI) : Trigger(botAI, "faction champions opener") {}
    bool IsActive() override;
};

class ToCFactionChampionsBallTriggerContext : public NamedObjectContext<Trigger>
{
public:
    ToCFactionChampionsBallTriggerContext() : NamedObjectContext<Trigger>()
    {
        creators["faction champions pack tank return"] =
            &ToCFactionChampionsBallTriggerContext::faction_champions_pack_tank_return;
        creators["faction champions pack tank attack"] =
            &ToCFactionChampionsBallTriggerContext::faction_champions_pack_tank_attack;
        creators["faction champions ball stand"] = &ToCFactionChampionsBallTriggerContext::faction_champions_ball_stand;
        creators["faction champions opener"] = &ToCFactionChampionsBallTriggerContext::faction_champions_opener;
    }

private:
    static Trigger* faction_champions_pack_tank_return(PlayerbotAI* botAI) {
        return new FactionChampionsPackTankReturnTrigger(botAI);
    }

    static Trigger* faction_champions_pack_tank_attack(PlayerbotAI* botAI) {
        return new FactionChampionsPackTankAttackTrigger(botAI);
    }

    static Trigger* faction_champions_ball_stand(PlayerbotAI* botAI) {
        return new FactionChampionsBallStandTrigger(botAI);
    }

    static Trigger* faction_champions_opener(PlayerbotAI* botAI) { return new FactionChampionsOpenerTrigger(botAI); }
};

void AddToCFactionChampionsBallTriggerNodes(std::vector<TriggerNode*>& triggers);

#endif
