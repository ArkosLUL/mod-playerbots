#ifndef PLAYERBOTS_RAID_TOCACTIONS_FACTIONCHAMPIONSBALL_H
#define PLAYERBOTS_RAID_TOCACTIONS_FACTIONCHAMPIONSBALL_H

#include "Action.h"
#include "NamedObjectContext.h"
#include "ToCActions_NorthrendBeasts.h"
#include "ToCActions_Shared.h"

// Walks the pack tank back to where the champions landed
class FactionChampionsPackTankReturnAction : public NorthrendBeastsWalkAction
{
public:
    FactionChampionsPackTankReturnAction(PlayerbotAI* botAI)
        : NorthrendBeastsWalkAction(botAI, "faction champions pack tank return") {}
    bool Execute(Event event) override;
};

class FactionChampionsPackTankAttackAction : public ToCDirectAttackAction
{
public:
    FactionChampionsPackTankAttackAction(PlayerbotAI* botAI)
        : ToCDirectAttackAction(botAI, "faction champions pack tank attack") {}
    bool Execute(Event event) override;
};

class FactionChampionsBallStandAction : public NorthrendBeastsWalkAction
{
public:
    FactionChampionsBallStandAction(PlayerbotAI* botAI)
        : NorthrendBeastsWalkAction(botAI, "faction champions ball stand") {}
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
        creators["faction champions pack tank return"] =
            &ToCFactionChampionsBallActionContext::faction_champions_pack_tank_return;
        creators["faction champions pack tank attack"] =
            &ToCFactionChampionsBallActionContext::faction_champions_pack_tank_attack;
        creators["faction champions ball stand"] = &ToCFactionChampionsBallActionContext::faction_champions_ball_stand;
        creators["faction champions opener"] = &ToCFactionChampionsBallActionContext::faction_champions_opener;
    }

private:
    static Action* faction_champions_pack_tank_return(PlayerbotAI* botAI) {
        return new FactionChampionsPackTankReturnAction(botAI);
    }

    static Action* faction_champions_pack_tank_attack(PlayerbotAI* botAI) {
        return new FactionChampionsPackTankAttackAction(botAI);
    }

    static Action* faction_champions_ball_stand(PlayerbotAI* botAI) {
        return new FactionChampionsBallStandAction(botAI);
    }

    static Action* faction_champions_opener(PlayerbotAI* botAI) { return new FactionChampionsOpenerAction(botAI); }
};

#endif
