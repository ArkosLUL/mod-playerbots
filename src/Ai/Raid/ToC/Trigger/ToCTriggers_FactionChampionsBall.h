#ifndef PLAYERBOTS_RAID_TOCTRIGGERS_FACTIONCHAMPIONSBALL_H
#define PLAYERBOTS_RAID_TOCTRIGGERS_FACTIONCHAMPIONSBALL_H

#include <vector>

#include "NamedObjectContext.h"
#include "Trigger.h"

class FactionChampionsFrontlineReturnTrigger : public Trigger
{
public:
    FactionChampionsFrontlineReturnTrigger(PlayerbotAI* botAI) : Trigger(botAI, "faction champions frontline return") {}
    bool IsActive() override;
};

class FactionChampionsFrontlineAttackTrigger : public Trigger
{
public:
    FactionChampionsFrontlineAttackTrigger(PlayerbotAI* botAI) : Trigger(botAI, "faction champions frontline attack") {}
    bool IsActive() override;
};

class FactionChampionsBallStandTrigger : public Trigger
{
public:
    FactionChampionsBallStandTrigger(PlayerbotAI* botAI) : Trigger(botAI, "faction champions ball stand") {}
    bool IsActive() override;
};

class FactionChampionsChasedTrigger : public Trigger
{
public:
    FactionChampionsChasedTrigger(PlayerbotAI* botAI) : Trigger(botAI, "faction champions chased") {}
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
        creators["faction champions frontline return"] =
            &ToCFactionChampionsBallTriggerContext::faction_champions_pack_tank_return;
        creators["faction champions frontline attack"] =
            &ToCFactionChampionsBallTriggerContext::faction_champions_pack_tank_attack;
        creators["faction champions ball stand"] = &ToCFactionChampionsBallTriggerContext::faction_champions_ball_stand;
        creators["faction champions opener"] = &ToCFactionChampionsBallTriggerContext::faction_champions_opener;
        creators["faction champions chased"] = &ToCFactionChampionsBallTriggerContext::faction_champions_chased;
    }

private:
    static Trigger* faction_champions_pack_tank_return(PlayerbotAI* botAI) {
        return new FactionChampionsFrontlineReturnTrigger(botAI);
    }

    static Trigger* faction_champions_pack_tank_attack(PlayerbotAI* botAI) {
        return new FactionChampionsFrontlineAttackTrigger(botAI);
    }

    static Trigger* faction_champions_ball_stand(PlayerbotAI* botAI) {
        return new FactionChampionsBallStandTrigger(botAI);
    }

    static Trigger* faction_champions_opener(PlayerbotAI* botAI) { return new FactionChampionsOpenerTrigger(botAI); }

    static Trigger* faction_champions_chased(PlayerbotAI* botAI) { return new FactionChampionsChasedTrigger(botAI); }
};

void AddToCFactionChampionsBallTriggerNodes(std::vector<TriggerNode*>& triggers);

#endif
