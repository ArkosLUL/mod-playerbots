#ifndef PLAYERBOTS_RAID_TOCACTIONS_FACTIONCHAMPIONSBALL_H
#define PLAYERBOTS_RAID_TOCACTIONS_FACTIONCHAMPIONSBALL_H

#include "Action.h"
#include "NamedObjectContext.h"
#include "ToCActions_NorthrendBeasts.h"
#include "ToCActions_Shared.h"

// Walks the frontline bot back to the pack centre
class FactionChampionsFrontlineReturnAction : public NorthrendBeastsWalkAction
{
public:
    FactionChampionsFrontlineReturnAction(PlayerbotAI* botAI)
        : NorthrendBeastsWalkAction(botAI, "faction champions frontline return") {}
    bool Execute(Event event) override;
};

class FactionChampionsFrontlineAttackAction : public ToCDirectAttackAction
{
public:
    FactionChampionsFrontlineAttackAction(PlayerbotAI* botAI)
        : ToCDirectAttackAction(botAI, "faction champions frontline attack") {}
    bool Execute(Event event) override;
};

class FactionChampionsBallStandAction : public NorthrendBeastsWalkAction
{
public:
    FactionChampionsBallStandAction(PlayerbotAI* botAI)
        : NorthrendBeastsWalkAction(botAI, "faction champions ball stand") {}
    bool Execute(Event event) override;
};

// Boosts away from the melee champion on it, so it spends the boost chasing
class FactionChampionsChasedAction : public NorthrendBeastsWalkAction
{
public:
    FactionChampionsChasedAction(PlayerbotAI* botAI) : NorthrendBeastsWalkAction(botAI, "faction champions chased") {}
    bool Execute(Event event) override;
};

// Walks into the pack and casts the AoE CC that makes the champions burn their trinket
class FactionChampionsOpenerAction : public NorthrendBeastsWalkAction
{
public:
    FactionChampionsOpenerAction(PlayerbotAI* botAI) : NorthrendBeastsWalkAction(botAI, "faction champions opener") {}
    bool Execute(Event event) override;
};

class ToCFactionChampionsBallActionContext : public NamedObjectContext<Action>
{
public:
    ToCFactionChampionsBallActionContext()
    {
        creators["faction champions frontline return"] =
            &ToCFactionChampionsBallActionContext::faction_champions_pack_tank_return;
        creators["faction champions frontline attack"] =
            &ToCFactionChampionsBallActionContext::faction_champions_pack_tank_attack;
        creators["faction champions ball stand"] = &ToCFactionChampionsBallActionContext::faction_champions_ball_stand;
        creators["faction champions opener"] = &ToCFactionChampionsBallActionContext::faction_champions_opener;
        creators["faction champions chased"] = &ToCFactionChampionsBallActionContext::faction_champions_chased;
    }

private:
    static Action* faction_champions_pack_tank_return(PlayerbotAI* botAI) {
        return new FactionChampionsFrontlineReturnAction(botAI);
    }

    static Action* faction_champions_pack_tank_attack(PlayerbotAI* botAI) {
        return new FactionChampionsFrontlineAttackAction(botAI);
    }

    static Action* faction_champions_ball_stand(PlayerbotAI* botAI) {
        return new FactionChampionsBallStandAction(botAI);
    }

    static Action* faction_champions_opener(PlayerbotAI* botAI) { return new FactionChampionsOpenerAction(botAI); }

    static Action* faction_champions_chased(PlayerbotAI* botAI) { return new FactionChampionsChasedAction(botAI); }
};

#endif
